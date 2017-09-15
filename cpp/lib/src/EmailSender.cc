/** \file   EmailSender.cc
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Artur Kedzierski
 *  \author Dr. Gordon W. Paynter
 *
 *  \copyright 2015 Universitätsbibliothek Tübingen.
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
#include "Compiler.h"
#include "DnsUtil.h"
#include "FileDescriptor.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "SocketUtil.h"
#include "SslConnection.h"
#include "StringUtil.h"
#include "util.h"


#define NUL '\0'


namespace {


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
        IniFile ini_file("/usr/local/var/lib/tuelib/cronjobs/smtp_server.conf");
        smtp_server = ini_file.getString("SMTPServer", "server_address");
    }

    return smtp_server;
}


void CheckResponse(const std::string &command, const std::string &server_response, const std::string &expected) {
    if (not StringUtil::Match(expected, server_response))
        throw std::runtime_error("in EmailSender.cc: did not receive expected server response to \"" + command
                                 + "\" but instead got \"" + server_response + "\"!");
}


void PerformExchange(const int socket_fd, const TimeLimit &time_limit, const std::string &command,
                     const std::string &expected_response_pattern, SslConnection * const ssl_connection = nullptr)
{
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
    CheckResponse(command, buf, expected_response_pattern);
}


void PerformHeloExchange(const int socket_fd, const TimeLimit &time_limit,
                         SslConnection * const ssl_connection = nullptr)
{
    PerformExchange(socket_fd, time_limit, "HELO " + DnsUtil::GetHostname(), "2[0-9][0-9]*", ssl_connection);
}


void PerformStartTlsExchange(const int socket_fd, const TimeLimit &time_limit) {
    PerformExchange(socket_fd, time_limit, "STARTTLS", "2[0-9][0-9]*");
}


void PerformMailFromExchange(const int socket_fd, const std::string &sender_email_address,
                             const TimeLimit &time_limit, SslConnection * const ssl_connection)
{
    PerformExchange(socket_fd, time_limit, "MAIL FROM:<" + sender_email_address + ">", "2[0-9][0-9]*",
                    ssl_connection);
}


void PerformReceipientToExchange(const int socket_fd, const std::string &receiver_email_address,
                                 const TimeLimit &time_limit, SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "RCPT TO:<" + receiver_email_address + "\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformReceipientToExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ")");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0)
        throw std::runtime_error("in PerformReceipientToExchange(EmailSender.cc): Can't read SMTP server's response "
                                 "to RCPT TO! ("+ std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    CheckResponse("RCPT TO", buf, "2[0-9][0-9]*");
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
    message += "Subject: " + subject + "\r\n\r\n";
    if (format == EmailSender::PLAIN_TEXT)
        message += "Content-Type: text/plain; charset=\"utf-8\"";
    else
        message += "Content-Type: text/html; charset=\"utf-8\"";
    if (priority != EmailSender::DO_NOT_SET_PRIORITY)
        message += "X-Priority: " + std::to_string(priority) + "\r\n\r\n";
    message += GetDotStuffedMessage(message_body) + ".\r\n";

    return message;
}


void ProcessSendEmailExchange(const int socket_fd, const EmailSender::Priority priority,
                              const EmailSender::Format format, const std::string &sender,
                              const std::string &recipient, const std::string &subject,
                              const std::string &message_body, const TimeLimit &time_limit,
                              SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "DATA\r\n", ssl_connection) == -1))
        throw std::runtime_error("in ProcessSendEmailExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ")");

    char buf[1000];
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) // read the response
        throw std::runtime_error("in EmailSender::SendEmail: Can't read SMTP server's response to DATA! ("
                                 + std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    CheckResponse("DATA", buf, "3[0-9][0-9]*");

    // Send the actual email:
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit,
             CreateEmailMessage(priority, format, sender, recipient, subject, message_body) + "\r\n.\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in ProcessSendEmailExchange(EmailSender.cc): SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ") (2)");
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0)
        throw std::runtime_error("in ProcessSendEmailExchange(EmailSender.cc): Can't read SMTP server's response to "
                                 "sent data! (" + std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    CheckResponse("sent message", buf, "2[0-9][0-9]*");
}


void PerformQuitExchange(const int socket_fd, const TimeLimit &time_limit, SslConnection * const ssl_connection) {
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "QUIT\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformQuitExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ")");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0)
        throw std::runtime_error("in PerformQuitExchange(EmailSender.cc): Can't read SMTP server's response to "
                                 "QUIT! (" + std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    CheckResponse("QUIT", buf, "2[0-9][0-9]*");
}


} // unnamed namespace


namespace EmailSender {


bool SendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
               const std::string &message_body, const Priority priority, const Format format,
               const std::string &reply_to, const bool use_ssl)
{
    if (unlikely(sender.empty() and reply_to.empty()))
        Error("in EmailSender::SendEmail: both \"sender\" and \"reply_to\" can't be empty!");

    const TimeLimit time_limit(10000 /* ms */);

    const bool log(not MiscUtil::SafeGetEnv("ENABLE_SMPT_CLIENT_LOGGING").empty());

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
    if (log) {
        buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
        std::clog << "Server sent: " << buf << '\n';
    }

    try {
        PerformHeloExchange(socket_fd, time_limit);

        std::unique_ptr<SslConnection> ssl_connection;
        if (use_ssl) {
            PerformStartTlsExchange(socket_fd, time_limit);
            ssl_connection.reset(new SslConnection(socket_fd));
        }

        PerformHeloExchange(socket_fd, time_limit, ssl_connection.get());
        PerformMailFromExchange(socket_fd, sender, time_limit, ssl_connection.get());

        // Send email to each recipient:
        const std::list<std::string> receiver_email_address_list{ recipient };
        for (const auto &receiver_email_address : receiver_email_address_list)
            PerformReceipientToExchange(socket_fd, receiver_email_address, time_limit, ssl_connection.get());

        ProcessSendEmailExchange(socket_fd, priority, format, sender, recipient, subject, message_body, time_limit,
                                 ssl_connection.get());
        PerformQuitExchange(socket_fd, time_limit, ssl_connection.get());
    } catch (const std::exception &x) {
        if (log)
            std::clog << x.what() << '\n';
        return false;
    }

    return true;
}


} // namespace EmailSender
