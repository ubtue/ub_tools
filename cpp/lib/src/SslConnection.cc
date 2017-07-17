/** \file    SslConnection.h
 *  \brief   Implementation class SslConnection.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2006-2008 Project iVia.
 *  \copyright 2006-2008 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "SslConnection.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include "Compiler.h"
#include "TimeUtil.h"


std::mutex SslConnection::mutex_;
bool SslConnection::ssl_libraries_have_been_initialised_;
std::list<SslConnection::ContextInfo> SslConnection::context_infos_;


SslConnection::SslConnection(const int fd, const Method method, const ClientServerMode client_server_mode,
                             const ThreadingSupportMode threading_support_mode)
        : threading_support_mode_(threading_support_mode), ssl_connection_(nullptr), last_ret_val_(0)
{
    std::unique_ptr<std::lock_guard<std::mutex>> mutex_locker;
    if (threading_support_mode == SUPPORT_MULTITHREADING)
        mutex_locker.reset(new std::lock_guard<std::mutex>(SslConnection::mutex_));

    ssl_context_ = SslConnection::InitContext(method, client_server_mode);

    ssl_connection_ = ::SSL_new(ssl_context_);
    if (unlikely(ssl_connection_ == nullptr))
        throw std::runtime_error("in SslConnection::SslConnection: ::SSL_new() failed!");

    if (unlikely(not ::SSL_set_fd(ssl_connection_, fd)))
        throw std::runtime_error("in SslConnection::SslConnection: ::SSL_set_fd() failed!");

    const unsigned NO_OF_TRIES(10);
    for (unsigned try_no(0); try_no < NO_OF_TRIES; ++try_no) {
        const int ret_val(::SSL_connect(ssl_connection_));
        if (unlikely(ret_val == 0))
            throw std::runtime_error("in SslConnection::SslConnection: ::SSL_connect() failed with return "
                                     "value (0)!");
        if (ret_val == 1) // We succeeded.
            return;

        TimeUtil::Millisleep(300);
    }

    throw std::runtime_error("in SslConnection::SslConnection: ::SSL_connect() failed after "
                             + std::to_string(NO_OF_TRIES) + " tries!");
}


SslConnection::~SslConnection() {
    // Several of the functions called here throw exceptions which could cause an unexpected exception if called while
    // an exception is already in progress, destroying this object. So, don't clean up if this is the case.
    if (not std::uncaught_exception()) {
        std::unique_ptr<std::lock_guard<std::mutex>> mutex_locker;
        if (threading_support_mode_ == SUPPORT_MULTITHREADING)
            mutex_locker.reset(new std::lock_guard<std::mutex>(SslConnection::mutex_));

        if (ssl_connection_ != nullptr)
            ::SSL_free(ssl_connection_);
        SslConnection::ReleaseContext(ssl_context_);
    }
}


ssize_t SslConnection::read(void * const data, size_t data_size) {
    std::unique_ptr<std::lock_guard<std::mutex>> mutex_locker;
    if (threading_support_mode_ == SUPPORT_MULTITHREADING)
        mutex_locker.reset(new std::lock_guard<std::mutex>(SslConnection::mutex_));

        return last_ret_val_ = ::SSL_read(ssl_connection_, data, data_size);
}


ssize_t SslConnection::write(const void * const data, size_t data_size) {
    std::unique_ptr<std::lock_guard<std::mutex>> mutex_locker;
    if (threading_support_mode_ == SUPPORT_MULTITHREADING)
        mutex_locker.reset(new std::lock_guard<std::mutex>(SslConnection::mutex_));

    return last_ret_val_ = ::SSL_write(ssl_connection_, data, data_size);
}


int SslConnection::getLastErrorCode() const {
    std::unique_ptr<std::lock_guard<std::mutex>> mutex_locker;
    if (threading_support_mode_ == SUPPORT_MULTITHREADING)
        mutex_locker.reset(new std::lock_guard<std::mutex>(SslConnection::mutex_));

    return ::SSL_get_error(ssl_connection_, last_ret_val_);
}


namespace {


class ContextInfoMatch {
    const SslConnection::Method method_;
    const SslConnection::ClientServerMode client_server_mode_;
public:
    explicit ContextInfoMatch(const SslConnection::Method method,
                              const SslConnection::ClientServerMode client_server_mode)
        : method_(method), client_server_mode_(client_server_mode) { }
    bool operator()(const SslConnection::ContextInfo &rhs) const;
};


bool ContextInfoMatch::operator()(const SslConnection::ContextInfo &rhs) const {
    if (rhs.method_ != method_)
        return false;
    if (rhs.client_server_mode_ != client_server_mode_)
        return false;
    return true;
}


} // unnamed namespace


SSL_CTX *SslConnection::InitContext(const Method method, const ClientServerMode client_server_mode) {
    if (not ssl_libraries_have_been_initialised_) {
        ::SSL_load_error_strings();
        ::SSL_library_init();
        // We don't call ::RAND_add() because we assume the existence of /dev/urandom!

        ssl_libraries_have_been_initialised_ = true;
    }

    // First check to see if we already have a context with the correct options:
    std::list<ContextInfo>::iterator matching_context_info(std::find_if(context_infos_.begin(), context_infos_.end(),
                                                                        ContextInfoMatch(method,
                                                                                         client_server_mode)));
    if (matching_context_info != context_infos_.end()) {
        ++matching_context_info->usage_count_;
        return matching_context_info->ssl_context_;
    }

    // If we make it here we need to create a new ContextInfo:
    ContextInfo new_context_info(method, client_server_mode);
    switch (client_server_mode) {
    case CLIENT:
        new_context_info.ssl_context_ = SslConnection::InitClient(method);
        break;
    case SERVER:
        new_context_info.ssl_context_ = SslConnection::InitServer(method);
        break;
    case CLIENT_AND_SERVER:
        new_context_info.ssl_context_ = SslConnection::InitClientAndServer(method);
        break;
    default:
        throw std::runtime_error("in SslConnection::InitContext: unknown client/server mode!");
    }
    context_infos_.push_back(new_context_info);
    return new_context_info.ssl_context_;
}


namespace {


class ContextMatch {
    const SSL_CTX * const ssl_context_to_match_;
public:
    explicit ContextMatch(const SSL_CTX * const ssl_context_to_match): ssl_context_to_match_(ssl_context_to_match) { }
    bool operator()(const SslConnection::ContextInfo &rhs) const { return rhs.ssl_context_ == ssl_context_to_match_; }
};


} // unnamed namespace


void SslConnection::ReleaseContext(const SSL_CTX * const ssl_context) {
    std::list<ContextInfo>::iterator matching_context_info(
        std::find_if(context_infos_.begin(), context_infos_.end(), ContextMatch(ssl_context)));
    if (unlikely(matching_context_info == context_infos_.end()))
        throw std::runtime_error("in SslConnection::ReleaseContext: can't find context to release!");

    --matching_context_info->usage_count_;
    if (matching_context_info->usage_count_ == 0) {
        ::SSL_CTX_free(matching_context_info->ssl_context_);
        context_infos_.erase(matching_context_info);
    }
}


SSL_CTX *SslConnection::InitClient(const Method method) {
    const SSL_METHOD *ssl_method;

    switch (method) {
    case TLS_V1:
        ssl_method = ::TLSv1_client_method();
        break;
    case TLS_V1_1:
        ssl_method = ::TLSv1_1_client_method();
        break;
    case TLS_V1_2:
        ssl_method = ::TLSv1_2_client_method();
        break;
    case ALL_STREAM_METHODS:
        ssl_method = ::SSLv23_client_method();
        break;
    case DTLS_V1:
        ssl_method = ::DTLSv1_client_method();
        break;
    case DTLS_V1_2:
        ssl_method = ::DTLSv1_2_client_method();
        break;
    case ALL_DATAGRAM_METHODS:
        ssl_method = ::DTLS_client_method();
        break;
    default:
        throw std::runtime_error("in SslConnection::InitClient: unknown method!");
    }

    if (unlikely(ssl_method == nullptr))
        throw std::runtime_error("in SslConnection::InitClient: unexpected nullptr SSL method!");

    return ::SSL_CTX_new(ssl_method);
}


SSL_CTX *SslConnection::InitServer(const Method method) {
    const SSL_METHOD *ssl_method;

    switch (method) {
    case TLS_V1:
        ssl_method = ::TLSv1_server_method();
        break;
    case TLS_V1_1:
        ssl_method = ::TLSv1_1_server_method();
        break;
    case TLS_V1_2:
        ssl_method = ::TLSv1_2_server_method();
        break;
    case ALL_STREAM_METHODS:
        ssl_method = ::SSLv23_server_method();
        break;
    case DTLS_V1:
        ssl_method = ::DTLSv1_server_method();
        break;
    case DTLS_V1_2:
        ssl_method = ::DTLSv1_2_server_method();
        break;
    case ALL_DATAGRAM_METHODS:
        ssl_method = ::DTLS_server_method();
        break;
    default:
        throw std::runtime_error("in SslConnection::InitServer: unknown method!");
    }

    if (unlikely(ssl_method == nullptr))
        throw std::runtime_error("in SslConnection::InitServer: unexpected nullptr SSL method!");

    return ::SSL_CTX_new(ssl_method);
}


SSL_CTX *SslConnection::InitClientAndServer(const Method method) {
    const SSL_METHOD *ssl_method;

    switch (method) {
    case TLS_V1:
        ssl_method = ::TLSv1_method();
        break;
    case TLS_V1_1:
        ssl_method = ::TLSv1_1_method();
        break;
    case TLS_V1_2:
        ssl_method = ::TLSv1_2_method();
        break;
    case ALL_STREAM_METHODS:
        ssl_method = ::SSLv23_method();
        break;
    case DTLS_V1:
        ssl_method = ::DTLSv1_method();
        break;
    case DTLS_V1_2:
        ssl_method = ::DTLSv1_2_method();
        break;
    case ALL_DATAGRAM_METHODS:
        ssl_method = ::DTLS_method();
        break;
    default:
        throw std::runtime_error("in SslConnection::InitClientAndServer: unknown method!");
    }

    if (unlikely(ssl_method == nullptr))
        throw std::runtime_error("in SslConnection::InitClientAndServer: unexpected nullptr SSL method!");

    return ::SSL_CTX_new(ssl_method);
}
