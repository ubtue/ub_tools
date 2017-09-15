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
#include <memory>
#include <list>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DnsUtil.h"
#include "FileDescriptor.h"
#include "IniFile.h"
#include "SocketUtil.h"
#include "SslConnection.h"
#include "StringUtil.h"
#include "util.h"


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


void WriteToConnection(const int socket_fd, const TimeLimit &time_limit, const std::string &data,
                              SslConnection * const ssl_connection)
{
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, data, ssl_connection) == -1))
        throw std::runtime_error("in WriteToConnection(EmailSender.cc) SocketUtil::TimedWrite failed! ("
                                 + std::string(strerror(errno)) + "))");
}


std::string GetDotStuffedMessage(const std::string &message) {
    std::list<std::string> lines;
    StringUtil::SplitThenTrim(message, "\n", "\r", &lines, /* suppress_empty_words = */ false);
    for (std::list<std::string>::iterator line(lines.begin()); line != lines.end(); ++line) {
        if (not line->empty() and (*line)[0] == '.')
            *line = "." + *line;
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


} // unnamed namespace


namespace EmailSender {


bool SendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
               const std::string &message_body, const Priority priority, const Format format,
               const std::string &reply_to, const bool use_ssl)
{
    if (unlikely(sender.empty() and reply_to.empty()))
        Error("in EmailSender::SendEmail: both \"sender\" and \"reply_to\" can't be empty!");

    const TimeLimit time_limit(10000 /* ms */);

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
    if (SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get()) <= 0) {
        Warning("in EmailSender::SendEmail: Can't read SMTP server's welcome message!");
        return false;
    }

    // HELO <hostname>
    WriteToConnection(socket_fd, time_limit, "HELO " + DnsUtil::GetHostname() + "\r\n", ssl_connection.get());
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get())) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to HELO!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to \"HELO\" command: " + std::string(buf));
        return false;
    }

    // MAIL FROM: <email address of sender>
    WriteToConnection(socket_fd, time_limit, "MAIL FROM:<" + sender + ">\r\n", ssl_connection.get());
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get())) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to MAIL FROM:!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to \"MAIL FROM:\" command: "
                + std::string(buf));
        return false;
    }

    // Send email to each recipient:
    const std::list<std::string> receiver_email_address_list{ recipient };
    for (std::list<std::string>::const_iterator receiver_email_address(receiver_email_address_list.begin());
         receiver_email_address != receiver_email_address_list.end(); ++receiver_email_address)
    {
        // RCPT TO: <email address of receiver>
        WriteToConnection(socket_fd, time_limit, "RCPT TO:<" + *receiver_email_address + ">\r\n", ssl_connection.get());
        if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get())) <= 0) { // read the response
            Warning("in EmailSender::SendEmail: Can't read SMTP server's response to RCPT TO:!");
            return false;
        }
        buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
        // Expect a 2xx success code:
        if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
            Warning("in EmailSender::SendEmail: Bad status code in response to \"RCPT TO:\" command: "
                    + std::string(buf));
            return false;
        }
    }

    // DATA
    WriteToConnection(socket_fd, time_limit, "DATA\r\n", ssl_connection.get());
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to DATA!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
    // Expect a 3xx code:
    if (not StringUtil::Match("3[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to \"DATE\" command: " + std::string(buf));
        return false;
    }

    // <data terminated by "." on a line by itself>
    WriteToConnection(socket_fd, time_limit,
                      CreateEmailMessage(priority, format, sender, recipient, subject, message_body) + "\r\n.\r\n",
                      ssl_connection.get());
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection.get())) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to sent data!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
    // Expect a 2xx success code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to sent data: " + std::string(buf));
        return false;
    }

    // QUIT
    WriteToConnection(socket_fd, time_limit, "QUIT\r\n", ssl_connection.get());
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) { // read the response
        Warning("in EmailSender::SendEmail: Can't read SMTP server's response to QUIT!");
        return false;
    }
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = '\0';
    // Expect a 2xx code:
    if (not StringUtil::Match("2[0-9][0-9]*", buf)) {
        Warning("in EmailSender::SendEmail: Bad status code in response to \"QUIT\" command: " + std::string(buf));
        return false;
    }
    
    return true;
}


} // namespace EmailSender
