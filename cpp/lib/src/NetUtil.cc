/** \file    NetUtil.cc
 *  \brief   Implementation of network-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#include "NetUtil.h"
#include <stdexcept>
#include <unordered_map>
#include <cerrno>
#include <cmath>
#include <net/if.h>
#include <sys/ioctl.h>
#include "StringUtil.h"


#ifndef DIM
#       define DIM(array)       (sizeof(array) / sizeof(array[0]))
#endif


namespace NetUtil {


bool StringToNetworkAddressAndMask(const std::string &s, in_addr_t * const network_address, in_addr_t * const netmask) {
    std::string::size_type slash_pos = s.find('/');
    if (slash_pos == std::string::npos)
        return false;

    in_addr address;
    if (not ::inet_aton(s.substr(0, slash_pos).c_str(), &address))
        return false;
    *network_address = address.s_addr;

    if (s.length() <= slash_pos or not StringUtil::IsUnsignedNumber(s.substr(slash_pos + 1)))
        return false;

    unsigned subnet_size = std::atoi(s.substr(slash_pos + 1).c_str());
    if (subnet_size > 32)
        return false;
    else if (subnet_size == 0)
        *netmask = 0;
    else {
        *netmask = ~0u << (32u - subnet_size);
        *netmask = htonl(*netmask);
    }

    return true;
}


bool StringToNetworkAddress(const std::string &s, in_addr_t * const network_address) {
    in_addr address;
    if (not ::inet_aton(s.c_str(), &address))
        return false;
    *network_address = address.s_addr;

    return true;
}


in_addr_t StringToNetworkAddress(const std::string &s) {
    in_addr_t network_address(0x0);
    if (likely(StringToNetworkAddress(s, &network_address)))
        return network_address;

    throw std::runtime_error("in NetUtil::StringToNetworkAddress: \"" + s + "\" is not a valid IPv4 address!");
}


bool NetworkAddressToString(const in_addr_t network_address, std::string * const s) {
    uint32_t network_address_in_host_byte_order(network_address);
    network_address_in_host_byte_order = ntohl(network_address_in_host_byte_order);
    char buf[3 + 1 + 3 + 1 + 3 + 1 + 3 + 1];
    std::sprintf(buf, "%0u.%0u.%0u.%0u", (network_address_in_host_byte_order >> 24u),
                 (network_address_in_host_byte_order >> 16u) & 0xFF,
                 (network_address_in_host_byte_order >> 8u) & 0xFF,
                 network_address_in_host_byte_order & 0xFF);
    *s = buf;

    return true;
}


std::string NetworkAddressToString(const in_addr_t network_address) {
    std::string network_address_as_string;
    if (unlikely(not NetworkAddressToString(network_address, &network_address_as_string)))
        throw std::runtime_error("in NetUtil::NetworkAddressToString: can't convert in_addr_t to an IPv4 address!");

    return network_address_as_string;
}


namespace {


void PopulateNetmaskToPrefixTable(std::unordered_map<in_addr_t, std::string> * const netmask_to_prefix) {
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("128.0.0.0"), std::string("/1")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("192.0.0.0"), std::string("/2")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("224.0.0.0"), std::string("/3")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("240.0.0.0"), std::string("/4")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("248.0.0.0"), std::string("/5")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("252.0.0.0"), std::string("/6")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("254.0.0.0"), std::string("/7")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.0.0.0"), std::string("/8")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.128.0.0"), std::string("/9")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.192.0.0"), std::string("/10")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.224.0.0"), std::string("/11")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.240.0.0"), std::string("/12")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.248.0.0"), std::string("/13")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.252.0.0"), std::string("/14")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.254.0.0"), std::string("/15")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.0.0"), std::string("/16")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.128.0"), std::string("/17")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.192.0"), std::string("/18")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.224.0"), std::string("/19")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.240.0"), std::string("/20")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.248.0"), std::string("/21")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.252.0"), std::string("/22")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.254.0"), std::string("/23")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.0"), std::string("/24")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.128"), std::string("/25")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.192"), std::string("/26")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.224"), std::string("/27")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.240"), std::string("/28")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.248"), std::string("/29")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.252"), std::string("/30")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.254"), std::string("/31")));
    netmask_to_prefix->insert(std::make_pair(StringToNetworkAddress("255.255.255.255"), std::string("/32")));
}


std::string NetmaskToIpPrefix(const in_addr_t netmask) {
    static std::unordered_map<in_addr_t, std::string> netmask_to_prefix;
    if (unlikely(netmask_to_prefix.empty()))
        PopulateNetmaskToPrefixTable(&netmask_to_prefix);
    std::unordered_map<in_addr_t, std::string>::const_iterator netmask_and_prefix(netmask_to_prefix.find(netmask));
    if (netmask_and_prefix == netmask_to_prefix.end())
        return "";
    return netmask_and_prefix->second;
}


} // unnamed namespace


std::string NetworkAddressAndMaskToString(const in_addr_t network_address, const in_addr_t netmask) {
    std::string address, ip_prefix;
    if (not NetworkAddressToString(network_address, &address))
        throw std::runtime_error("in NetUtil::NetworkAddressAndMaskToString: invalid network address("
                                 + std::to_string(network_address) + ")");

    if (netmask == 0)
        ip_prefix = "/0";
    else
        ip_prefix = NetmaskToIpPrefix(netmask);

    if (ip_prefix == "")
        throw std::runtime_error("in NetUtil::NetworkAddressAndMaskToString: invalid netmask(" + StringUtil::ToString(netmask)
                                 + ")");

    return address + ip_prefix;
}


namespace {


#ifndef __linux__
#      error "You need to implement a new method of finding the list of network interfaces!"
#endif


void GetInterfaces(std::list<ifreq> * const interface_requests) {
    interface_requests->clear();

    const int sock_fd = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock_fd == -1)
        throw std::runtime_error("in GetInterfaces: can't get IPv4 socket!");

    // Loop until we've allocated struct ifreq's for all interfaces:
    unsigned no_of_interface_requests(10);
    ifconf ifc;
    for (;;) {
        const int bufsize(static_cast<int>(no_of_interface_requests * sizeof(ifreq)));
        ifc.ifc_len = bufsize;
        ifc.ifc_buf = new char[bufsize];

        if (::ioctl(sock_fd, SIOCGIFCONF, &ifc) != 0) {
            delete [] ifc.ifc_buf;
            throw std::runtime_error("in GetInterfaces: ioctl(2) failed (" + std::to_string(errno) + ")!");
        }

        if (ifc.ifc_len == bufsize) { // We possibly need room for more ifreq's.
            delete [] ifc.ifc_buf;
            no_of_interface_requests *= 2; // Double the number of request buffers:
            continue;
        }

        break;
    }

    ifreq *interface_request = ifc.ifc_req;
    for (ssize_t i = 0; i < ifc.ifc_len; i += sizeof(ifreq)) {
        interface_requests->push_back(*interface_request);
        interface_request++;
    }

    delete [] ifc.ifc_buf;
}


} // unnamed namespace


void GetLocalIPv4Addrs(std::list<in_addr_t> * const ip_addresses) {
    ip_addresses->clear();

    std::list<ifreq> interface_requests;
    GetInterfaces(&interface_requests);

    for (std::list<ifreq>::const_iterator ifreq(interface_requests.begin()); ifreq != interface_requests.end(); ++ifreq)
        ip_addresses->push_back(reinterpret_cast<const sockaddr_in *>(&ifreq->ifr_ifru.ifru_addr)->sin_addr.s_addr);
}


namespace {


// The table of gTLD's and ".arpa".
const char * const g_tlds[] = {
    "com",
    "edu",
    "gov",
    "int",
    "mil",
    "net",
    "org",
    "biz",
    "info",
    "name",
    "pro",
    "aero",
    "coop",
    "museum",
    "museum",
    "arpa",
};


const struct {
        char country_code_[2 + 1];
        unsigned pseudo_tld_label_count_;
} cc_tld_exceptions[] = {
        { "de", 1 },  // Germany dosn't use ".com." or ".co." or anything like GB (".co.uk" or ".ac.uk") or similar.
};


} // unnamed namespace


std::string GetQuasiTopLevelDomainName(const std::string &domain_name) {
    std::list<std::string> labels;
    StringUtil::SplitThenTrim(StringUtil::ToLower(domain_name), ".", " \t\n\r\v", &labels);
    if (unlikely(labels.empty()))
        return "";

    // Compare against the gTLD's and ".arpa":
    for (unsigned i = 0; i < DIM(g_tlds); ++i) {
        if (labels.back() == g_tlds[i])
            return g_tlds[i];
    }

    // Make sure we're dealing with a possible ccTLD:
    if (labels.back().length() != 2)
        return "";

    for (unsigned i = 0; i < DIM(cc_tld_exceptions); ++i) {
        if (labels.back() == cc_tld_exceptions[i].country_code_) {
            if (labels.size() < cc_tld_exceptions[i].pseudo_tld_label_count_)
                return "";
            std::string pseudo_tld;
            std::list<std::string>::const_reverse_iterator label(labels.rbegin());
            for (unsigned k = 0; k < cc_tld_exceptions[i].pseudo_tld_label_count_; ++k) {
                if (k > 0)
                    pseudo_tld = "." + pseudo_tld;
                pseudo_tld = *label + pseudo_tld;
            }

            return pseudo_tld;
        }
    }

    // Assume all other ccTLD's behave like ".ac.uk" etc.:
    if (labels.size() < 2)
        return "";

    std::list<std::string>::const_reverse_iterator label(labels.rbegin());
    const std::string last_label(*label);
    ++label;
    return *label + "." + last_label;
}


bool GetPeerIpAddressFromSocket(const int socket_fd, in_addr_t * const ip_address) {
    sockaddr peeraddr;
    socklen_t addrlen;
    if (0 != ::getpeername(socket_fd, &peeraddr, &addrlen))
        return false;

    if (peeraddr.sa_family != AF_INET)
        return false;

    sockaddr_in *peeraddr_in((sockaddr_in *)&peeraddr);
    *ip_address = peeraddr_in->sin_addr.s_addr;
    return true;
}


} // namespace NetUtil
