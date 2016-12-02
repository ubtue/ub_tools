/** \file    Resolver.h
 *  \brief   Declaration of class Resolver.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2007 Project iVia.
 *  Copyright 2005-2007 The Regents of The University of California.
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

#ifndef RESOLVER_H
#define RESOLVER_H


#include <list>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <arpa/inet.h>
#include "TimeLimit.h"


//Forward declaraions:
class Logger;


/** \class  Resolver
 *  \brief  Implements a caching DNS service.
 */
class Resolver {
    /** If non-zero, then log all Resolver actions in a log file. */
    unsigned verbosity_;

    /** If verbosity_ is non-zero then this used to log operations. */
    Logger * logger_;
    bool cleanup_logger_; // If true, it indicates that we internally allocated our own Logger and need to delete it
    // when we destroy an instance of this class!

    int udp_fd_;

    typedef unsigned char byte;
    byte *reply_packet_;
    uint16_t reply_packet_size_;

    /** The set of DNS servers of whom we can make requests. */
    std::unordered_map<in_addr_t, unsigned> dns_server_ip_addresses_and_busy_counts_;

    /** \class  Cache
     *  \brief  A cache where the results of DNS lookups are stored for later re-use.
     */
    class Cache {
        struct CacheEntry {
            time_t expire_time_;
            std::set<in_addr_t> ip_addresses_;
        public:
            CacheEntry(const time_t expire_time, const std::set<in_addr_t> &ip_addresses)
                : expire_time_(expire_time), ip_addresses_(ip_addresses) { }
        };
        std::unordered_map<std::string, CacheEntry> resolved_hostnames_cache_;
    public:
        Cache() { }
        bool lookup(const std::string &hostname, std::set<in_addr_t> * const ip_addresses);
        void insert(const std::string &hostname, const std::set<in_addr_t> &ip_addresses, const uint32_t ttl);
    } cache_;

    static uint16_t next_request_id_;
public:
    enum ResultType { RESOLVED, UNKNOWN };
    struct Result {
        ResultType result_type_;
        std::string hostname_;
        std::set<in_addr_t> ip_addresses_;
    public:
        Result(const ResultType result_type, const std::string &hostname, const std::set<in_addr_t> ip_addresses)
            : result_type_(result_type), hostname_(hostname), ip_addresses_(ip_addresses) { }
    };
private:
    std::list<Result> resolved_addresses_;
public:
    /** \brief  Construct a Resolver object.
     *  \param  dns_servers  Optional list of nameserver IP addresses for use by the resoolver.
     *  \param  logger       The logger object to log to.
     *  \param  verbosity    The log verbosity requested.
     *  \note The Resolver class requires a list of nameserver IP addresses, which can come froom one of three sources.  If
     *  the dns_servers parameter is non-empty, then nameservers will be drawn from this list.  Otherwise, if a
     *  ~/.iViaCore/Resolver.conf file exists and contains a [DNS Servers] section, the nameservers will be read from
     *  this section.  Otherwise, nameservers will be drawn from the standard UNIX /etc/resolv.conf file.
     */
    explicit Resolver(const std::list<std::string> &dns_servers = std::list<std::string>(),
                      Logger * const logger = nullptr, const unsigned verbosity = 3);
    explicit Resolver(const std::string &dns_server, Logger * const logger = nullptr, const unsigned verbosity = 3);
    explicit Resolver(const in_addr_t dns_server, Logger * const logger = nullptr, const unsigned verbosity = 3);
    ~Resolver();

    /** \brief   Submits a lookup request to a DNS server.
     *  \param   hostname  The name we want resolved.
     *  \warning You must not mix calls to this interface with calls to resolve()!
     */
    void submitRequest(const std::string &hostname);

    /** \brief   Poll for resolved hostname IP addresses.
     *  \param   results  List of resolved and/or failed lookups.
     *  \warning You must not mix calls to this interface with calls to resolve()!
     */
    void poll(std::list<Result> * const results);

    /** \brief   One-shot address resolve routine.  If you need to resolve multiple addresses, please consider
     *           submitRequest() and poll() instead.
     *  \param   domainname    The name we want to resolve.
     *  \param   time_limit    A time limit in milliseconds.  Will be updated upon return, e.g. set to 0 if we have a
     *                         timeout.
     *  \param   ip_addresses  Where the resolved IP address will be stored if we have a successful return.
     *  \return  "True" if "hostname" has been properly resolved, otherwise "false".
     *  \warning You must not mix calls to this interface with calls to either submitRequest() or poll()!
     */
    bool resolve(const std::string &domainname, const TimeLimit &time_limit, std::set<in_addr_t> * const ip_addresses);

    /** \brief  Generates a DNS query request packet.
     *  \param  hostname    The hostname we want to resolve.
     *  \param  request_id  The request ID of the generated DNS request in host byte-order.
     *  \param  packet      Where to store the generated query request package.
     *  \return The size of the generated packet.
     */
    static ptrdiff_t GenerateRequestPacket(const std::string &hostname, const uint16_t request_id, unsigned char * const packet);

    /** \brief  Decodes a nameserver reply and, if possible, extracts one or more IP addresses from the reply.
     *  \param  packet_start  Points to the start of the received network packet.
     *  \param  packet_size   The length of the received network packet.
     *  \param  domainnames   After a successful call this will point to the domainname and associated CNAMEs of the
     *                        resolved IP address.
     *  \param  ip_addresses  After a successful call this will point to a set of IPv4 addresses in network byte
     *                        order.
     *  \param  ttl           Time-to-live for the returned nameserver information.
     *  \param  reply_id      The ID of the reply packet in host byte-order.
     *  \param  truncated     True if the "TC" bit was set in the DNS server's reply header.
     *  \param  logger        If non-nullptr, where we send logging messages.
     *  \param  verbosity     Verbosity of logging (0-5).  Only relevant if "logger" is not nullptr.
     *  \return True if we successfully extracted at least one IP address or we received a truncated reply, else false.
     */
    static bool DecodeReply(const unsigned char * const packet_start, const size_t packet_size, std::set<std::string> * const domainnames,
                            std::set<in_addr_t> * const ip_addresses, uint32_t * const ttl, uint16_t * const reply_id, bool * const truncated,
                            Logger * const logger = nullptr, const unsigned verbosity = 3);

    /** \brief   Checks whether a DNS server is alive or not.
     *  \param   server_ip_address    The server's IP address as a dotted quad.
     *  \param   hostname_to_resolve  The hostname whose IP address lookup we want to use as a test.
     *  \param   time_limit           The maximum amount of time we want to give the server to respond (in milliseconds).
     *  \param   logger               The logger object to log to.
     *  \param   verbosity            The log verbosity requested.
     *  \return  True if the server responded within the given maximum time or false if it didn't.
     *  \warning This function is absolutely not thread-safe!
     */
    static bool ServerIsAlive(const std::string &server_ip_address, const std::string &hostname_to_resolve,
                              const unsigned time_limit = 5000, Logger * const logger = nullptr, const unsigned verbosity = 3);
    static bool ServerIsAlive(const in_addr_t server_ip_address, const std::string &hostname_to_resolve,
                              const unsigned time_limit = 5000, Logger * const logger = nullptr, const unsigned verbosity = 3);

    /** \brief  Extracts DNS server IP addresses from /etc/resolv.conf.
     *  \param  server_ip_addresses  Upon return, this variable will contain the resolver IP addresses.
     *  \return The number of extracted IP addresses.
     */
    static unsigned GetServersFromResolvDotConf(std::vector<in_addr_t> * const server_ip_addresses);
private:
    Resolver(const Resolver &rhs);                 // Intentionally unimplemented!
    const Resolver operator=(const Resolver &rhs); // Intentionally unimplemented!

    /** Initialises the UDP socket that we use to talk to servers. */
    void initUdpSocket();

    /** \brief  Sends a DNS request encoded in "packet" to "resolver_ip_address" using UDP.
     *  \param  resolver_ip_address  IP address of a nameserver.
     *  \param  packet               The initialised DNS request packet to send.
     *  \param  packet_size          The size of the DNS request packet.
     */
    void sendUdpRequest(const in_addr_t resolver_ip_address, const unsigned char * const packet, const unsigned packet_size) const;

    /** \brief  Sends a DNS request encoded in "packet" to "resolver_ip_address" using TCP.
     *  \param  resolver_ip_address  IP address of a nameserver.
     *  \param  time_limit           A time limit in milliseconds.  Will be updated upon return, e.g. set to 0 if we have a timeout.
     *  \param  packet               The initialised DNS request packet to send.
     *  \param  packet_size          The size of the DNS request packet.
     *  \return A valid connected socket file descriptor or -1 if an error occurred.
     */
    int sendTcpRequest(const in_addr_t resolver_ip_address, const TimeLimit &time_limit, const unsigned char * const packet,
                       const unsigned packet_size) const;

    static ssize_t TimedUdpRead(int socket_fd, const TimeLimit &time_limit, void * const data, size_t data_size) throw ();
};


/** \class  ThreadSafeDnsCache
 *  \brief  A thread-safe DNS lookup cache.
 */
class ThreadSafeDnsCache {
    struct ThreadSafeDnsCacheEntry {
        time_t expire_time_;
        std::set<in_addr_t> ip_addresses_;
    public:
        ThreadSafeDnsCacheEntry(const time_t expire_time, const std::set<in_addr_t> &ip_addresses)
            : expire_time_(expire_time), ip_addresses_(ip_addresses) { }
    };
    std::unordered_map<std::string, ThreadSafeDnsCacheEntry> resolved_hostnames_cache_;
    std::mutex cache_access_mutex_;
public:
    ThreadSafeDnsCache() { }
    bool lookup(const std::string &hostname, std::set<in_addr_t> * const ip_addresses);
    void insert(const std::string &hostname, const std::set<in_addr_t> &ip_addresses, const uint32_t ttl);
private:
    ThreadSafeDnsCache(const ThreadSafeDnsCache &rhs);                 // Intentionally unimplemented!
    const ThreadSafeDnsCache operator=(const ThreadSafeDnsCache &rhs); // Intentionally unimplemented!
};


/** \class  SimpleResolver
 *  \brief  Implements a simple sequential resolver with optional support for an external DNS cache.
 *  \note   If used within a multithreaded application you typically want to create a SimpleResolver object in the main thread and then pass references to
 *          the single object into the individual worker threads and then call resolve() within each worker thread.
 */
class SimpleResolver {
    ThreadSafeDnsCache dns_cache_;
    std::vector< std::pair<in_addr_t, unsigned> > dns_server_ip_addresses_and_busy_counts_;
    std::mutex dns_server_ip_addresses_and_busy_count_access_mutex_;
    uint16_t next_request_id_;
    std::mutex request_id_mutex_;
public:
    /** \brief  Creates a resolver.
     *  \param  dns_servers  If non-empty these DNS servers will be used.  Otherwise  DNS servers in listed ETC_DIR "/Resolver.conf" and if that
     *                       doesn't exist from servers listed in /etc/resolv.conf will be used.
     */
    explicit SimpleResolver(const std::vector<std::string> &dns_servers = std::vector<std::string>());

    /** \brief  Attempts to resolve a hostname to one or more IP addresses.
     *  \param  hostname      The hostname to resolve.  Is allowed to be an IP address.
     *  \param  time_limit    Lookup time limit in milliseconds.
     *  \param  ip_addresses  If the lookup succeeds, this is where some or all of the IP addresses corresponding to "hostname" will be returned.
     *  \return True if the lookup succeeded or false if the lookup failed within the given time constraints.
     */
    bool resolve(const std::string &hostname, const TimeLimit &time_limit, std::set<in_addr_t> * const ip_addresses);
private:
    SimpleResolver(const SimpleResolver &rhs);                 // Intentionally unimplemented!
    const SimpleResolver operator=(const SimpleResolver &rhs); // Intentionally unimplemented!
    void init(const std::vector<std::string> &dns_servers, ThreadSafeDnsCache * const cache);
    in_addr_t getLeastBusyDnsServerAndIncUsageCount();
    void decDnsServerUsageCount(const in_addr_t server_ip_address);
    uint16_t getNextRequestId();
    bool processServerReply(const unsigned char * const reply_packet, const size_t reply_packet_size, const uint16_t expected_reply_id,
                            std::set<in_addr_t> * const ip_addresses);
};


#endif // ifndef RESOLVER_H
