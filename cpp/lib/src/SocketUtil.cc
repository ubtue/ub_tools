/** \file    SocketUtil.cc
 *  \brief   Implementation of Unix socket related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Artur Kedzierski
 *  \author  Roger Gabriel
 *  \author  Jiangtao Hu
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

#define _USE_BSD // To get "timersub" from <sys/time.h>
#include "SocketUtil.h"
#include <cassert>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "DnsUtil.h"
#include "FileDescriptor.h"
#include "StringUtil.h"
#include "SslConnection.h"
#include "TimerUtil.h"


namespace {


extern "C" void ConnectAlarm(int /*signal_no*/) {
    // just interrupt the connect
    return;
}


} // unnames namespace


typedef void (*SignalHandler)(int);


namespace SocketUtil {


bool StringToAddress(const std::string &domainname, const TimeLimit &time_limit, in_addr_t * const inet_addr,
                     std::string * const error_message, unsigned number_of_retries)
{
    bool resolved_address = false;
    unsigned number_of_attempts = 0;

    while (not resolved_address) {
        // Attempt to resolve the domain name:
        std::string get_host_error_message;
        resolved_address = DnsUtil::TimedGetHostByName(domainname, time_limit, inet_addr, &get_host_error_message);
        ++number_of_attempts;

        // Successfully looked up host:
        if (resolved_address)
            return true;

        // Error looking up host and a timeout occurred:
        else if (time_limit.limitExceeded()) {
            *error_message = "in SocketUtil::StringToAddress: TimedGetHostByName(\"" + domainname + "\") timed out on attempt "
                + StringUtil::ToString(number_of_attempts) + ": " + get_host_error_message + "!";
            return false;
        }

        // Error looking up host and we've exceeded our retry limit:
        else if (number_of_attempts >= number_of_retries) {
            *error_message = "in SocketUtil::StringToAddress: TimedGetHostByName(\"" + domainname + "\") failed ("
                + StringUtil::ToString(number_of_attempts) + " attempts): " + get_host_error_message + "!";
            errno = ENXIO;
            return false;
        }

        // Error looking upo host, but we'll sleep then try again:
        else
            ::usleep(std::min(200u, time_limit.getRemainingTime()));
    }

    // We should never get here:
    throw std::runtime_error("in SocketUtil::StringToAddress: undefined state.");
}


int TcpConnect(const in_addr_t address, const unsigned short port, const TimeLimit &time_limit,
               std::string * const error_message, const NagleOptionType nagle_option,
               const ReuseAddrOptionType reuse_addr_option)
{
    error_message->clear();

    FileDescriptor socket_fd(::socket(AF_INET, SOCK_STREAM, 0));
    if (unlikely(not socket_fd.isValid())) {
        *error_message = "socket(2) failed (" + std::to_string(errno) + ")!";
        return -1;
    }

    // Initialise the address of the server we're connecting to:
    struct sockaddr_in server_address;
    std::memset(&server_address, '\0', sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = address;

    TimerUtil::SaveAndRestorePendingTimer save_and_restore_pending_timer;

    if (reuse_addr_option == REUSE_ADDR) {
        // Enable rapid reuse of ports:
        const int reuse_addr_flag = 1;
        if (::setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_flag, sizeof(reuse_addr_flag)) != 0) {
            *error_message = "setsockopt(2) failed for SO_REUSEADDR (" + std::to_string(errno) + ")!";
            return -1;
        }
    }

    if (nagle_option == DISABLE_NAGLE) {
        // Turn off Nagle's algorithm:
        const int no_delay_flag = 1;
        if (::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &no_delay_flag, sizeof(no_delay_flag)) != 0) {
            *error_message = "setsockopt(2) failed for SO_REUSEADDR (" + std::to_string(errno)
                + ")!";
            return -1;
        }
    }


    struct sigaction new_action;

    /* set up alarm */
    new_action.sa_handler = ConnectAlarm;
    sigemptyset(&new_action.sa_mask);
#ifdef SA_INTERRUPT
    new_action.sa_flags = SA_INTERRUPT;
#else
    new_action.sa_flags = 0;
#endif
    if (::sigaction(SIGALRM, &new_action, nullptr) < 0) {
        *error_message = "sigaction(2) failed to set the new signal handler ("
            + std::to_string(errno) + ")!";
        return -1;
    }

    // Get any time left on a possible previous alarm clock:
    unsigned remaining_time_on_old_clock = save_and_restore_pending_timer.getRemainingTimeOnPendingTimer();

    // Wind up the clock:
    TimerUtil::malarm(std::min(time_limit.getRemainingTime(), remaining_time_on_old_clock));

    if (::connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) {
        if (errno == EINTR)
            errno = ETIMEDOUT;

        *error_message = "connect(2) failed ("+ std::to_string(errno) + ")!";
        socket_fd = -1;
    }

    // Turn off alarm if it did not go off:
    TimerUtil::malarm(0);

    struct timeval end_time;
    ::gettimeofday(&end_time, nullptr);

    // Alarm took too long?
    if (errno == ETIMEDOUT) { // Yes, timed out.
        *error_message = "TcpConnect timed out.";
        return -1;
    }

    return socket_fd.release();
}


int TcpConnect(const std::string &address, const unsigned short port, const TimeLimit &time_limit,
               std::string * const error_message, const NagleOptionType nagle_option,
               const ReuseAddrOptionType reuse_addr_option)
{
    std::string string_to_address_error_message;
    in_addr_t server_address;
    if (not StringToAddress(address, time_limit, &server_address, &string_to_address_error_message, 0 /* retries */)) {
        *error_message = "SocketUtil::StringToAddress reported: " + string_to_address_error_message;
        return -1;
    }

    return TcpConnect(server_address, port, time_limit, error_message, nagle_option, reuse_addr_option);
}


ssize_t TimedRead(int socket_fd, const TimeLimit &time_limit, void * const data, size_t data_size,
                  SslConnection * const ssl_connection)
{
    struct timeval select_timeout;
#ifdef __linux__
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
 select_again:
#else
 select_again:
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
#endif
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_fd, &read_set);
    switch (::select(socket_fd + 1, &read_set, nullptr, nullptr, &select_timeout)) {
    case -1:
        if (errno == EINTR)
            goto select_again;
        return -1;
    case 0:
        errno = ETIMEDOUT;
        return -1;
    }

    if (unlikely(data_size == 0))
        return 0;

 read_again:
    ssize_t ret_val;
    if (ssl_connection == nullptr) { // We have a non-SSL connection.
        ret_val = ::read(socket_fd, data, data_size);
        if (ret_val < 0) {
            if (errno == EINTR)
                goto read_again;
            if (errno == EWOULDBLOCK or errno == EAGAIN)
                goto select_again;
            return -1;
        }
    } else { // We have an SSL connection.
        ret_val = ssl_connection->read(data, data_size);
        if (ret_val < 0) {
            switch (ssl_connection->getLastErrorCode()) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // This is the EWOULDBLOCK equivalent.
                goto select_again;
            case SSL_ERROR_SYSCALL: // SSL has already set errno for us.
                if (errno == EWOULDBLOCK or errno == EAGAIN)
                    goto select_again;
                return -1;
            default: // A real SSL error.
                errno = EPROTO;
                return -1;
            }
        }
    }

    return ret_val;
}


bool TimedRead(int socket_fd, const TimeLimit &time_limit, std::string * const s,
               SslConnection * const ssl_connection)
{
    s->clear();

    ssize_t retval;
    do {
        char buf[10240];
        retval = TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection);
        if (retval > 0)
            s->append(buf, retval);
    } while (not time_limit.limitExceeded() and retval > 0);

    return retval >= 0;
}


ssize_t TimedWrite(int socket_fd, const TimeLimit &time_limit, const void * const data, size_t data_size,
                   SslConnection * const ssl_connection)
{
    struct timeval select_timeout;
#ifdef __linux__
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
 select_again:
#else
 select_again:
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
#endif
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_fd, &write_set);
    switch (::select(socket_fd + 1, nullptr, &write_set, nullptr, &select_timeout)) {
    case -1:
        if (errno == EINTR)
            goto select_again;
        return -1;
    case 0:
        errno = ETIMEDOUT;
        return -1;
    }
    assert(FD_ISSET(socket_fd, &write_set));

 write_again:
    ssize_t ret_val;
    if (ssl_connection != nullptr) {
        ret_val = ssl_connection->write(data, data_size);
        if (ret_val < 0) {
            switch (ssl_connection->getLastErrorCode()) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // This is the EWOULDBLOCK equivalent.
                goto select_again;
            case SSL_ERROR_SYSCALL: // SSL has already set errno for us.
                ret_val = -1;
                break;
            default: // A real SSL error.
                errno = EPROTO;
                ret_val = -1;
            }
        }
    }
    else { // We have a regular, non-SSL connection.
        ret_val = ::write(socket_fd, data, data_size);
        if (ret_val < 0) {
            if (errno == EINTR)
                goto write_again;
            if (errno == EWOULDBLOCK)
                goto select_again;
            ret_val = -1;
        }
    }

    return ret_val;
}


ssize_t TimedRecvFrom(const int socket_fd, const TimeLimit &time_limit, void * const data, const size_t data_size,
                      struct sockaddr_in *from, const int flags)
{
    struct timeval select_timeout;
#ifdef __linux__
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
 select_again:
#else
 select_again:
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
#endif
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(socket_fd, &read_set);
    switch (::select(socket_fd + 1, &read_set, nullptr, nullptr, &select_timeout)) {
    case -1:
        if (errno == EINTR)
            goto select_again;
        return -1;
    case 0:
        errno = ETIMEDOUT;
        return -1;
    }
    assert(FD_ISSET(socket_fd, &read_set));

 receive_again:
    socklen_t addr_len = sizeof(struct sockaddr_in);
    ssize_t ret_val = ::recvfrom(socket_fd, data, data_size, flags, reinterpret_cast<struct sockaddr *>(from),
                                 &addr_len);
    if (ret_val < 0) {
        if (errno == EINTR)
            goto receive_again;
        if (errno == EWOULDBLOCK)
            goto select_again;
        ret_val = -1;
    }

    return ret_val;
}


ssize_t TimedSendTo(const int socket_fd, const TimeLimit &time_limit, const void * const data, const size_t data_size,
                    const struct sockaddr_in &to, const int flags)
{
    struct timeval select_timeout;
#ifdef __linux__
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
 select_again:
#else
 select_again:
    MillisecondsToTimeVal(time_limit.getRemainingTime(), &select_timeout);
#endif
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_fd, &write_set);
    switch (::select(socket_fd + 1, nullptr, &write_set, nullptr, &select_timeout)) {
    case -1:
        if (errno == EINTR)
            goto select_again;
        return -1;
    case 0:
        errno = ETIMEDOUT;
        return -1;
    }
    assert(FD_ISSET(socket_fd, &write_set));

 send_again:
    ssize_t ret_val = ::sendto(socket_fd, data, data_size, flags, reinterpret_cast<const struct sockaddr *>(&to),
                               sizeof(struct sockaddr));
    if (ret_val < 0) {
        if (errno == EINTR)
            goto send_again;
        if (errno == EWOULDBLOCK)
            goto select_again;
        ret_val = -1;
    }

    return ret_val;
}


bool SendUdpRequest(const int socket_fd, const in_addr_t server_ip_address, const uint16_t port_no,
                    const unsigned char * const packet, const unsigned packet_size)
{
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_no);
    server_addr.sin_addr.s_addr = server_ip_address;
    std::memset(&server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));
    return ::sendto(socket_fd, packet, packet_size, 0, reinterpret_cast<const struct sockaddr *>(&server_addr),
                    sizeof server_addr) != -1;
}


} // namespace SocketUtil
