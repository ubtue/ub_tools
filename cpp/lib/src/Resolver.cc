/** \file    Resolver.cc
 *  \brief   Implementation of class Resolver.  Based on RFC1035.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2005-2008 Project iVia.
 *  Copyright 2005-2008 The Regents of The University of California.
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

#include "Resolver.h"
#include <fstream>
#include <map>
#include <stdexcept>
#include <cerrno>
#include <ctime>
#include <inttypes.h>
#include <arpa/nameser_compat.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include "Compiler.h"
#include "File.h"
#include "FileDescriptor.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "NetUtil.h"
#include "SocketUtil.h"
#include "StringUtil.h"
#include "TimerUtil.h"
#include "util.h"


uint16_t Resolver::next_request_id_;


namespace {


std::mutex resolver_mutex;


} // unnamed namespace


bool Resolver::Cache::lookup(const std::string &hostname, std::set<in_addr_t> * const ip_addresses) {
    std::unordered_map<std::string, CacheEntry>::iterator entry(resolved_hostnames_cache_.find(hostname));
    if (entry != resolved_hostnames_cache_.end()) {
        const time_t now(std::time(nullptr));
        if (entry->second.expire_time_ > now) {
            *ip_addresses = entry->second.ip_addresses_;
            return true;
        }

        // Entry has expired => remove it from the cache:
        resolved_hostnames_cache_.erase(entry);
    }

    return false;
}


void Resolver::Cache::insert(const std::string &hostname, const std::set<in_addr_t> &ip_addresses, const uint32_t ttl) {
    // Flush the cache if it contains more than 100,000 entries:
    if (resolved_hostnames_cache_.size() > 100000)
        resolved_hostnames_cache_.clear();
    else {
        // Check to see whether we already have information about this "hostname":
        std::unordered_map<std::string, CacheEntry>::iterator cache_entry(resolved_hostnames_cache_.find(hostname));
        if (unlikely(cache_entry != resolved_hostnames_cache_.end())) {
            for (std::set<in_addr_t>::const_iterator ip_address(ip_addresses.begin());
                 ip_address != ip_addresses.end(); ++ip_address)
                cache_entry->second.ip_addresses_.insert(*ip_address);
            return;
        }
    }

    // Create a new cache entry:
    const time_t now(std::time(nullptr));
    const Cache::CacheEntry new_cache_entry(now + ttl, ip_addresses);
    resolved_hostnames_cache_.insert(std::make_pair(hostname, new_cache_entry));
}


Resolver::Resolver(const std::list<std::string> &dns_servers, const unsigned verbosity)
    : verbosity_(verbosity), udp_fd_(-1), reply_packet_(nullptr), reply_packet_size_(0)
{
    // This variable keeps track of whether we have read in a
    // list of DNS servers from one of the three sources yet:
    bool dns_servers_processed = false;

    // Get the resolver IP addresses from the "dns_server" parameter:
    if (not dns_servers.empty()) {
        for (std::list<std::string>::const_iterator dns_server(dns_servers.begin());
             dns_server != dns_servers.end(); ++dns_server)
            {
                in_addr_t ip_address;
                if (unlikely(not NetUtil::StringToNetworkAddress(*dns_server, &ip_address)))
                    throw std::runtime_error("in Resolver::Resolver: \"" + *dns_server
                                    + "\" is not a valid IP address (1)!");
                dns_server_ip_addresses_and_busy_counts_.insert(std::make_pair(ip_address, 0));
            }

        dns_servers_processed = true;
    }

    // If a Resolver.conf file exists, read it:
    if (FileUtil::Exists(ETC_DIR "/Resolver.conf")) {
        IniFile ini_file(ETC_DIR "/Resolver.conf");

        // Read logging instructions:
        verbosity_ = ini_file.getUnsigned("Logging", "verbosity", 0);
        if (verbosity_ > 5)
            verbosity_ = 5;

        // Read DNS servers:
        if (not dns_servers_processed and ini_file.sectionIsDefined("DNS Servers")) {
            // Add each entry in the [DNS Servers] section of Resolver.conf:
            const std::list<std::string> names(ini_file.getSectionEntryNames("DNS Servers"));
            for (std::list<std::string>::const_iterator name(names.begin()); name != names.end(); ++name) {
                const std::string ip_address_str(ini_file.getString("DNS Servers", *name));
                in_addr_t ip_address;
                if (unlikely(not NetUtil::StringToNetworkAddress(ip_address_str, &ip_address)))
                    throw std::runtime_error("Resolver.conf: \"" + ip_address_str + "\""
                                    " is not a valid IP address (2)!");
                dns_server_ip_addresses_and_busy_counts_.insert(std::make_pair(ip_address, 0));
            }

            dns_servers_processed = true;
        }
    }

    // As a last resort, attempt to get the resolver IP addresses from /etc/resolv.conf:
    if (not dns_servers_processed) {
        std::vector<in_addr_t> server_ip_addresses;
        Resolver::GetServersFromResolvDotConf(&server_ip_addresses);
        for (std::vector<in_addr_t>::const_iterator server_address(server_ip_addresses.begin());
             server_address != server_ip_addresses.end(); ++server_address)
            dns_server_ip_addresses_and_busy_counts_.insert(std::make_pair(*server_address, 0));

        dns_servers_processed = true;
    }

    // Ensure we have at least one DNS Server:
    if (dns_server_ip_addresses_and_busy_counts_.empty())
        throw std::runtime_error("in Resolver::Resolver: no DNS Servers found");

    initUdpSocket();
}


Resolver::Resolver(const std::string &dns_server, const unsigned verbosity)
    : verbosity_(verbosity)
{
    in_addr_t ip_address;
    if (unlikely(not NetUtil::StringToNetworkAddress(dns_server, &ip_address)))
        throw std::runtime_error("in Resolver::Resolver: \"" + dns_server + "\" is not a valid IP address (3)!");

    dns_server_ip_addresses_and_busy_counts_.insert(std::make_pair(ip_address, 0));

    initUdpSocket();
}


Resolver::Resolver(const in_addr_t dns_server, const unsigned verbosity)
    : verbosity_(verbosity)
{
    dns_server_ip_addresses_and_busy_counts_.insert(std::make_pair(dns_server, 0));

    initUdpSocket();
}


Resolver::~Resolver() {
    if (udp_fd_ != -1)
        ::close(udp_fd_);
    delete [] reply_packet_;
}


void Resolver::initUdpSocket() {
    udp_fd_ = ::socket(PF_INET, SOCK_DGRAM, 0);
    if (unlikely(udp_fd_ == -1))
        throw std::runtime_error("in Resolver::Resolver: socket(2) failed (" + std::to_string(errno)+ ")!");

    // Turn off blocking because we are going to use select(2) which on Linux doesn't reliably work with
    // blocking file descriptors:
    FileUtil::SetNonblocking(udp_fd_);

    // Allocate a buffer to hold UDP DNS server replies and make sure that it is 4-byte aligned:
    const size_t MAX_UDP_REPLY_PACKET_SIZE(512);
    reply_packet_ = reinterpret_cast<byte *>(new uint32_t[(MAX_UDP_REPLY_PACKET_SIZE + sizeof(uint32_t) - 1)
                                                          / sizeof(uint32_t)]);
    reply_packet_size_ = MAX_UDP_REPLY_PACKET_SIZE;
}


namespace {


// IsValidHostname -- sloppy implementation that only tests whether the overall hostname is too long.
//
inline bool IsValidHostname(const std::string &hostname) {
    return hostname.length() <= 255;
}


} // unnamed namespace


void Resolver::submitRequest(const std::string &mixed_case_hostname) {
    // If "name" is a dotted quad, we can just store it for future reference by poll():
    in_addr_t network_address;
    if (NetUtil::StringToNetworkAddress(mixed_case_hostname, &network_address)) {
        std::set<in_addr_t> network_addresses;
        network_addresses.insert(network_address);
        resolved_addresses_.push_back(Result(RESOLVED, mixed_case_hostname, network_addresses));
        return;
    }

    const std::string hostname(StringUtil::ToLower(mixed_case_hostname));
    if (unlikely(not IsValidHostname(hostname)))
        throw std::runtime_error("in Resolver::submitRequest: \"" + hostname + "\" is not a valid hostname!");

    // Get the next request ID in a threadsafe manner:
    uint16_t request_id;
    {
        std::lock_guard<std::mutex> mutex_locker(resolver_mutex);
        request_id = Resolver::next_request_id_;
        ++Resolver::next_request_id_;
    }

    // Create the request packet:
    unsigned char packet[512];
    const ptrdiff_t packet_size(Resolver::GenerateRequestPacket(hostname, request_id, packet));

    // Find the least loaded server:
    std::unordered_map<in_addr_t, unsigned>::const_iterator server_and_busy_count(
        dns_server_ip_addresses_and_busy_counts_.begin());
    in_addr_t least_loaded_server(server_and_busy_count->first);
    unsigned lowest_busy_count_so_far(server_and_busy_count->second);
    for (/* Empty! */; server_and_busy_count != dns_server_ip_addresses_and_busy_counts_.end();
                     ++server_and_busy_count)
    {
        if (server_and_busy_count->second < lowest_busy_count_so_far) {
            lowest_busy_count_so_far = server_and_busy_count->second;
            least_loaded_server = server_and_busy_count->first;
        }
    }

    // Now submit the request...
    sendUdpRequest(least_loaded_server, packet, static_cast<unsigned>(packet_size));

    // ...and update the busy count of the server we used.
    std::unordered_map<in_addr_t, unsigned>::iterator least_loaded_server_iter(
        dns_server_ip_addresses_and_busy_counts_.find(least_loaded_server));
    ++least_loaded_server_iter->second;
}


void Resolver::poll(std::list<Result> * const results) {
    *results = resolved_addresses_;
    resolved_addresses_.clear();

    ssize_t read_retcode;
    unsigned char packet[1000];
    const TimeLimit time_limit(0);
    while ((read_retcode = SocketUtil::TimedRead(udp_fd_, time_limit, packet, sizeof(packet))) > 0) {
        std::set<std::string> domainnames;
        std::set<in_addr_t> ip_addresses;
        uint32_t ttl;
        uint16_t reply_id;
        bool truncated;
        if (DecodeReply(packet, read_retcode, &domainnames, &ip_addresses, &ttl, &reply_id, &truncated, verbosity_)) {
            // For now, we ignore truncated packets.  We should really send a TCP request instead:
            if (truncated)
                continue;

            for (const auto &domainname : domainnames)
                cache_.insert(domainname, ip_addresses, ttl);
        }
    }
}


namespace { // helper functions for logging


void LogIpAddresses(const std::string &domainname, const std::string &event, const std::set<in_addr_t> &ip_addresses,
                    const TimeLimit &time_limit)
{
    std::string result(domainname + " (" + StringUtil::ToString(time_limit.getRemainingTime()) + "ms): " + event
                       + ":");
    for (std::set<in_addr_t>::const_iterator ip(ip_addresses.begin()); ip != ip_addresses.end(); ++ip)
        result += " " + NetUtil::NetworkAddressToString(*ip);

    logger->info(result);
}


void LogIpAddress(const std::string &domainname, const std::string &event,
                  const in_addr_t &ip_address, const TimeLimit &time_limit)
{
    logger->info(domainname + " (" + StringUtil::ToString(time_limit.getRemainingTime()) + "ms): "
                 + event + ": " + NetUtil::NetworkAddressToString(ip_address));
}


} // unnamed namespace


bool Resolver::resolve(const std::string &mixed_case_domainname, const TimeLimit &time_limit,
                       std::set<in_addr_t> * const ip_addresses)
{
    ip_addresses->clear();
    const std::string domainname(StringUtil::ToLower(mixed_case_domainname));

    if (verbosity_ >= 3)
        logger->info(domainname + " (" + std::to_string(time_limit.getRemainingTime()) + "ms): Requested...");

    // Check to see whether "domainname" is already an IP address:
    in_addr domainname_as_address;
    if (::inet_aton(domainname.c_str(), &domainname_as_address)) {
        ip_addresses->insert(domainname_as_address.s_addr);
        if (verbosity_ >= 4)
            LogIpAddress(domainname, "Already an address", domainname_as_address.s_addr, time_limit);
        return true;
    }

    // First see if we already know the answer to our query:
    if (cache_.lookup(domainname, ip_addresses)) {
        if (verbosity_ >= 4)
            LogIpAddresses(domainname, "In cache", *ip_addresses, time_limit);
        return true;
    }

    // Decide which resolver we should use and get the next request ID in a threadsafe manner:
    uint16_t request_id;
    static size_t next_resolver_index(::getpid());
    in_addr_t selected_dns_server;
    {
        std::lock_guard<std::mutex> mutex_locker(resolver_mutex);

        // 1. Decide which resolver we should use:
        const size_t resolver_count(dns_server_ip_addresses_and_busy_counts_.size());
        next_resolver_index = (next_resolver_index + 1) % resolver_count;
        std::unordered_map<in_addr_t, unsigned>::const_iterator dns_server_ip_address_and_busy_count(
            dns_server_ip_addresses_and_busy_counts_.begin());
        for (unsigned i = 0; i < next_resolver_index; ++i)
            ++dns_server_ip_address_and_busy_count;
        selected_dns_server = dns_server_ip_address_and_busy_count->first;
        if (verbosity_ >= 5)
            LogIpAddress(domainname, "DNS Server", selected_dns_server, time_limit);

        // 2. Get the next request ID in a threadsafe manner:
        request_id = Resolver::next_request_id_;
        ++Resolver::next_request_id_;
    }

    // Create the request packet:
    unsigned char packet[512] __attribute__((aligned(sizeof(uint32_t))));
    const ptrdiff_t packet_size(Resolver::GenerateRequestPacket(domainname, request_id, packet));

    // Now submit the request...
    if (verbosity_ >= 5)
        logger->info("in resolve: about to send UPD request (ID " + std::to_string(request_id) + ").");
    sendUdpRequest(selected_dns_server, packet, static_cast<unsigned>(packet_size));

    // Now wait for the server's response:
    for (;;) {
        if (verbosity_ >= 5)
            logger->info("in resolve: remaining time " + std::to_string(time_limit.getRemainingTime()) + " ms");
        ssize_t actual_packet_size(SocketUtil::TimedRead(udp_fd_, time_limit, reply_packet_, reply_packet_size_));
process_reply_packet:
        if (actual_packet_size <= 0) {
            if (errno == ETIMEDOUT) {
                if (verbosity_ >= 4)
                    logger->info(domainname + " failed: actual_packet_size <= 0, (timed out)");
                return false;
            }
            else {
                if (verbosity_ >= 4)
                    logger->info("in resolve: resolving of \"" + domainname
                                 + "\" failed: actual_packet_size <= 0, looping!");
                continue;
            }
        }

        std::set<std::string> resolved_domainnames;
        uint32_t ttl;
        uint16_t reply_id;
        bool truncated;
        if (DecodeReply(reply_packet_, actual_packet_size, &resolved_domainnames, ip_addresses, &ttl, &reply_id,
                        &truncated, verbosity_))
        {
            if (ip_addresses->empty()) {
                if (reply_id == request_id)
                    return false;
                else
                    continue;
            }

            for (std::set<std::string>::const_iterator resolved_domainname(resolved_domainnames.begin());
                 resolved_domainname != resolved_domainnames.end(); ++resolved_domainname)
            {
                if (not truncated)
                    cache_.insert(*resolved_domainname, *ip_addresses, ttl);
            }

            // Make sure we got the reply we were waiting for:
            if (reply_id != request_id) {
                if (verbosity_ >= 5)
                    logger->info("in resolve: discarding reply with unexpected ID: " + std::to_string(reply_id)
                                 + ".");
                ip_addresses->clear();
                continue;
            }

            if (truncated) { // => We attempt to get the information using TCP instead of UDP.
                if (verbosity_ >= 4)
                    logger->info("in resolve: UDP packet was truncated.  Attempting TCP request.");
                FileDescriptor tcp_fd(sendTcpRequest(selected_dns_server, time_limit, packet,
                                                     static_cast<unsigned>(packet_size)));
                if (tcp_fd == -1) {
                    if (verbosity_ >= 4)
                        logger->info("in resolve: failed to send request " + std::to_string(errno)+ "!");
                    return false;
                }

                if (verbosity_ >= 5)
                    logger->info("in resolve: remaining time " + std::to_string(time_limit.getRemainingTime())
                                 + " ms.");
                if (unlikely(not FileUtil::SetBlocking(tcp_fd)))
                    throw std::runtime_error("in Resolver::resolve: FileUtil::SetBlocking() failed!");
                uint16_t reply_data_size;
                SocketUtil::TimedRead(tcp_fd, time_limit, &reply_data_size, sizeof(reply_data_size));
                reply_data_size = ntohs(reply_data_size);
                if (reply_data_size > reply_packet_size_) {
                    delete [] reply_packet_;
                    reply_packet_ = nullptr;
                    reply_packet_ = reinterpret_cast<byte *>(new uint32_t[(reply_data_size + sizeof(uint32_t) - 1)
                                                                          / sizeof(uint32_t)]);
                    reply_packet_size_ = reply_data_size;
                }

                actual_packet_size = reply_data_size;
                ssize_t byte_got(0);
                byte *buf_pointer(reply_packet_);

                while (reply_data_size > 0
                       and (byte_got = SocketUtil::TimedRead(tcp_fd, time_limit, buf_pointer, reply_data_size)) > 0)
                {
                    reply_data_size = static_cast<uint16_t>(reply_data_size - byte_got);
                    buf_pointer += byte_got;
                }

                if (byte_got == -1) {
                    if (verbosity_ >= 4)
                        logger->info("in resolve: SocketUtil::TimedRead() failed!");
                    return false;
                }
                if (verbosity_ >= 5)
                    logger->info("in resolve: actual_packet_size = " + std::to_string(actual_packet_size) + "d.");

                goto process_reply_packet;
            }

            if (verbosity_ >= 4)
                LogIpAddresses(domainname, "Reply", *ip_addresses, time_limit);
            return true;
        } else {
            if (verbosity_ >= 4)
                logger->info(domainname + " Failed: could not decode reply!");
            return false;
        }
    }
}


// Resolver::generateRequestPacket -- generates a DNS query packet.  See RFC1035 for details.
//
ptrdiff_t Resolver::GenerateRequestPacket(const std::string &hostname, const uint16_t request_id, unsigned char * const packet) {
    // Initialise the query header:
    HEADER * const dns_header = reinterpret_cast<HEADER *>(packet);
    std::memset(dns_header, '\0', sizeof(HEADER));
    dns_header->id = htons(request_id);
    dns_header->rd = 1; // Request recursion.
    dns_header->qdcount = htons(1);

    // Split the hostname into "labels" and append them to the packet header:
    std::list<std::string> labels;
    StringUtil::Split(hostname, '.', &labels, /* suppress_empty_components = */ false);
    unsigned char *cp = packet + sizeof(HEADER);
    for (std::list<std::string>::const_iterator label(labels.begin()); label != labels.end(); ++label) {
        *cp++ = static_cast<unsigned char>(label->length());
        for (const char *label_cp(label->c_str()); *label_cp != '\0'; ++label_cp)
            *cp++ = *label_cp;
    }
    *cp++ = 0;

    uint16_t *sp = reinterpret_cast<uint16_t *>(cp);
    *sp++ = htons(1); // Set the qtype field to type "A", indicating a host address.
    *sp++ = htons(1); // Set the qclass field to type "IN", indicating the Internet.

    return reinterpret_cast<unsigned char *>(sp) - packet;
}


void Resolver::sendUdpRequest(const in_addr_t resolver_ip_address, const unsigned char * const packet,
                              const unsigned packet_size) const
{
    if (unlikely(not SocketUtil::SendUdpRequest(udp_fd_, resolver_ip_address, 53 /* DNS service port */, packet, packet_size)))
        throw std::runtime_error("in Resolver::sendUdpRequest: sending a UDP request failed (" + std::to_string(errno)+ ")!");
}


int Resolver::sendTcpRequest(const in_addr_t resolver_ip_address, const TimeLimit &time_limit, const unsigned char * const packet,
                             const unsigned packet_size) const
{
    SocketUtil::NagleOptionType nagle_option_type;
#ifdef TCP_CORK
    nagle_option_type = SocketUtil::USE_NAGLE;
#else
    nagle_option_type = SocketUtil::DISABLE_NAGLE;
#endif
    std::string error_message;
    const int socket_fd(SocketUtil::TcpConnect(resolver_ip_address, 53, time_limit, &error_message, nagle_option_type));
    if (socket_fd == -1) {
        if (verbosity_ >= 4)
            logger->info("in Resolver::sendTcpRequest: SocketUtil::TcpConnect() failed (" + error_message + ")!");
        return -1;
    }

#ifdef TCP_CORK
    int optval(1);
    if (unlikely(::setsockopt(socket_fd, SOL_TCP, TCP_CORK, &optval, sizeof optval) == -1))
        throw std::runtime_error("in Resolver::sendTcpRequest: setsockopt(2) failed (1)!");
#endif

    uint16_t buf_size((uint16_t)(htons(packet_size)));
    if (SocketUtil::TimedWrite(socket_fd, time_limit, &buf_size, sizeof(buf_size)) == -1) {
        if (verbosity_ >= 4)
            logger->info("in Resolver::sendTcpRequest: SocketUtil::TimedWrite() failed!");
        ::close(socket_fd);
        return -1;
    }

    if (SocketUtil::TimedWrite(socket_fd, time_limit, packet, packet_size) == -1) {
        if (verbosity_ >= 4)
            logger->info("in Resolver::sendTcpRequest: SocketUtil::TimedWrite() failed!");
        ::close(socket_fd);
        return -1;
    }

#ifdef TCP_CORK
    optval = 0;
    if (unlikely(::setsockopt(socket_fd, SOL_TCP, TCP_CORK, &optval, sizeof optval) == -1))
        throw std::runtime_error("in Resolver::sendTcpRequest: setsockopt(2) failed (2)!");
#endif

    return socket_fd;
}


namespace {


// ExtractDomainName -- extract a domain name from a DNS server response.  See RFC1035 for details.
//
//                      The trickiest part is that at any point in decoding we can encounter what the RFC calls "message
//                      compression".  This is indicated by the two highest bits of a 16 bit quantity being set.  The
//                      lower 14 bits then indicate an offset relative to the start of the packet.  Message compression
//                      can occurr more than once within one domainname.
//
std::string ExtractDomainName(const unsigned char * const packet_start, const unsigned char *&cp) {
    std::string domain_name;
    bool first_label(true);
    while (*cp != 0) {
        if (not first_label)
            domain_name += '.';
        else
            first_label = false;

        // Message compression?
        if ((*cp & 0xC0u) == 0xC0u) { // 2 high-order bits are set => message compression!
            union {
                unsigned char bytes_[2];
                uint16_t u16_;
            } overlay;
            overlay.bytes_[0] = static_cast<unsigned char>(*cp & ~0xC0u);
            ++cp;
            overlay.bytes_[1] = *cp;
            ++cp;
            const uint16_t offset(ntohs(overlay.u16_));
            const unsigned char *cp1(packet_start + offset);
            return domain_name + ExtractDomainName(packet_start, cp1);
        }

        const std::string::size_type label_length(*cp++);
        domain_name += std::string(reinterpret_cast<const char *>(cp), label_length);
        cp += label_length;
    }
    ++cp; // Skip over trailing zero byte.

    return domain_name;
}


} // unnamed namespace


bool Resolver::DecodeReply(const unsigned char * const packet_start, const size_t packet_size,
                           std::set<std::string> * const domainnames, std::set<in_addr_t> * const ip_addresses,
                           uint32_t * const ttl, uint16_t * const reply_id, bool * const truncated,
                           const unsigned _verbosity)
{
    // Set the actual verbosity:
    const unsigned verbosity(_verbosity);

    if (unlikely(packet_size < sizeof(HEADER))) {
        if (verbosity > 2)
            logger->info("in Resolver::DecodeReply: got a short packet!");
        return false;
    }

    domainnames->clear();
    const HEADER *dns_header(reinterpret_cast<const HEADER *>(packet_start));
    *reply_id = ntohs(dns_header->id);
    *truncated = dns_header->tc;
    if (verbosity >= 5)
        logger->info("in Resolver::DecodeReply: TC bit is " + std::to_string(*truncated ? 1 : 0));
    if (unlikely(*truncated))
        return true; // Need to retry using TCP?

    // We set the "recursion desired" bit and therefore also expect it in our reply:
    if (unlikely(dns_header->rd != 1))
        return false;

    switch (dns_header->rcode) {
    case 0:
        /* We succeeded! */
        break;
    case 1:
        if (verbosity >= 2)
            logger->info("in Resolver::DecodeReply: the server indicated that we sent an invalid request!");
        #if __GNUC__ >= 7
        [[fallthrough]];
        #endif
    default:
        /* Some kind of error condition that we ignore. */
        return false;
    }

    const uint16_t qdcount = ntohs(dns_header->qdcount);
    const uint16_t ancount = ntohs(dns_header->ancount);

    // Skip over the question section:
    const unsigned char *cp(packet_start + sizeof(HEADER));
    for (unsigned question_record_index(0); question_record_index < qdcount; ++question_record_index) {
        const std::string query_domainname(ExtractDomainName(packet_start, cp));
        domainnames->insert(query_domainname);

        //const uint16_t rr_type(ntohs(*reinterpret_cast<const uint16_t *>(cp)));
        cp += sizeof(uint16_t);

        //const uint16_t rr_class(ntohs(*reinterpret_cast<const uint16_t *>(cp)));
        cp += sizeof(uint16_t);
    }

    for (unsigned resource_record_index(0); resource_record_index < ancount; ++resource_record_index) {
        const std::string rr_domainname(ExtractDomainName(packet_start, cp));

        const uint16_t rr_type(ntohs(*reinterpret_cast<const uint16_t *>(cp)));
        cp += sizeof(uint16_t);

        const uint16_t rr_class(ntohs(*reinterpret_cast<const uint16_t *>(cp)));
        cp += sizeof(uint16_t);

        *ttl = *reinterpret_cast<const uint32_t * const>(cp);
        *ttl = ntohl(*ttl);
        cp += sizeof(uint32_t);

        const uint16_t rdlength(ntohs(*reinterpret_cast<const uint16_t *>(cp)));
        cp += sizeof(uint16_t);

        if (rr_type == 1 and rr_class == 1) { // Type == A and class == IN.
            if (rdlength != sizeof(in_addr_t)) {
                if (verbosity >= 2)
                    logger->info("in Resolver::DecodeReply: unexpected \"rdlength\" for A type RR record!");
                return false;
            }

            domainnames->insert(rr_domainname);
            in_addr_t ip_address;
            std::memcpy(&ip_address, cp, sizeof(in_addr_t)); // Intentionally returned in network byte order!
            ip_addresses->insert(ip_address);
            cp += rdlength;

            if (verbosity >= 5)
                logger->info("in Resolver::DecodeReply: " + rr_domainname + ": "
                            + NetUtil::NetworkAddressToString(ip_address));
        } else if (rr_type == 5 and rr_class == 1) { // Type == CNAME and class == IN.
            const std::string cname_domainname(ExtractDomainName(packet_start, cp));
            domainnames->insert(cname_domainname);
            if (verbosity >= 5)
                logger->info("in Resolver::DecodeReply: " + rr_domainname + ": CNAME");

        }
        else // Nothing we're interested in.
            cp += rdlength; // Skip over the rdata field.
    }

    return true;
}


bool Resolver::ServerIsAlive(const std::string &server_ip_address, const std::string &hostname_to_resolve,
                             const unsigned time_limit, const unsigned verbosity)
{
    Resolver resolver(server_ip_address, verbosity);

    std::set<in_addr_t> ip_addresses;
    if (not resolver.resolve(hostname_to_resolve, time_limit, &ip_addresses))
        return false;

    return not ip_addresses.empty();
}


bool Resolver::ServerIsAlive(const in_addr_t server_ip_address, const std::string &hostname_to_resolve,
                             const unsigned time_limit, const unsigned verbosity)
{
    Resolver resolver(server_ip_address, verbosity);

    std::set<in_addr_t> ip_addresses;
    if (not resolver.resolve(hostname_to_resolve, time_limit, &ip_addresses))
        return false;

    return not ip_addresses.empty();
}


unsigned Resolver::GetServersFromResolvDotConf(std::vector<in_addr_t> * const server_ip_addresses) {
    server_ip_addresses->clear();

    File resolv_conf("/etc/resolv.conf", "r");
    if (resolv_conf.fail())
        throw std::runtime_error("in Resolver::GetServersFromResolvDotConf: can't open \"/etc/resolv.conf\" for reading!");

    while (not resolv_conf.eof()) {
        std::string line;
        resolv_conf.getline(&line);
        StringUtil::Trim(" \t", &line);
        if (line.length() < 12)
            continue;
        if (line.substr(0, 10) == "nameserver") {
            std::string resolver_ip_address;
            resolver_ip_address = line.substr(10);
            StringUtil::LeftTrim(&resolver_ip_address);

            in_addr_t ip_address;
            if (unlikely(not NetUtil::StringToNetworkAddress(resolver_ip_address, &ip_address)))
                throw std::runtime_error("in Resolver::GetServersFromResolvDotConf: \"" + resolver_ip_address + "\" is not a valid IP address (2)!");
            server_ip_addresses->push_back(ip_address);
        }
    }

    return static_cast<unsigned>(server_ip_addresses->size());
}


bool ThreadSafeDnsCache::lookup(const std::string &hostname, std::set<in_addr_t> * const ip_addresses) {
    // Synchronize access to the internal cache data structures:
    std::lock_guard<std::mutex> mutex_locker(cache_access_mutex_);

    std::unordered_map<std::string, ThreadSafeDnsCacheEntry>::iterator entry(resolved_hostnames_cache_.find(hostname));
    if (entry != resolved_hostnames_cache_.end()) {
        const time_t now(std::time(nullptr));
        if (entry->second.expire_time_ > now) {
            *ip_addresses = entry->second.ip_addresses_;
            return true;
        }

        // Entry has expired => remove it from the cache:
        resolved_hostnames_cache_.erase(entry);
    }

    return false;
}


void ThreadSafeDnsCache::insert(const std::string &hostname, const std::set<in_addr_t> &ip_addresses, const uint32_t ttl) {
    // Synchronize access to the internal cache data structures:
    std::lock_guard<std::mutex> mutex_locker(cache_access_mutex_);

    // Flush the cache if it contains more than 100,000 entries:
    if (resolved_hostnames_cache_.size() > 100000)
        resolved_hostnames_cache_.clear();
    else {
        // Check to see whether we already have information about this "hostname":
        std::unordered_map<std::string, ThreadSafeDnsCacheEntry>::iterator cache_entry(resolved_hostnames_cache_.find(hostname));
        if (unlikely(cache_entry != resolved_hostnames_cache_.end())) {
            for (const auto &ip_address : ip_addresses)
                cache_entry->second.ip_addresses_.insert(ip_address);
            return;
        }
    }

    // Create a new cache entry:
    const time_t now(std::time(nullptr));
    const ThreadSafeDnsCache::ThreadSafeDnsCacheEntry new_cache_entry(now + ttl, ip_addresses);
    resolved_hostnames_cache_.insert(std::make_pair(hostname, new_cache_entry));
}


SimpleResolver::SimpleResolver(const std::vector<std::string> &dns_servers) {
    // Get the resolver IP addresses from the "dns_server" parameter:
    if (not dns_servers.empty()) {
        for (std::vector<std::string>::const_iterator dns_server(dns_servers.begin());
             dns_server != dns_servers.end(); ++dns_server)
        {
            in_addr_t ip_address;
            if (unlikely(not NetUtil::StringToNetworkAddress(*dns_server, &ip_address)))
                throw std::runtime_error("in SimpleResolver::init: \"" + *dns_server
                                + "\" is not a valid IP address (1)!");
            dns_server_ip_addresses_and_busy_counts_.push_back(std::make_pair(ip_address, 0));
        }
    } else if (FileUtil::Exists(ETC_DIR "/Resolver.conf")) { // If a Resolver.conf file exists, read it.
        IniFile ini_file(ETC_DIR "/Resolver.conf");

        // Read DNS servers:
        if (ini_file.sectionIsDefined("DNS Servers")) {
            // Add each entry in the [DNS Servers] section of Resolver.conf:
            const std::list<std::string> names(ini_file.getSectionEntryNames("DNS Servers"));
            for (std::list<std::string>::const_iterator name(names.begin()); name != names.end(); ++name) {
                const std::string ip_address_str(ini_file.getString("DNS Servers", *name));
                in_addr_t ip_address;
                if (unlikely(not NetUtil::StringToNetworkAddress(ip_address_str, &ip_address)))
                    throw std::runtime_error("Resolver.conf: \"" + ip_address_str + "\""
                                    " is not a valid IP address (2)!");
                dns_server_ip_addresses_and_busy_counts_.push_back(std::make_pair(ip_address, 0));
            }
        }

        if (unlikely(dns_server_ip_addresses_and_busy_counts_.empty()))
            throw std::runtime_error("in SimpleResolver::init: failed to find DNS server IP address in " ETC_DIR
                            "/Resolver.conf!");
    } else { // As a last resort, attempt to get the resolver IP addresses from /etc/resolv.conf.
        std::ifstream resolv_conf("/etc/resolv.conf");
        if (resolv_conf.fail())
            throw std::runtime_error("in SimpleResolver::init: can't open \"/etc/resolv.conf\" for reading!");

        std::vector<std::string> resolvers;
        while (resolv_conf) {
            std::string line;
            std::getline(resolv_conf, line);
            StringUtil::Trim(" \t", &line);
            if (line.length() < 12)
                continue;
            if (line.substr(0, 10) == "nameserver") {
                std::string resolver_ip_address;
                resolver_ip_address = line.substr(10);
                StringUtil::LeftTrim(&resolver_ip_address);

                in_addr_t ip_address;
                if (unlikely(not NetUtil::StringToNetworkAddress(resolver_ip_address, &ip_address)))
                    throw std::runtime_error("in SimpleResolver::init: \"" + resolver_ip_address
                                    + "\" is not a valid IP address (2)!");
                dns_server_ip_addresses_and_busy_counts_.push_back(std::make_pair(ip_address, 0));
            }
        }

        if (unlikely(dns_server_ip_addresses_and_busy_counts_.empty()))
            throw std::runtime_error("in SimpleResolver::init: failed to find DNS server IP address in /etc/resolv.conf!");
    }

    // Ensure we have at least one DNS Server:
    if (dns_server_ip_addresses_and_busy_counts_.empty())
        throw std::runtime_error("in SimpleResolver::init: no DNS Servers found");
}


bool SimpleResolver::resolve(const std::string &hostname, const TimeLimit &time_limit, std::set<in_addr_t> * const ip_addresses) {
    ip_addresses->clear();

    // Check to see whether "hostname" is already an IP address:
    in_addr hostname_as_address;
    if (::inet_aton(hostname.c_str(), &hostname_as_address)) {
        ip_addresses->insert(hostname_as_address.s_addr);
        return true;
    }

    // See if we already know the answer to our query:
    if (dns_cache_.lookup(hostname, ip_addresses))
        return true;

    const FileDescriptor udp_fd(::socket(PF_INET, SOCK_DGRAM, 0));
    if (unlikely(udp_fd == -1))
        throw std::runtime_error("in SimpleResolver::resolve: socket(2) failed (" + std::to_string(errno)+ ")!");

    // Turn off blocking because we are going to use select(2) which on Linux doesn't reliably work with
    // blocking file descriptors:
    FileUtil::SetNonblocking(udp_fd);

    // Decide which resolver we should use and get the next request ID in a threadsafe manner:
    const in_addr_t resolver_address(getLeastBusyDnsServerAndIncUsageCount());

    //
    // Send the DNS request...
    //

    const uint16_t request_id(getNextRequestId());
    unsigned char packet[512];
    const ptrdiff_t packet_size(Resolver::GenerateRequestPacket(hostname, request_id, packet));
    if (unlikely(not SocketUtil::SendUdpRequest(udp_fd, resolver_address, 53 /* DNS service port */, packet, static_cast<unsigned>(packet_size))))
        throw std::runtime_error("in SimpleResolver::resolve: sending a UDP request failed (" + std::to_string(errno)+ ")!");

    //
    // ...and now wait for a reply:
    //

    // Allocate a buffer on the stack to hold UDP DNS server replies and make sure that it is 4-byte aligned:
    const size_t MAX_UDP_REPLY_PACKET_SIZE(512);
    uint32_t reply_packet_buffer[(MAX_UDP_REPLY_PACKET_SIZE + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
    unsigned char * const reply_packet(reinterpret_cast<unsigned char *>(reply_packet_buffer));
    const size_t max_reply_packet_size(MAX_UDP_REPLY_PACKET_SIZE);

    for (;;) {
        const ssize_t actual_reply_packet_size(SocketUtil::TimedRead(udp_fd, time_limit, reply_packet, max_reply_packet_size));
        if (actual_reply_packet_size <= 0) {
            if (likely(errno == ETIMEDOUT))
                return false;
            throw std::runtime_error("in SimpleResolver::resolve: SocketUtil::TimedRead() failed (" + std::to_string(errno)+ ")!");
        }
        else if (processServerReply(reply_packet, actual_reply_packet_size, request_id, ip_addresses))
            break;
    }

    decDnsServerUsageCount(resolver_address);

    return not ip_addresses->empty();
}


in_addr_t SimpleResolver::getLeastBusyDnsServerAndIncUsageCount() {
    std::lock_guard<std::mutex> mutex_locker(dns_server_ip_addresses_and_busy_count_access_mutex_);

    std::vector< std::pair<in_addr_t, unsigned> >::iterator server_and_usage_count(
        dns_server_ip_addresses_and_busy_counts_.begin());
    std::vector< std::pair<in_addr_t, unsigned> >::iterator least_busy_server(server_and_usage_count);
    for (++server_and_usage_count; server_and_usage_count != dns_server_ip_addresses_and_busy_counts_.end();
         ++server_and_usage_count)
    {
        if (server_and_usage_count->second < least_busy_server->second)
            least_busy_server = server_and_usage_count;
    }

    // Update usage count...
    ++least_busy_server->second;

    // ...and return IP address.
    return least_busy_server->first;
}


void SimpleResolver::decDnsServerUsageCount(const in_addr_t server_ip_address) {
    std::lock_guard<std::mutex> mutex_locker(dns_server_ip_addresses_and_busy_count_access_mutex_);

    // Look for the entry in "dns_server_ip_addresses_and_busy_counts_" and decrement the busy count:
    for (auto &server_and_usage_count : dns_server_ip_addresses_and_busy_counts_) {
        if (server_and_usage_count.first == server_ip_address) {
            if (unlikely(server_and_usage_count.second == 0))
                throw std::runtime_error("in SimpleResolver::decDnsServerUsageCount: this should never happen!");
            --server_and_usage_count.second;

            return;
        }
    }

    // If we get here we did not find a server entry matching "server_ip_address" which should never happen!
    throw std::runtime_error("in SimpleResolver::decDnsServerUsageCount: we should never get here!");
}


uint16_t SimpleResolver::getNextRequestId() {
    std::lock_guard<std::mutex> mutex_locker(request_id_mutex_);

    ++next_request_id_;
    return next_request_id_;
}


bool SimpleResolver::processServerReply(const unsigned char * const reply_packet, const size_t reply_packet_size,
                                        const uint16_t expected_reply_id, std::set<in_addr_t> * const ip_addresses)
{
    std::set<std::string> hostnames;
    uint32_t ttl(0);
    uint16_t reply_id;
    bool truncated;
    if (not Resolver::DecodeReply(reply_packet, reply_packet_size, &hostnames, ip_addresses, &ttl,
                                  &reply_id, &truncated)) // We received a garbled reply packet and will ignore it!
        return false;
    else if (unlikely(reply_id != expected_reply_id))     // We most likely received a reply to an earlier request
        return false;                                 // and will ignore it!
    else { // We're in luck.
        if (not ip_addresses->empty()) {
            // Update the DNS cache:
            for (const auto &hostname : hostnames)
                dns_cache_.insert(hostname, *ip_addresses, ttl);
        }

        return true;
    }
}
