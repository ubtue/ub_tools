/** \file    NetUtil.h
 *  \brief   Declaration of network-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#ifndef NETUTIL_H
#define NETUTIL_H


#include <fstream>
#include <list>
#include <string>
#include <arpa/inet.h>
#include "SList.h"


namespace NetUtil {


/** \brief  Expects strings of the form 138.23.0.0 which get parsed into a network address.
 *  \param  s                Hopefully the network/bits string.
 *  \param  network_address  Where to store the network address represented by "s" if all goes well.
 *  \return True on success and false upon failure.
 */
bool StringToNetworkAddress(const std::string &s, in_addr_t * const network_address);


/** \brief  Expects strings of the form 138.23.0.0 which get parsed into a network address.
 *  \param  s                Hopefully the network/bits string.
 *  \return The IPv4 address corresponding.
 *  \note   Throws an exception if "s" does not represent a valid IPv4 address.
 */
in_addr_t StringToNetworkAddress(const std::string &s);


/** \brief  Expects strings of the form 138.23.0.0/16 which get parsed into
 *          a network address and a netmask.
 *  \param  s                Hopefully the network/bits string.
 *  \param  network_address  Returns the network address part of "s" if all goes well.
 *  \param  netmask          Returns the netmask part of "s" if all goes well.
 *  \return True on success and false upon failure.
 */
bool StringToNetworkAddressAndMask(const std::string &s, in_addr_t * const network_address, in_addr_t * const netmask);

/** \brief  Converts an IP address to a string.
 *  \param  network_address  The network address to be converted.
 *  \param  s                Where to store the dotted quad.
 *  \return True on success and false upon failure.
 */
bool NetworkAddressToString(const in_addr_t network_address, std::string * const s);

/** \brief   Converts an IP address to a string.
 *  \param   network_address  The network address to be converted.
 *  \return  The network address on success and throws an Exception on failure.
 */
std::string NetworkAddressToString(const in_addr_t network_address);


/** \brief   Converts an IP address and net mask to a string, e.g. "192.168.0.0/16"
 *  \param   network_address  The network address to be converted.
 *  \param   netmask          The netmask to convert.
 *  \return  The network address on success and throws an Exception on failure.
 */
std::string NetworkAddressAndMaskToString(const in_addr_t network_address, const in_addr_t netmask);


template<typename Data> struct NetAddrBlockAndData {
    in_addr_t network_address_, mask_;
    Data data_;
public:
    NetAddrBlockAndData(const in_addr_t network_address, const in_addr_t mask, const Data &data)
        : network_address_(network_address & mask), mask_(mask), data_(data)
    { }
};


template<typename Data> std::ostream &operator<<(std::ostream &output,
                                                 const NetAddrBlockAndData<Data> &net_addr_block_and_data)
{
    std::string temp;
    NetworkAddressToString(net_addr_block_and_data.network_address_, &temp);
    output << "[" << temp << ',';
    NetworkAddressToString(net_addr_block_and_data.mask_, &temp);
    output << temp << ',' << net_addr_block_and_data.data_ << ']';
    return output;
}


template<typename Data> class NetAddrBlocksAndData: public SList< NetAddrBlockAndData<Data> > {
public:
    Data getSelection(const in_addr_t address, const Data &default_data) const;
};


template<typename Data> Data NetAddrBlocksAndData<Data>::getSelection(const in_addr_t address, const Data &default_data) const {
    for (const auto &net_addr_block_and_data : *this) {
        if ((address & net_addr_block_and_data.mask_) ==
            (net_addr_block_and_data.network_address_ & net_addr_block_and_data.mask_))
            return net_addr_block_and_data.data_;
    }

    return default_data;
}


template<typename Data> std::ostream &operator<<(std::ostream &output,
                                                 const NetAddrBlocksAndData<Data> &net_addr_blocks_and_data)
{
    for (const auto &netaddr_block_and_data : net_addr_blocks_and_data)
        output << netaddr_block_and_data << '\n';

    return output;
}


/** Retrieves the list of IPv4 addresses for the machine we're on. */
void GetLocalIPv4Addrs(std::list<in_addr_t> * const ip_addresses);


/** \brief  Given a domain name, returns the quasi top-level domain name.
 *  \param  domain_name  The domain name whose quasi top-level domain name we want.
 *  \return The quasi top-level domain name if we can determine it, otherwise false.
 *  \note   For all gTLD's like ".mil", ".com" or ".int" and ".arpa", we return the gTLD or ".arpa".  For ccTLD's we special case based on country.  An
 *          example would be ".com.br" as a quasi top-level domain for Brazil.  Since we don't have the resources we only handle very few countries.  You
 *          can easily add more.
 */
std::string GetQuasiTopLevelDomainName(const std::string &domain_name);


/** \breif  Get the peer ip address if possible.
 *  \param  socket_fd  The socket from which to retrieve the peer ip.
 *  \param  ip_address  The output ip address
 *  \return true if ip address was retrieved, false otherwise
 */
bool GetPeerIpAddressFromSocket(const int socket_fd, in_addr_t * const ip_address);


} // namespace NetUtil


#endif
