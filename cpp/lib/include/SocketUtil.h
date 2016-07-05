/** \file    SocketUtil.h
 *  \brief   Unix socket related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2007 Project iVia.
 *  Copyright 2002-2007 The Regents of The University of California.
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

#include <stdexcept>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include "TimeLimit.h"


// forward declaration(s):
class SslConnection;


/** \namespace  SocketUtil
 *  \brief      Unix socket related utility functions.
 *
 *  See accompanying SocketUtilTest program for example usage.
 */
namespace SocketUtil {


/** \brief  Converts a string to an Internet address.
 *  \param  address            The hostname or IP address.
 *  \param  time_limit         Timeout in milliseconds.
 *  \param  inet_addr          The internal IP address representation in host byte order if successful.
 *  \param  error_message      If an error occurred an explanation will be found here.
 *  \param  number_of_retries  Number of times to retry server if gethostbyname fails.
 *  \return True if the lookup succeeds otherwise false.
 *
 *  \note   This function caches the last successful lookup, so may not reflect recent changes to DNS.
 */
bool StringToAddress(const std::string &address, const TimeLimit &time_limit, in_addr_t * const inet_addr,
                     std::string * const error_message, unsigned number_of_retries = 0);


/** \brief  Converts a string to an Internet address.
 *  \param  address            The hostname or IP address.
 *  \param  inet_addr          The internal IP address representation if successful.
 *  \param  number_of_retries  Number of retries to attempt in when gethostbyname
 *
 *  \note    This function caches the last successful lookup, so may not reflect recent changes to DNS.
 */
inline void StringToAddress(const std::string &address, in_addr_t * const inet_addr,
                            unsigned number_of_retries = 0)
{
        std::string error_message;
        if (not StringToAddress(address, TimeLimit(15000), inet_addr, &error_message, number_of_retries))
            throw std::runtime_error("in SocketUtil::StringToAddress: " + error_message);
}


enum NagleOptionType { USE_NAGLE, DISABLE_NAGLE };
enum ReuseAddrOptionType { DONT_REUSE_ADDR, REUSE_ADDR };


/** \brief  Creates and connects a TCP socket.
 *  \param  address            The IP address to connect to.
 *  \param  port               The TCP port number.
 *  \param  time_limit         Timeout in milliseconds.
 *  \param  error_message      The resulting error message (if any) from the connection attempt
 *  \param  nagle_option       Enables of disables nagleing on the socket.
 *  \param  reuse_addr_option  Optionally sets SO_REUSEADDR on the newly created socket if requested.
 *  \return -1 on error or a valid socket file descriptor (sets errno or h_errno on most errors).
 */
int TcpConnect(const in_addr_t address, const unsigned short port, const TimeLimit &time_limit,
               std::string * const error_message, const NagleOptionType nagle_option = USE_NAGLE,
               const ReuseAddrOptionType reuse_addr_option = DONT_REUSE_ADDR);


/** \brief  Creates and connects a TCP socket.
 *  \param  address            The hostname or IP address to connect to.
 *  \param  port               The TCP port number.
 *  \param  time_limit         Timeout in milliseconds.
 *  \param  error_message      The resulting error message (if any) from the connection attempt
 *  \param  nagle_option       Enables of disables nagleing on the socket.
 *  \param  reuse_addr_option  Optionally sets SO_REUSEADDR on the newly created socket if requested.
 *  \return -1 on error or a valid socket file descriptor (sets errno or h_errno on most errors).
 */
int TcpConnect(const std::string &address, const unsigned short port, const TimeLimit &time_limit,
               std::string * const error_message, const NagleOptionType nagle_option = USE_NAGLE,
               const ReuseAddrOptionType reuse_addr_option = DONT_REUSE_ADDR);


/** \brief  Allows reading from a socket file descriptor with a given time limit.
 *  \param  socket_fd       The socket to read from.
 *  \param  time_limit      Timeout in milliseconds.
 *  \param  data            Where to put the read data.
 *  \param  data_size       Up to how much to read.
 *  \param  ssl_connection  A pointer to an SSL connection object.  If not null, this will be used automatically.
 *  \return -1 if a timeout or other error occurred, otherwise the number of bytes read.  You can distinguish a timeout
 *          from other error conditions by checking the value of errno.  When a timeout has occurred, errno will be set
 *          to ETIMEOUT.
 */
ssize_t TimedRead(int socket_fd, const TimeLimit &time_limit, void * const data, size_t data_size,
                  SslConnection * const ssl_connection = nullptr);


/** \brief  Allows reading from a socket file descriptor with a given time limit.
 *  \param  socket_fd       The socket to read from.
 *  \param  time_limit      Timeout in milliseconds.
 *  \param  s               Where to put the read data.
 *  \param  ssl_connection  A pointer to an SSL connection object.  If not null, this will be used automatically.
 *  \return True if EOF was found, otherwise false.
 */
bool TimedRead(int socket_fd, const TimeLimit &time_limit, std::string * const s,
               SslConnection * const ssl_connection = nullptr);


/** \brief  Allows writing to a file descriptor with a given time limit.
 *  \param  socket_fd       The file descriptor to write to.
 *  \param  time_limit      Timeout in milliseconds.
 *  \param  data            The data to write.
 *  \param  data_size       Up to how much to write.
 *  \param  ssl_connection  A pointer to an SSL connection object.  If not null, this will be used automatically.
 *  \return  -1 if a timeout or other error occurred, otherwise the number of bytes
 *           written.
 */
ssize_t TimedWrite(int socket_fd, const TimeLimit &time_limit, const void * const data, size_t data_size,
                   SslConnection * const ssl_connection = nullptr);


/** \brief  Allows writing to a file descriptor with a given time limit.
 *  \param  socket_fd       The file descriptor to write to.
 *  \param  time_limit      Timeout in milliseconds.
 *  \param  data            The data to write.
 *  \param  ssl_connection  A pointer to an SSL connection object.  If not null, this will be used automatically.
 *  \return  -1 if a timeout or other error occurred, otherwise the number of bytes
 *           written.
 */
inline ssize_t TimedWrite(int socket_fd, const TimeLimit &time_limit, const std::string &data,
                          SslConnection * const ssl_connection = nullptr)
{
        return TimedWrite(socket_fd, time_limit, data.c_str(), data.size(), ssl_connection);
}


/** \brief  A wrapper around recvfrom(2) for receiving datagrams over a TCP/IPv4 network, allowing for a timeout.
 *  \param  socket_fd   The file descriptor to read from.
 *  \param  time_limit  Timeout in milliseconds.
 *  \param  data        Where to put the read data.
 *  \param  data_size   Up to how much to read.
 *  \param  from        The sender's address.
 *  \param  flags       See recvfrom(2) for flags which can be or'ed together.
 *  \return  -1 if a timeout or other error occurred, otherwise the number of bytes received.
 */
ssize_t TimedRecvFrom(const int socket_fd, const TimeLimit &time_limit, void * const data, const size_t data_size,
                      struct sockaddr_in *from, const int flags = 0);


/** \brief  A wrapper around sendto(2) for sending datagrams over a TCP/IPv4 network, allowing for a timeout.
 *  \param  socket_fd   The file descriptor to write to.
 *  \param  time_limit  Timeout in milliseconds.
 *  \param  data        The data to write.
 *  \param  data_size   How much to write.
 *  \param  to          The destination address.
 *  \param  flags       See sendto(2) for flags which can be or'ed together.
 *  \return  -1 if a timeout or other error occurred, otherwise the number of bytes sent.
 *  \note    If you're sending UDP datagrams, they are limited to 64 KiB in size.
 */
ssize_t TimedSendTo(const int socket_fd, const TimeLimit &time_limit, const void * const data, const size_t data_size,
                    const struct sockaddr_in &to, const int flags = 0);


/** \brief  Sends a UDP request encoded in "packet" to "server_ip_address".
 *  \param  socket_fd          The connectionless socket to send the request on.  Typically this socket should be set up
 *                             socket(PF_INET, SOCK_DGRAM, 0) follwed by a call to FileUtil::SetNonblocking().
 *  \param  server_ip_address  The IP address of the server.
 *  \param  port_no            The port number of the server.
 *  \param  packet             The initialised DNS request packet to send.
 *  \param  packet_size        The size of the DNS request packet.
 *  \return True on success and false on failure.
 *  \note   If an error occurrs, errno will be set to the appropriate error code.
 */
bool SendUdpRequest(const int socket_fd, const in_addr_t server_ip_address, const uint16_t port_no,
                    const unsigned char * const packet, const unsigned packet_size);


} // namespace SocketUtil



