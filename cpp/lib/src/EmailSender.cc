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


bool PerformHeloExchange(const int socket_fd, const TimeLimit &time_limit, SslConnection * const ssl_connection) {
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "HELO " + DnsUtil::GetHostname() + "\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformHeloExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + "))");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0) {
        Warning("in PerformHeloExchange(EmailSender.cc): Can't read SMTP server's response to HELO!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in PerformHeloExchange(SendEmail.cc): Bad status code in response to \"HELO\" command: "
                + std::string(buf));
        return false;
    }

    return true;
}


bool PerformMailFromExchange(const int socket_fd, const std::string &sender_email_address,
                             const TimeLimit &time_limit, SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "MAIL FROM:<" + sender_email_address + "\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformMailFromExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + "))");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0) {
        Warning("in PerformMailFromExchange(EmailSender.cc): Can't read SMTP server's response to MAIL FROM:!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in PerformMailFromExchange(EmailSender.cc): Bad status code in response to \"MAIL FROM:\" command: "
                + std::string(buf));
        return false;
    }

    return true;
}


bool PerformReceipientToExchange(const int socket_fd, const std::string &receiver_email_address,
                                 const TimeLimit &time_limit, SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "MAIL FROM:<" + receiver_email_address + "\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformReceipientToExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + "))");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0) {
        Warning("in PerformReceipientToExchange(EmailSender.cc): Can't read SMTP server's response to MAIL FROM:!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in PerformReceipientToExchange(EmailSender.cc): Bad status code in response to \"RCPT TO:\" "
                "command: " + std::string(buf));
        return false;
    }

    return true;
}


bool ProcessSendEmailExchange(const int socket_fd, const EmailSender::Priority priority,
                              const EmailSender::Format format, const std::string &sender,
                              const std::string &recipient, const std::string &subject,
                              const std::string &message_body, const TimeLimit &time_limit,
                              SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "DATA\r\n", ssl_connection) == -1))
        throw std::runtime_error("in ProcessSendEmailExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ")) (1)");

    char buf[1000];
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to DATA!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 3xx code:
    if (not StringUtil::Match("3[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to \"DATA\" command: " + std::string(buf));
        return false;
    }

    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit,
             CreateEmailMessage(priority, format, sender, recipient, subject, message_body) + "\r\n.\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in ProcessSendEmailExchange(EmailSender.cc): SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + ")) (2)");
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0) {
        Warning("in ProcessSendEmailExchange(EmailSender.cc): Can't read SMTP server's response to sent data!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in ProcessSendEmailExchange(EmailSender.cc): Bad status code in response to sent data: "
                + std::string(buf));
        return false;
    }

    return true;
}


bool PerformQuitExchange(const int socket_fd, const TimeLimit &time_limit, SslConnection * const ssl_connection) {
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, "QUIT\r\n",
                                        ssl_connection) == -1))
        throw std::runtime_error("in PerformQuitExchange(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + "))");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0) {
        Warning("in PerformQuitExchange(EmailSender.cc): Can't read SMTP server's response to QUIT!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in PerformQuitExchange(SendEmail.cc): Bad status code in response to \"QUIT\" command: "
                + std::string(buf));
        return false;
    }

    return true;
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
        Warning("in EmailSender::SendEmail: can't connect to SMTP server \"" + GetSmtpServer() + ":"
                + std::to_string(PORT) + " (" + error_message + ")!");
        return false;
    }

    std::unique_ptr<SslConnection> ssl_connection;
    if (use_ssl)
        ssl_connection.reset(new SslConnection(socket_fd));

    // Read the welcome message:
    char buf[1000];
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get())) <= 0) {
        Warning("in EmailSender::SendEmail: Can't read SMTP server's welcome message!");
        return false;
    }
    if (log) {
        buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
        std::clog << "Sever sent: " << buf << '\n';
    }

    if (not PerformHeloExchange(socket_fd, time_limit, ssl_connection.get()))
        return false;

    if (not PerformMailFromExchange(socket_fd, sender, time_limit, ssl_connection.get()))
        return false;

    // Send email to each recipient:
    const std::list<std::string> receiver_email_address_list{ recipient };
    for (const auto &receiver_email_address : receiver_email_address_list) {
        if (not PerformReceipientToExchange(socket_fd, receiver_email_address, time_limit, ssl_connection.get()))
            return false;
    }

    if (not ProcessSendEmailExchange(socket_fd, priority, format, sender, recipient, subject, message_body,
                                     time_limit, ssl_connection.get()))
        return false;

    if (not PerformQuitExchange(socket_fd, time_limit, ssl_connection.get()))
        return false;

    return true;
}


} // namespace EmailSender
