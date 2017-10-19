/** \file    DnsServerAndPool.cc
 *  \brief   Implementation of class DnsServer and DnsServerPool.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2006-2009 Project iVia.
 *  Copyright 2006-2009 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "DnsServerAndPool.h"
#include <functional>
#include <set>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include "Compiler.h"
#include "FileUtil.h"
#include "NetUtil.h"
#include "Resolver.h"
#include "SocketUtil.h"
#include "TimeUtil.h"
#include "util.h"


uint16_t DnsServer::next_query_id_;


void DnsServer::OutstandingRequests::addRequest(const uint16_t query_id, const std::string &hostname,
                                                const uint64_t expiration_time)
{
    expireOldRequests();

    // If we have too many requests, we remove the oldest one:
    if (count_ >= max_count_)
        pop_front();

    push_back(OutstandingRequest(query_id, hostname, expiration_time));
    ++count_;
}


#if __GNUC__ > 3
namespace {
#endif


class QueryIdMatch: public std::unary_function<DnsServer::OutstandingRequest, bool> {
    uint16_t query_id_to_match_;
public:
    explicit QueryIdMatch(const uint16_t query_id_to_match): query_id_to_match_(query_id_to_match) { }
    bool operator()(const DnsServer::OutstandingRequest &request) const
        { return request.query_id_ == query_id_to_match_; }
};


#if __GNUC__ > 3
} // unnamed namespace
#endif


bool DnsServer::OutstandingRequests::removeRequest(const uint16_t query_id, std::string * const hostname) {
    if (unlikely(empty()))
        throw std::runtime_error("in DnsServer::OutstandingRequests::removeRequest: attempt to remove a request "
                                 "from an empty list!");

    const iterator request(std::find_if(begin(), end(), QueryIdMatch(query_id)));
    if (unlikely(request == end())) {
        hostname->clear();
        return false;
    }

    *hostname = request->hostname_;
    erase(request);

    --count_;

    return true;
}


void DnsServer::OutstandingRequests::expireOldRequests() {
    const uint64_t now(TimeUtil::GetCurrentTimeInMilliseconds());
    while (not empty() and now > front().expiration_time_) {
        pop_front();
        --count_;
    }
}


DnsServer::DnsServer(const in_addr_t &server_ip_address, const unsigned max_outstanding_request_count,
                     const unsigned request_lifetime)
    : server_ip_address_(server_ip_address), max_outstanding_request_count_(max_outstanding_request_count),
      socket_fd_(-1), request_lifetime_(request_lifetime), outstanding_requests_(max_outstanding_request_count + 10)
{
    const protoent *protocol_entry = ::getprotobyname("udp");
    if (unlikely(protocol_entry == nullptr))
        throw std::runtime_error("in Resolver::Resolver: can't get protocol entry for \"udp\"!");

    socket_fd_ = ::socket(PF_INET, SOCK_DGRAM, protocol_entry->p_proto);
    if (unlikely(socket_fd_ == -1))
        throw std::runtime_error("in Resolver::Resolver: socket(2) failed (" + std::to_string(errno) + ")!");

    // Turn off blocking because we are going to use select(2) which on Linux doesn't reliably work with
    // blocking file descriptors:
    FileUtil::SetNonblocking(socket_fd_);

    struct sockaddr_in socket_address;
    std::memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_family      = AF_INET;
    socket_address.sin_port        = htons(53);
    socket_address.sin_addr.s_addr = server_ip_address;
    if (::connect(socket_fd_, reinterpret_cast<sockaddr *>(&socket_address), sizeof(struct sockaddr_in)) != 0)
        throw std::runtime_error("in DnsServer::DnsServer: can't connect(2) to "
                                 + NetUtil::NetworkAddressToString(server_ip_address) + "!");
}


bool DnsServer::isBusy() const {
    outstanding_requests_.expireOldRequests();
    return outstanding_requests_.size() >= max_outstanding_request_count_;
}


bool DnsServer::addLookupRequest(const std::string &valid_hostname, uint16_t * const query_id, uint64_t * const deadline) {
    outstanding_requests_.expireOldRequests();
    if (unlikely(outstanding_requests_.size() >= max_outstanding_request_count_))
        return false;

    unsigned char packet[512];
    const ptrdiff_t packet_size(Resolver::GenerateRequestPacket(valid_hostname, next_query_id_, packet));
    *query_id = next_query_id_;
    const ssize_t retcode = ::write(socket_fd_, packet, packet_size);
    if (unlikely(retcode == -1))
        throw std::runtime_error("in DnsServer::addLookupRequest: write(2) failed (" + std::to_string(errno) + ")!");
    else if (unlikely(retcode != packet_size))
        throw std::runtime_error("in DnsServer::addLookupRequest: short write (expected: " + std::to_string(packet_size)
                                 + ", actual: " + std::to_string(retcode) + ")!");

    const uint64_t expire_time(TimeUtil::GetCurrentTimeInMilliseconds() + request_lifetime_);
    outstanding_requests_.addRequest(next_query_id_, valid_hostname, expire_time);
    if (deadline != nullptr)
        *deadline = expire_time;
    ++next_query_id_;

    return true;
}


bool DnsServer::processServerReply(std::vector<in_addr_t> * const resolved_ip_addresses,
                                   std::vector<std::string> * const resolved_domainnames,
                                   uint32_t * const ttl, uint16_t * const query_id)
{
    resolved_ip_addresses->clear();
    resolved_domainnames->clear();

    const TimeLimit time_limit(0);
    unsigned char packet[1000];

    if (unlikely(outstanding_requests_.empty())) {
        SocketUtil::TimedRead(socket_fd_, time_limit, packet, sizeof(packet));
        logger->warning("in DnsServer::processServerReply: can't process a reply with an empty queue!");
        return false;
    }

    const ssize_t read_retcode(SocketUtil::TimedRead(socket_fd_, time_limit, packet, sizeof(packet)));
    if (read_retcode <= 0)
        return false;

    // Attempt to decode the reply:
    uint16_t reply_id;
    bool truncated;
    std::set<in_addr_t> ip_addresses;
    std::set<std::string> domainnames;
    if (Resolver::DecodeReply(packet, read_retcode, &domainnames, &ip_addresses, ttl, &reply_id, &truncated,
                              0 /* verbosity */))
    {
        std::string original_request_hostname;
        if (likely(outstanding_requests_.removeRequest(reply_id, &original_request_hostname))) {
            resolved_ip_addresses->clear();
            for (const auto &ip_address : ip_addresses)
                resolved_ip_addresses->push_back(ip_address);

            resolved_domainnames->clear();
            resolved_domainnames->push_back(original_request_hostname);
            for (const auto &domainname : domainnames) {
                if (original_request_hostname != domainname)
                    resolved_domainnames->push_back(domainname);
            }

            *query_id = reply_id;
        }

        return not resolved_domainnames->empty();
    } else {
        std::string original_request_hostname2;
        outstanding_requests_.removeRequest(reply_id, &original_request_hostname2);
        *query_id = reply_id;

        return false;
    }
}


const in_addr_t DnsCache::BAD_ENTRY(0);


bool DnsCache::lookup(const std::string &hostname, in_addr_t * const ip_address) {
    std::unordered_map<std::string, DnsCacheEntry>::iterator entry(resolved_hostnames_cache_.find(hostname));
    if (entry != resolved_hostnames_cache_.end()) {
        const time_t now(std::time(nullptr));
        if (entry->second.expire_time_ > now) {
            *ip_address = entry->second.ip_address_;
            return true;
        }

        // Entry has expired => remove it from the cache:
        resolved_hostnames_cache_.erase(entry);
    }

    return false;
}


void DnsCache::insert(const std::string &hostname, const in_addr_t &ip_address, const uint32_t ttl) {
    // Flush the cache if it contains more than cache_flush_size_ entries:
    if (resolved_hostnames_cache_.size() >= cache_flush_size_)
        resolved_hostnames_cache_.clear();
    else { // Check to see whether we already have information about this "hostname":
        std::unordered_map<std::string, DnsCacheEntry>::iterator cache_entry(resolved_hostnames_cache_.find(hostname));
        if (unlikely(cache_entry != resolved_hostnames_cache_.end()))
            return;
    }

    // Create a new cache entry:
    const uint64_t now(std::time(nullptr));
    const DnsCache::DnsCacheEntry new_cache_entry(now + ttl, ip_address);
    resolved_hostnames_cache_.insert(std::make_pair(hostname, new_cache_entry));
}


DnsServerPool::DnsServerPool(const std::vector<in_addr_t> &dns_server_ip_addresses,
                             std::vector<int> * const server_socket_file_descriptors,
                             const unsigned request_lifetime, const unsigned max_queue_length_per_server,
                             const unsigned max_cache_size)
    : cache_(max_cache_size), max_queue_length_per_server_(max_queue_length_per_server)
{
    if (unlikely(dns_server_ip_addresses.size() < 2))
        throw std::runtime_error("in DnsServerPool::DnsServerPool: can't create a \"pool\" with less than 2 server IP addresses!");

    server_socket_file_descriptors->clear();
    server_socket_file_descriptors->reserve(dns_server_ip_addresses.size());
    for (const auto &ip_address : dns_server_ip_addresses) {
        DnsServer * const new_dns_server(new DnsServer(ip_address, max_queue_length_per_server, request_lifetime));
        servers_.push_back(new_dns_server);
        server_socket_file_descriptors->push_back(new_dns_server->getFileDescriptor());
    }
}


DnsServerPool::~DnsServerPool() {
    for (auto &server : servers_)
        delete server;
}


bool DnsServerPool::isBusy() const {
    // We're only busy if all of our servers are busy:
    for (const auto &server : servers_) {
        if (not server->isBusy())
            return false;
    }

    return true; // We didn't find a single non-busy server!
}


bool DnsServerPool::addLookupRequest(const std::string &valid_hostname, in_addr_t * const resolved_ip_address) {
    // First see whether the "hostname" is already an IP address:
    if (NetUtil::StringToNetworkAddress(valid_hostname, resolved_ip_address))
        return true;

    // Now check our IP address cache:
    if (cache_.lookup(valid_hostname, resolved_ip_address))
        return true;

    // Find the least loaded server:
    DnsServer *least_loaded_server(nullptr);
    unsigned max_queue_length_so_far(max_queue_length_per_server_);
    for (std::vector<DnsServer *>::iterator server(servers_.begin()); server != servers_.end(); ++server) {
        const unsigned queue_length((*server)->getQueueLength());
        if (queue_length < max_queue_length_so_far)
            least_loaded_server = *server;
    }

    if (least_loaded_server != nullptr) {
        uint16_t query_id;
        uint64_t deadline;
        least_loaded_server->addLookupRequest(valid_hostname, &query_id, &deadline);
    }

    return false;
}


bool DnsServerPool::processServerReply(const int socket_fd, in_addr_t * const resolved_ip_address, std::string * const hostname) {
    for (auto &server : servers_) {
        if (server->getFileDescriptor() == socket_fd) {
            std::vector<in_addr_t> resolved_ip_addresses;
            std::vector<std::string> resolved_domainnames;
            uint32_t ttl;
            uint16_t reply_id;
            if (not server->processServerReply(&resolved_ip_addresses, &resolved_domainnames, &ttl, &reply_id))
                return false;

            if (resolved_ip_addresses.empty()) {
                cache_.insertUnresolvableEntry(resolved_domainnames.front());
                *resolved_ip_address = DnsCache::BAD_ENTRY;
            }
            else {
                *resolved_ip_address = *resolved_ip_addresses.begin();
                for (std::vector<std::string>::const_iterator domainname(resolved_domainnames.begin());
                     domainname!= resolved_domainnames.end(); ++domainname)
                    cache_.insert(*domainname, *resolved_ip_address, ttl);
            }

            *hostname = resolved_domainnames.front();

            return true;
        }

    }

    throw std::runtime_error("in DnsServerPool::processServerReply: received reply for an unknown socket file descriptor "
                             + std::to_string(socket_fd) + "!");
}


unsigned DnsServerPool::getQueueLength() const {
    unsigned queue_length(0);
    for (const auto &server : servers_)
        queue_length += server->getQueueLength();

    return queue_length;
}


double DnsServerPool::getAverageQueueLength() const {
    double sum(0.0);
    for (const auto &server : servers_)
        sum += server->getQueueLength();

    return sum / static_cast<double>(servers_.size());
}
