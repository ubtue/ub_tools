/** \file    DnsUtil.h
 *  \brief   Declarations for Infomine DNS utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#ifndef DNS_UTIL_H
#define DNS_UTIL_H


#include <string>
#include <list>
#include <csetjmp>
#include <netdb.h>
#include <netinet/in.h>
#include "TimeLimit.h"


/** Global operator for testing of equality of two structs in_addr. */
inline bool operator==(const in_addr in_addr1, const in_addr in_addr2) {
    return in_addr1.s_addr == in_addr2.s_addr;
}


/** Global operator for testing of equality of two structs in_addr. */
inline bool operator!=(const in_addr in_addr1, const in_addr in_addr2) {
    return in_addr1.s_addr != in_addr2.s_addr;
}


namespace DnsUtil {


/** \brief  Checks if the address is a valid IPv4 address.
 *  \param  address  an adress to check
 *  \return True if valid.
 */
bool IsValidIPv4Address(const std::string &address);


/** \brief   Is the host name technically correct?
 *  \param   host_name  The domain name to evaluate.
 *  \return  True if the host name is valid, false otherwise.
 *  \note    "host_name" must follow RFC952 and RFC1123.  In particular a "host_name" must have at least
 *           one period in order to be valid.
 */
bool IsValidHostName(const std::string &host_name);


bool IsDottedQuad(const std::string &possible_dotted_quad);
bool IpAddrToHostname(const std::string &dotted_quad, std::string * const hostname);
bool IpAddrToHostnames(const std::string &dotted_quad, std::list<std::string> * const hostnames);


/** \brief   A replacement for gethostbyname(3) that supports imposing a time limit.
 *  \param   hostname       A hostname that should be mapped to one or more IP addresses.
 *  \param   time_limit     Available time in milliseconds (will be updated by this function).  If it exceeds 20s
 *                          we throw an exception!
 *  \param   ip_address     The resolved IP address in network byte order upon a successful return.
 *  \param   error_message  If an error or timeout occurs a textual description will be returned here.
 *  \return  True for success, else false.
 */
bool TimedGetHostByName(const std::string &hostname, const TimeLimit &time_limit, in_addr_t * const ip_address,
                        std::string * const error_message);


/** Returns the current system's hostname. */
std::string GetHostname();


/** \brief  This is a caching function that returns an IP address for a URL.
 *  \param  url The URL to look up.
 *  \return DnsCache::BAD_ENTRY on failure, a dotted quad on success.
 */
in_addr_t GetIPAddress(const std::string &url, unsigned dns_timeout = 2000);


/** \brief  This is a caching function that returns an IP address for a URL.
 *  \param  url The URL to look up.
 *  \return Empty string on failure, a dotted quad string on success.
 */
std::string GetIPAddressStr(const std::string &url, unsigned dns_timeout = 2000);


} // namespace DnsUtil


#endif // ifndef DNS_UTIL_H
