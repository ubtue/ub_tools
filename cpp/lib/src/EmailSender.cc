/** \file   EmailSender.cc
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Artur Kedzierski
 *  \author Dr. Gordon W. Paynter
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.
 *  \copyright 2002-2008 Project iVia.
 *  \copyright 2002-2008 The Regents of The University of California.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "EmailSender.h"
#include <iostream>
#include <memory>
#include <list>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <openssl/bio.h>
#include "Compiler.h"
#include "DnsUtil.h"
#include "FileDescriptor.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "SocketUtil.h"
#include "SslConnection.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


#define NUL '\0'


namespace {


// For some reason we can't use the version in TextUtil even though that one also seems correct.
std::string Base64Encode(const std::string &s) {
    // Create BIO to perform base64 encoding:
    BIO * const b64(::BIO_new(::BIO_f_base64()));
    ::BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // Create BIO that holds the result:
    BIO * const mem(::BIO_new(::BIO_s_mem()));

    // Chain base64 with mem, so writing to b64 will encode base64 and write to mem:
    ::BIO_push(b64, mem);

    // Write data:
    bool done(false);
    while (not done) {
        const int res(::BIO_write(b64, s.data(), s.size()));
        if (res <= 0) {
            if (::BIO_should_retry(b64))
                continue;
            else {
                ::BIO_free_all(b64);
                throw std::runtime_error("in Base64Encode(EmailSender.cc): encoding error!");
            }
        } else // success!
            done = true;
    }

    BIO_flush(b64);

    char *data;
    const long len(::BIO_get_mem_data(mem, &data));

    // assign data to output
    std::string retval(data, len);

    ::BIO_free_all(b64);

    return retval;
}


bool perform_logging;


const std::string SMTP_CONFIG_PATH("/usr/local/var/lib/tuelib/cronjobs/smtp_server.conf");


// GetDateInRFC822Format()  -- returns current date and time in an RFC-822 compliant format.
//
std::string GetDateInRFC822Format() {
    const time_t now(std::time(nullptr));

    // Convert struct time to an RFC-822 formatted string:
    char date_string[100 + 1];
    std::strftime(date_string, sizeof(date_string), "%a, %e %b %Y %H:%M:%S %z", std::localtime(&now));

    return date_string;
}


static std::string smtp_server;


std::string GetSmtpServer() {
    if (smtp_server.empty()) {
        const IniFile ini_file(SMTP_CONFIG_PATH);
        smtp_server = ini_file.getString("SMTPServer", "server_address");
    }

    return smtp_server;
}


static std::string server_user;


std::string GetServerUser() {
    if (server_user.empty()) {
        const IniFile ini_file(SMTP_CONFIG_PATH);
        server_user = ini_file.getString("SMTPServer", "server_user");
    }

    return server_user;
}


static std::string server_password;


std::string GetServerPassword() {
    if (server_password.empty()) {
        const IniFile ini_file(SMTP_CONFIG_PATH);
        server_password = ini_file.getString("SMTPServer", "server_password");
    }

    return server_password;
}


void CheckResponse(const std::string &command, const std::string &server_response, const std::string &expected) {
    if (not StringUtil::Match(expected, server_response))
        throw std::runtime_error("in EmailSender.cc: did not receive expected server response to \"" + command
                                 + "\" but instead got \"" + server_response + "\"!");
}


std::string PerformExchange(const int socket_fd, const TimeLimit &time_limit, const std::string &command,
                            const std::string &expected_response_pattern,
                            SslConnection * const ssl_connection = nullptr)
{
    if (perform_logging)
        std::clog << "In PerformExchange: sending: " << command << '\n';
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, command + "\r\n", ssl_connection) == -1))
        throw std::runtime_error("in PerformExchange(EmailSender.cc) SocketUtil::TimedWrite failed! (sent: "
                                 + command + ", error: "+ std::string(strerror(errno)) + ")");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0)
        throw std::runtime_error("in PerformExchange(EmailSender.cc): Can't read SMTP server's response to \""
                                 + command + "\"! (" + std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    if (perform_logging)
        std::clog << "In PerformExchange: received: " << buf << '\n';
    CheckResponse(command, buf, expected_response_pattern);
    return buf;
}


std::string GetDotStuffedMessage(const std::string &message) {
    std::list<std::string> lines;
    StringUtil::SplitThenTrim(message, "\n", "\r", &lines, /* suppress_empty_words = */ false);
    for (auto &line : lines) {
        if (line == ".")
            line = "." + line;
    }

    std::string dot_stuffed_message(message);
    StringUtil::Join(lines, "\r\n", &dot_stuffed_message);

    return dot_stuffed_message;
}


std::string CreateEmailMessage(const EmailSender::Priority priority, const EmailSender::Format format,
                               const std::string &sender, const std::string &recipient, const std::string &subject,
                               const std::string &message_body, const std::string &cc = "")
{
    std::string message;
    message  = "Date: " + GetDateInRFC822Format() + "\r\n";
    message += "From: " + sender + "\r\n";
    message += "To: " + recipient + "\r\n";
    if (not cc.empty())
        message += "Cc: " + cc + "\r\n";
    message += "Subject: " + subject + "\r\n";
    if (format == EmailSender::PLAIN_TEXT)
        message += "Content-Type: text/plain; charset=\"utf-8\"\r\n";
    else
        message += "Content-Type: text/html; charset=\"utf-8\"\r\n";
    if (priority != EmailSender::DO_NOT_SET_PRIORITY)
        message += "X-Priority: " + std::to_string(priority) + "\r\n";

    message += "\r\n";
    message += GetDotStuffedMessage(message_body) + ".\r\n";

    return message;
}


} // unnamed namespace


namespace EmailSender {


void Authenticate(const int socket_fd, const TimeLimit &time_limit, SslConnection * const ssl_connection = nullptr) {
    std::string server_response(PerformExchange(socket_fd, time_limit, "AUTH LOGIN", "3[0-9][0-9]*", ssl_connection));
    if (perform_logging) {
        std::clog << "Decoded server response: " << TextUtil::Base64Decode(server_response.substr(4)) << '\n';
        std::clog << "Sending user name: " << server_user << '\n';
    }
    const std::string server_user(GetServerUser());
    server_response = PerformExchange(socket_fd, time_limit, Base64Encode(server_user), "3[0-9][0-9]*",
                                      ssl_connection);
    if (perform_logging) {
        std::clog << "Decoded server response: " << TextUtil::Base64Decode(server_response.substr(4)) << '\n';
        std::clog << "Sending server password: " << server_password << '\n';
    }
    const std::string server_password(GetServerPassword());
    PerformExchange(socket_fd, time_limit, Base64Encode(server_password), "2[0-9][0-9]*", ssl_connection);
}


bool SendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
               const std::string &message_body, const Priority priority, const Format format,
               const std::string &reply_to, const bool use_ssl, const bool use_authentication)
{
    if (unlikely(sender.empty() and reply_to.empty()))
        Error("in EmailSender::SendEmail: both \"sender\" and \"reply_to\" can't be empty!");

    const TimeLimit time_limit(10000 /* ms */);

    perform_logging = not MiscUtil::SafeGetEnv("ENABLE_SMPT_CLIENT_PERFORM_LOGGING").empty();

    // Open connection:
    const unsigned short PORT(use_ssl ? 587 : 25);
    std::string error_message;
    const FileDescriptor socket_fd(
        SocketUtil::TcpConnect(GetSmtpServer(), PORT, time_limit, &error_message, SocketUtil::DISABLE_NAGLE));
    if (socket_fd == -1) {
        throw std::runtime_error("in EmailSender::SendEmail: can't connect to SMTP server \"" + GetSmtpServer() + ":"
                + std::to_string(PORT) + " (" + error_message + ")!");
        return false;
    }

    // Read the welcome message:
    char buf[1000];
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) {
        Warning("in EmailSender::SendEmail: Can't read SMTP server's welcome message!");
        return false;
    }
    if (perform_logging) {
        buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
        std::clog << "Server sent: " << buf << '\n';
    }

    try {
        PerformExchange(socket_fd, time_limit, "EHLO " + DnsUtil::GetHostname(), "2[0-9][0-9]*");

        std::unique_ptr<SslConnection> ssl_connection;
        if (use_ssl) {
            PerformExchange(socket_fd, time_limit, "STARTTLS", "2[0-9][0-9]*");
            ssl_connection.reset(new SslConnection(socket_fd));
        }

        PerformExchange(socket_fd, time_limit, "EHLO " + DnsUtil::GetHostname(), "2[0-9][0-9]*",
                        ssl_connection.get());

        if (use_authentication)
            Authenticate(socket_fd, time_limit, ssl_connection.get());

        PerformExchange(socket_fd, time_limit, "MAIL FROM:<" + sender + ">", "2[0-9][0-9]*",
                        ssl_connection.get());

        // Send email to each recipient:
        const std::list<std::string> receiver_email_address_list{ recipient };
        for (const auto &receiver_email_address : receiver_email_address_list)
            PerformExchange(socket_fd, time_limit, "RCPT TO:<" + receiver_email_address + ">", "2[0-9][0-9]*",
                            ssl_connection.get());

        PerformExchange(socket_fd, time_limit, "DATA", "3[0-9][0-9]*", ssl_connection.get());
        PerformExchange(socket_fd, time_limit,
                        CreateEmailMessage(priority, format, sender, recipient, subject, message_body) + "\r\n.",
                        "2[0-9][0-9]*", ssl_connection.get());
        PerformExchange(socket_fd, time_limit, "QUIT", "2[0-9][0-9]*", ssl_connection.get());
    } catch (const std::exception &x) {
        if (perform_logging)
            std::clog << x.what() << '\n';
        return false;
    }

    return true;
}


} // namespace EmailSender
