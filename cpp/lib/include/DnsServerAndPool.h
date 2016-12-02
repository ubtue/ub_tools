/** \file    DnsServerAndPool.h
 *  \brief   Declaration of classes DnsServer and DnsServerPool.
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

#ifndef DNS_SERVER_AND_POOL_H
#define DNS_SERVER_AND_POOL_H


#include <string>
#include <list>
#include <unordered_map>
#include <vector>
#include <inttypes.h>
#include <arpa/inet.h>
#include <unistd.h>


/** \class  DnsServer
 *  \brief  Keeps track of requests for a single DNS server.
 */
class DnsServer {
    const in_addr_t server_ip_address_;
    const unsigned max_outstanding_request_count_;
    int socket_fd_; // File descriptor.
    unsigned request_lifetime_; // in milliseconds
    static uint16_t next_query_id_;
public:
    struct OutstandingRequest {
        uint16_t query_id_;
        std::string hostname_;
        uint64_t expiration_time_;
    public:
        OutstandingRequest(const uint16_t query_id, const std::string &hostname, const uint64_t expiration_time)
            : query_id_(query_id), hostname_(hostname), expiration_time_(expiration_time) { }
    };
private:
    class OutstandingRequests: private std::list<OutstandingRequest> {
        const unsigned max_count_; // Store no more than this many requests!
        unsigned count_;
    public:
        OutstandingRequests(const unsigned max_count): max_count_(max_count), count_(0) { }
        using std::list<OutstandingRequest>::empty;
        unsigned size() const { return count_; }
        void addRequest(const uint16_t query_id, const std::string &hostname, const uint64_t expiration_time);
        bool removeRequest(const uint16_t query_id, std::string * const hostname);
        void expireOldRequests();
    private:
        OutstandingRequests(const OutstandingRequests &rhs) = delete;
        const OutstandingRequests &operator=(const OutstandingRequests &rhs) = delete;
    };
    mutable OutstandingRequests outstanding_requests_;
public:
    /** \brief  Creates a new instance of a DnsServer object.
     *  \param  server_ip_address              The IP address of our associated DNS server.
     *  \param  max_outstanding_request_count  Up to how many recent request without replies we allow for.
     *  \param  request_lifetime               How long we wait for a DNS server reply without an answer before
     *                                         discarding a request.  (In milliseconds.)
     */
    DnsServer(const in_addr_t &server_ip_address, const unsigned max_outstanding_request_count, const unsigned request_lifetime = 5000);

    ~DnsServer() { if (socket_fd_ != -1 ) ::close(socket_fd_); }

    /** Returns the socket file descriptor used to communicate with our associated DNS server. */
    int getFileDescriptor() const { return socket_fd_; }

    in_addr_t getIpAddress() const { return server_ip_address_; }

    /** Returns true if we currently can't send any additional requests to our associated DNS server. */
    bool isBusy() const;

    /** Returns the number of outstanding requests with our DNS server. */
    unsigned getQueueLength() const { return outstanding_requests_.size(); }

    /** Returns the maximum number of outstanding requests with our DNS server. */
    unsigned getMaxQueueLength() const { return max_outstanding_request_count_; }

    /** \brief   Attempts to submit a new name lookup request to our DNS server.
     *  \param   valid_hostname  A name hostname that we want to resolve.
     *  \param   query_id        Uniquely identifies the submitted request.
     *  \param   deadline        If non-nullptr we store the request's expiration time in milliseconds since the epoch here.
     *  \return  True if we submitted the new request to our DNS server or false if our server is currently busy.
     */
    bool addLookupRequest(const std::string &valid_hostname, uint16_t * const query_id, uint64_t * const deadline = nullptr);

    /** \brief  Call this function if our socket file descriptor is ready for reading.
     *  \param  resolved_ip_addresses  List of IP address for an earlier submitted lookup request.  May be incomplete!
     *  \param  resolved_domainnames   List of hostnames for "resolved_ip_addresses".  May be incomplete!  We do guarantee that the first hostname is
     *                                 the one that was submitted with the original lookup request that led to the currently processed server reply.
     *  \param  ttl                    Time-to-live for the information received by our DNS server.  Typically used to implement a cache on top of this
     *                                 class.
     *  \param  query_id               Unique ID of the query for which we provide a resolution.
     *  \return True if we got a reply from a DNS server that resulted in us returning a useful result, otherwise false.
     *  \note   If the lookup failed "resolved_ip_addresses" will be empty and "ttl" will be unset.  But "resolved_domainnames" will have at least one
     *          entry.
     */
    bool processServerReply(std::vector<in_addr_t> * const resolved_ip_addresses, std::vector<std::string> * const resolved_domainnames,
                            uint32_t * const ttl, uint16_t * const query_id);
private:
    DnsServer(const DnsServer &rhs) = delete;
    const DnsServer &operator=(const DnsServer &rhs) = delete;
};


/** \class  DnsCache
 *  \brief  A cache where the results of DNS lookups are stored for later use.
 */
class DnsCache {
    const unsigned cache_flush_size_; // Flush the cache when we reach this size.
    struct DnsCacheEntry {
        time_t expire_time_;
        in_addr_t ip_address_;
    public:
        DnsCacheEntry(const uint64_t expire_time, const in_addr_t &ip_address)
            : expire_time_(expire_time), ip_address_(ip_address) { }
    };
    std::unordered_map<std::string, DnsCacheEntry> resolved_hostnames_cache_;
    unsigned bad_dns_expire_time_;
public:
    static const in_addr_t BAD_ENTRY;
public:
    /** \brief  Create a DnsCache instance.
     *  \param  cache_flush_size     The amount of entries that trigger a complete cache flush.
     *  \param  bad_dns_expire_time  The amount of time (in seconds) that we cache unresolvable entries for.
     */
    explicit DnsCache(const unsigned cache_flush_size, const unsigned bad_dns_expire_time = 7200)
        : cache_flush_size_(cache_flush_size), bad_dns_expire_time_(bad_dns_expire_time) { }

    /** \brief  Check for a cached DNS entry.
     *  \param  hostname  Name we'd like to resolve.
     *  \param  ip_address  Upon a successful return this will contain the resolved address or BAD_ENTRY if we found an unresolvable entry.
     *  \return True if we found "hostname" in the cache, else false.
     */
    bool lookup(const std::string &hostname, in_addr_t * const ip_address);

    void insert(const std::string &hostname, const in_addr_t &ip_address, const uint32_t ttl);

    void insertUnresolvableEntry(const std::string &hostname) { insert(hostname, BAD_ENTRY, bad_dns_expire_time_ * 1000); }
private:
    DnsCache(const DnsCache &rhs) = delete;
    const DnsCache &operator=(const DnsCache &rhs) = delete;
};


/**  \class  DnsServerPool
 *   \brief  Implements DNS lookup using a DNS server pool.
 *
 *   This class internally manages a set of DNS servers.  Requests are submitted via addLookupRequest(), which may return an IP address immediately if a
 *   translation has been cached.  It not, an external mechanism, e.g. select(2) will have to be used to determine when a server reply has occurred.  Then
 *   processServerReply() should be called.
 */
class DnsServerPool {
    DnsCache cache_;
    const unsigned max_queue_length_per_server_;
    std::vector<DnsServer *> servers_;
public:
    /** \brief  Initialises a DnsServerPool object.
     *  \param  dns_server_ip_addresses         A list of IP addresses for valid DNS servers.
     *  \param  server_socket_file_descriptors  The file descriptors that need to be monitored for read-readiness before a call to
     *                                          processServerReply().  Note: These descriptors will not have to be cleaned up.  The DnsServerPool
     *                                          destructor takes care of this!
     *  \param  request_lifetime                How long we wait for a DNS server reply without an answer before discarding a request.  (In
     *                                          milliseconds.)
     *  \param  max_queue_length_per_server     The maximum number of outstanding requests per DNS server before we consider the entire server pool to
     *                                          be busy.
     *  \param  max_cache_size                  Up to how many translations we're willing to cache.
     */
    explicit DnsServerPool(const std::vector<in_addr_t> &dns_server_ip_addresses, std::vector<int> * const server_socket_file_descriptors,
                           const unsigned request_lifetime = 5000, const unsigned max_queue_length_per_server = 10,
                           const unsigned max_cache_size = 10000);

    ~DnsServerPool();

    /** \return True, if there are no outstanding or queued requests, else returns false. */
    bool empty() const { return getQueueLength() == 0; }

    /** Returns "true" if all of our DNS servers are busy, otherwise returns "false". */
    bool isBusy() const;

    /** \brief   Used to submit a lookup request to the server pool.
     *  \param   valid_hostname       Must be a valid hostname to be translated to an IP address.
     *  \param   resolved_ip_address  If this function returns true, will contain the reolved address.
     *  \note    If all servers were busy and we were unable to send a request we set "*socket_fd" to -1.
     *  \return  "True" if we were able to immediately resolve the hostname, otherwise "false".
     *  \note    When we return true "resolved_ip_address" may be DnsCache::BAD_ENTRY!  This indicates that a DNS server indicated that no record
     *           was found for "valid_hostname."
     */
    bool addLookupRequest(const std::string &valid_hostname, in_addr_t * const resolved_ip_address);

    /** \brief  Use this when a socket file descriptor is ready to process a DNS server reply.
     *  \param  socket_fd            The socket file descriptor on which the server reply occurred.
     *  \param  resolved_ip_address  If this function returns "true", will contain a resolved IP address or DnsCache::BAD_ENTRY.
     *  \param  hostname             The hostname that was submitted with the initial request.
     *  \return True if we got a reply from a DNS server that resulted in us returning a useful result, otherwise false.
     */
    bool processServerReply(const int socket_fd, in_addr_t * const resolved_ip_address, std::string * const hostname);

    /** Returns the number of outstanding requests with our DNS servers. */
    unsigned getQueueLength() const;
private:
    DnsServerPool(const DnsServerPool &rhs) = delete;
    const DnsServerPool &operator=(const DnsServerPool &rhs) = delete;

    /** Returns the average queue length averaged over all DNS servers.  Used internally. */
    double getAverageQueueLength() const;
};


#endif // ifndef DNS_SERVER_AND_POOL_H
