/** \file    DnsUtil.cc
 *  \brief   Implementation of DNS related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2017 Universtitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "DnsUtil.h"
#include <list>
#include <stdexcept>
#include <cerrno>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "DnsServerAndPool.h"
#include "NetUtil.h"
#include "Resolver.h"
#include "StringUtil.h"
#include "TimerUtil.h"
#include "Url.h"


namespace DnsUtil {


bool IsValidIPv4Address(const std::string &address) {
    return DnsUtil::IsDottedQuad(address);
}


namespace {


enum LabelOption { ALLOW_UNDERSCORES_AND_LEADING_DIGITS, DISALLOW_UNDERSCORES_AND_LEADING_DIGITS };


bool IsValidLabel(const std::string &label, const LabelOption label_option) {
    if (unlikely(label.empty()))
        return false;

    // Examine first character:
    std::string::const_iterator ch(label.begin());
    if (not (StringUtil::IsAsciiLetter(*ch)
             or (label_option == ALLOW_UNDERSCORES_AND_LEADING_DIGITS and StringUtil::IsDigit(*ch))))
        return false;

    // Look at the characters in the middle:
    while (ch != label.end() - 1) {
        ++ch;
        if (not (StringUtil::IsAlphanumeric(*ch) or *ch == '-'
                 or (label_option == ALLOW_UNDERSCORES_AND_LEADING_DIGITS and *ch == '_')))
            return false;
    }

    // Examine last character:
    return StringUtil::IsAlphanumeric(*ch);
}


} // unnamed namespace


// IsValidHostName -- See RFC952 and RFC1123 for documentation
//
bool IsValidHostName(const std::string &host_name) {
    if (host_name.length() > 255)
        return false;

    if (unlikely(host_name.empty()))
        return false;

    // Disallow leading periods:
    if (unlikely(host_name[0] == '.'))
        return false;

    std::vector<std::string> labels;
    const unsigned label_count(StringUtil::Split(host_name, '.', &labels, /* suppress_empty_components = */ false));
    if (unlikely(label_count == 0))
        return false;

    for (std::vector<std::string>::const_iterator label(labels.begin()); label != labels.end(); ++label) {
        if (unlikely(not IsValidLabel(*label, ALLOW_UNDERSCORES_AND_LEADING_DIGITS)))
            return false;
    }

    return true;
}


bool IsDottedQuad(const std::string &possible_dotted_quad) {
    std::list<std::string> octets;
    StringUtil::Split(possible_dotted_quad, ".", &octets);
    if (octets.size() != 4)
        return false;
    for (std::list<std::string>::const_iterator octet(octets.begin()); octet != octets.end(); ++octet) {
        if (not StringUtil::IsUnsignedNumber(*octet))
            return false;
        if (StringUtil::ToUnsigned(*octet) > 255)
            return false;

    }

    return true;
}


bool IpAddrToHostname(const std::string &dotted_quad, std::string * const hostname) {
    hostname->clear();

    struct in_addr in_addr;
    if (::inet_aton(dotted_quad.c_str(), &in_addr) < 1)
        return false;

    h_errno = 0;
    struct hostent *entry = ::gethostbyaddr(reinterpret_cast<const char *>(&in_addr), sizeof(in_addr), AF_INET);
    if (h_errno != 0)
        return false;
    *hostname = entry->h_name;

    return true;
}


bool IpAddrToHostnames(const std::string &dotted_quad, std::list<std::string> * const hostnames) {
    hostnames->clear();

    struct in_addr in_addr;
    if (::inet_aton(dotted_quad.c_str(), &in_addr) < 1)
        return false;

    h_errno = 0;
    struct hostent *entry = ::gethostbyaddr(reinterpret_cast<const char *>(&in_addr),
                                            sizeof(in_addr), AF_INET);
    if (h_errno != 0)
        return false;

    hostnames->push_back(entry->h_name);

    for (const char * const *alias = entry->h_aliases; *alias != 0 /* rather than nullptr */; ++alias)
        hostnames->push_back(*alias);
    return true;
}


bool TimedGetHostByName(const std::string &hostname, const TimeLimit &time_limit, in_addr_t * const ip_address,
                        std::string * const error_message)
{
    // Make sure we *never* take more than 20 seconds:
    const TimeLimit local_time_limit(time_limit.getRemainingTime() < 20000 ? time_limit.getRemainingTime() : 20000);

    try {
        static SimpleResolver resolver;

        std::set<in_addr_t> ip_addresses;
        if (resolver.resolve(hostname, local_time_limit, &ip_addresses)) {
            *ip_address = *(ip_addresses.begin());
            return true;
        }

        if (local_time_limit.limitExceeded())
            *error_message = "timed out";
        else
            *error_message = "unknown resolver error";
    } catch (const std::exception &x) {
        *error_message = x.what();
    }

    return false;
}


std::string GetHostname() {
    const size_t max_hostname_length(::sysconf(_SC_HOST_NAME_MAX));
    #pragma GCC diagnostic ignored "-Wvla"
    char hostname[max_hostname_length + 1];
    #pragma GCC diagnostic warning "-Wvla"
    ::gethostname(hostname, max_hostname_length);

    return hostname;
}


in_addr_t GetIPAddress(const std::string &url, unsigned dns_timeout) {
    const unsigned CACHE_FLUSH_SIZE(200000U);

    const uint32_t ARBITRARY_TTL(7200000);// No TTL info available so just wing it.

    static DnsCache cache(CACHE_FLUSH_SIZE);
    in_addr_t ip;
    std::string dns_error;
    std::string domain(Url(url).getAuthority());
    if (cache.lookup(domain, &ip))
        return ip;
    if (DnsUtil::TimedGetHostByName(domain, dns_timeout, &ip, &dns_error)) {
        cache.insert(domain, ip, ARBITRARY_TTL);
        return ip;
    } else {
        cache.insertUnresolvableEntry(domain);
        return DnsCache::BAD_ENTRY;
    }
}


std::string GetIPAddressStr(const std::string &url, unsigned dns_timeout) {
    in_addr_t ip_address(GetIPAddress(url, dns_timeout));
    return ip_address == DnsCache::BAD_ENTRY ? "" : NetUtil::NetworkAddressToString(ip_address);
}


} // namespace DnsUtil
