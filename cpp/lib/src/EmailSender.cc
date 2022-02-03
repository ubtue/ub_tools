/** \file   EmailSender.cc
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2021 Universitätsbibliothek Tübingen.
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
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstdlib>
#include <openssl/bio.h>
#include "Compiler.h"
#include "DnsUtil.h"
#include "FileDescriptor.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MediaTypeUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "SocketUtil.h"
#include "SslConnection.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
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
                LOG_ERROR("encoding error!");
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


const std::string SMTP_CONFIG_PATH(UBTools::GetTuelibPath() + "cronjobs/smtp_server.conf");


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


class SMTPException {
    unsigned short response_code_;
    std::string description_;

public:
    SMTPException(const unsigned short response_code, const std::string &description)
        : response_code_(response_code), description_(description) { }

    unsigned short getResponseCode() const { return response_code_; }
    const std::string &getDescription() const { return description_; }
};


// \return the server response if it is a numeric code or 500 o/w.
unsigned short ServerResponseToUnsignedShort(const std::string &server_response) {
    unsigned short numeric_code;
    if (StringUtil::ToUnsignedShort(server_response.substr(0, 3), &numeric_code))
        return numeric_code;
    return 500;
}


void CheckResponse(const std::string &command, const std::string &server_response, const std::string &expected) {
    if (not StringUtil::Match(expected, server_response))
        throw SMTPException(ServerResponseToUnsignedShort(server_response),
                            "in EmailSender.cc: did not receive expected server response to \"" + command + "\" but instead got \""
                                + server_response + "\"!");
}


std::string PerformExchange(const int socket_fd, const TimeLimit &time_limit, const std::string &command,
                            const std::string &expected_response_pattern, SslConnection * const ssl_connection = nullptr) {
    if (perform_logging)
        std::clog << "In PerformExchange: sending: " << command << '\n';
    if (unlikely(SocketUtil::TimedWrite(socket_fd, time_limit, command + "\r\n", ssl_connection) == -1))
        throw SMTPException(523, "in PerformExchange(EmailSender.cc) SocketUtil::TimedWrite failed! (sent: " + command
                                     + ", error: " + std::string(strerror(errno)) + ")");

    // Read the response:
    ssize_t response_size;
    char buf[1000];
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf), ssl_connection)) <= 0)
        throw SMTPException(524, "in PerformExchange(EmailSender.cc): Can't read SMTP server's response to \"" + command + "\"! ("
                                     + std::string(strerror(errno)) + ")");
    buf[std::min(static_cast<size_t>(response_size), sizeof(buf) - 1)] = NUL;
    if (perform_logging)
        std::clog << "In PerformExchange: received: " << buf << '\n';
    CheckResponse(command, buf, expected_response_pattern);
    return buf;
}


void Authenticate(const int socket_fd, const TimeLimit &time_limit, SslConnection * const ssl_connection) {
    std::string server_response(PerformExchange(socket_fd, time_limit, "AUTH LOGIN", "3[0-9][0-9]*", ssl_connection));
    const std::string local_server_user(GetServerUser());
    if (perform_logging) {
        std::clog << "Decoded server response: " << TextUtil::Base64Decode(server_response.substr(4)) << '\n';
        std::clog << "Sending user name: " << local_server_user << '\n';
    }
    server_response = PerformExchange(socket_fd, time_limit, Base64Encode(local_server_user), "3[0-9][0-9]*", ssl_connection);
    const std::string local_server_password(GetServerPassword());
    if (perform_logging) {
        std::clog << "Decoded server response: " << TextUtil::Base64Decode(server_response.substr(4)) << '\n';
        std::clog << "Sending server password: " << local_server_password << '\n';
    }
    PerformExchange(socket_fd, time_limit, Base64Encode(local_server_password), "2[0-9][0-9]*", ssl_connection);
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


void AppendRecipientHeaders(std::string * const message, const std::string &recipient_type, const std::vector<std::string> &recipients) {
    for (const auto &recipient : recipients) {
        const std::string tag(TextUtil::InitialCaps(recipient_type) + ": ");
        if (perform_logging)
            std::clog << tag << recipient << '\n';
        message->append(tag + recipient + "\r\n");
    }
}


std::string &CreateSinglePartEmail(const EmailSender::Priority priority, const EmailSender::Format format, const std::string &message_body,
                                   std::string * const message) {
    if (format == EmailSender::PLAIN_TEXT)
        message->append("Content-Type: text/plain; charset=\"utf-8\"\r\n");
    else
        message->append("Content-Type: text/html; charset=\"utf-8\"\r\n");
    if (priority != EmailSender::DO_NOT_SET_PRIORITY)
        message->append("X-Priority: " + std::to_string(priority) + "\r\n");

    message->append("\r\n");
    message->append(GetDotStuffedMessage(message_body) + "\r\n.");

    return *message;
}


std::string &CreateMultiPartEmail(const EmailSender::Priority priority, const EmailSender::Format format, const std::string &message_body,
                                  const std::vector<std::pair<std::string, std::string>> &content_dispositions_and_contents,
                                  std::string * const message) {
    if (priority != EmailSender::DO_NOT_SET_PRIORITY)
        message->append("X-Priority: " + std::to_string(priority) + "\r\n");
    message->append("MIME-Version: 1.0\r\n");
    static const std::string BOUNDARY("d5BC1f14716511e98ae767d71f13b8d6");
    message->append("Content-Type: multipart/mixed; boundary=" + BOUNDARY + "\r\n");

    message->append("\r\n--" + BOUNDARY + "\r\n");
    if (format == EmailSender::PLAIN_TEXT)
        message->append("Content-Type: text/plain; charset=\"utf-8\"\r\n");
    else
        message->append("Content-Type: text/html; charset=\"utf-8\"\r\n");
    message->append("\r\n");
    message->append(GetDotStuffedMessage(message_body));

    static const unsigned MAX_ENCODED_LINE_LENGTH(76); // See RFC 2045
    for (const auto &content_disposition_and_content : content_dispositions_and_contents) {
        message->append("\r\n--" + BOUNDARY + "\r\n");
        message->append("Content-Type: " + MediaTypeUtil::GetMediaType(content_disposition_and_content.second) + "\r\n");
        message->append("Content-Disposition: " + content_disposition_and_content.first + "\r\n");
        message->append("Content-Transfer-Encoding: base64\r\n");
        message->append("\r\n");
        unsigned line_length(0);
        for (const char ch : Base64Encode(content_disposition_and_content.second)) {
            *message += ch;
            ++line_length;
            if (line_length == MAX_ENCODED_LINE_LENGTH) {
                message->append("\r\n");
                line_length = 0;
            }
        }
        if (line_length > 0)
            message->append("\r\n");
    }

    message->append("\r\n--" + BOUNDARY + "--\r\n");
    message->append(".");

    return *message;
}


std::string CreateEmailMessage(const EmailSender::Priority priority, const EmailSender::Format format, const std::string &sender,
                               const std::vector<std::string> &recipients, const std::vector<std::string> &cc_recipients,
                               const std::vector<std::string> &bcc_recipients, const std::string &subject, const std::string &message_body,
                               const std::vector<std::pair<std::string, std::string>> &content_dispositions_and_contents) {
    std::string message;
    message = "Date: " + GetDateInRFC822Format() + "\r\n";
    message += "From: " + sender + "\r\n";
    message += "X-Mailer: ub_tools mailer\r\n";
    AppendRecipientHeaders(&message, "to", recipients);
    AppendRecipientHeaders(&message, "cc", cc_recipients);
    AppendRecipientHeaders(&message, "bcc", bcc_recipients);

    // See RFC 1342
    message += "Subject: =?utf-8?B?" + Base64Encode(subject) + "?=\r\n";

    if (content_dispositions_and_contents.empty())
        return CreateSinglePartEmail(priority, format, message_body, &message);
    else
        return CreateMultiPartEmail(priority, format, message_body, content_dispositions_and_contents, &message);
}


bool CleanAddress(const std::string &email_address, std::string * const cleaned_up_email_address) {
    const auto open_angle_bracket_pos(email_address.find('<'));
    if (open_angle_bracket_pos == std::string::npos) {
        *cleaned_up_email_address = email_address;
        return true;
    }

    const auto close_angle_bracket_pos(email_address.find('>'));
    if (unlikely(close_angle_bracket_pos == std::string::npos))
        return false;

    *cleaned_up_email_address = email_address.substr(close_angle_bracket_pos + 1, close_angle_bracket_pos - open_angle_bracket_pos - 1);
    return true;
}


bool ProcessRecipients(const int socket_fd, const TimeLimit &time_limit, const std::vector<std::string> &recipients,
                       SslConnection * const ssl_connection) {
    for (const auto &recipient : recipients) {
        std::string cleaned_up_email_address;
        if (unlikely(not CleanAddress(recipient, &cleaned_up_email_address)))
            return false;
        PerformExchange(socket_fd, time_limit, "RCPT TO:<" + cleaned_up_email_address + ">", "2[0-9][0-9]*", ssl_connection);
    }

    return true;
}


void InitContentdispositions(const std::vector<std::string> &attachments, const EmailSender::AttachmentType attachment_type,
                             std::vector<std::pair<std::string, std::string>> * const content_dispositions_and_contents) {
    if (attachment_type == EmailSender::AT_INVALID) {
        if (unlikely(not attachments.empty()))
            LOG_ERROR(
                "you must specify a corresponding attachment type (AT_FILENAMES or AT_DATA) when providing email "
                "attachments!");
    }

    content_dispositions_and_contents->reserve(attachments.size());
    for (const auto &attachment : attachments) {
        if (attachment_type == EmailSender::AT_DATA)
            content_dispositions_and_contents->emplace_back(std::make_pair("inline", attachment));
        else { // we assume that attachment_type == EmailSender::AT_FILENAMES
            std::string data;
            if (unlikely(not FileUtil::ReadString(attachment, &data)))
                LOG_ERROR("failed to read content of attachment from \"" + attachment + "\"!");
            content_dispositions_and_contents->emplace_back(
                std::make_pair("attachment; filename=\"" + FileUtil::GetBasename(attachment) + "\"", data));
        }
    }
}


} // unnamed namespace


namespace EmailSender {


unsigned short SimplerSendEmail(const std::string &sender, const std::vector<std::string> &recipients, const std::string &subject,
                                const std::string &message_body, const Priority priority, const Format format) {
    return SendEmail(sender, recipients, /* cc_recipients = */ {}, /* bcc_recipients = */ {}, subject, message_body, priority, format);
}


unsigned short SendEmailWithFileAttachments(const std::string &sender, const std::vector<std::string> &recipients,
                                            const std::string &subject, const std::string &message_body,
                                            const std::vector<std::string> &attachment_filenames, const Priority priority,
                                            const Format format) {
    return SendEmail(sender, recipients, /* cc_recipients = */ {}, /* bcc_recipients = */ {}, subject, message_body, priority, format,
                     /* reply_to = */ "", attachment_filenames, AT_FILENAMES);
}


unsigned short SendEmailWithInlineAttachments(const std::string &sender, const std::vector<std::string> &recipients,
                                              const std::string &subject, const std::string &message_body,
                                              const std::vector<std::string> &attachments, const Priority priority, const Format format) {
    return SendEmail(sender, recipients, /* cc_recipients = */ {}, /* bcc_recipients = */ {}, subject, message_body, priority, format,
                     /* reply_to = */ "", attachments, AT_DATA);
}


unsigned short SendEmail(const std::string &sender, const std::vector<std::string> &recipients,
                         const std::vector<std::string> &cc_recipients, const std::vector<std::string> &bcc_recipients,
                         const std::string &subject, const std::string &message_body, const Priority priority, const Format format,
                         const std::string &reply_to, const std::vector<std::string> &attachments, const AttachmentType attachment_type,
                         const bool use_ssl, const bool use_authentication) {
    if (unlikely(sender.empty() and reply_to.empty()))
        LOG_ERROR("both \"sender\" and \"reply_to\" can't be empty!");

    std::vector<std::pair<std::string, std::string>> content_dispositions_and_contents;
    InitContentdispositions(attachments, attachment_type, &content_dispositions_and_contents);

    const TimeLimit time_limit(20000 /* ms */);

    perform_logging = not MiscUtil::SafeGetEnv("ENABLE_SMTP_CLIENT_PERFORM_LOGGING").empty();

    // Open connection:
    const unsigned short PORT(587);
    std::string error_message;
    const FileDescriptor socket_fd(SocketUtil::TcpConnect(GetSmtpServer(), PORT, time_limit, &error_message, SocketUtil::DISABLE_NAGLE));
    if (socket_fd == -1) {
        LOG_WARNING("can't connect to SMTP server \"" + GetSmtpServer() + ":" + std::to_string(PORT) + " (" + error_message + ")!");
        return 521;
    }

    // Read the welcome message:
    char buf[1000];
    ssize_t response_size;
    if ((response_size = SocketUtil::TimedRead(socket_fd, time_limit, buf, sizeof(buf))) <= 0) {
        LOG_WARNING("can't read SMTP server's welcome message!");
        return 522;
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

        PerformExchange(socket_fd, time_limit, "EHLO " + DnsUtil::GetHostname(), "2[0-9][0-9]*", ssl_connection.get());

        if (use_authentication)
            Authenticate(socket_fd, time_limit, ssl_connection.get());

        PerformExchange(socket_fd, time_limit, "MAIL FROM:<" + sender + ">", "2[0-9][0-9]*", ssl_connection.get());

        // Send email to each recipient:
        if (unlikely(recipients.empty() and cc_recipients.empty() and bcc_recipients.empty()))
            return false;
        if (unlikely(not ProcessRecipients(socket_fd, time_limit, recipients, ssl_connection.get())))
            return false;
        if (unlikely(not ProcessRecipients(socket_fd, time_limit, cc_recipients, ssl_connection.get())))
            return false;
        if (unlikely(not ProcessRecipients(socket_fd, time_limit, bcc_recipients, ssl_connection.get())))
            return false;

        PerformExchange(socket_fd, time_limit, "DATA", "3[0-9][0-9]*", ssl_connection.get());
        PerformExchange(socket_fd, time_limit,
                        CreateEmailMessage(priority, format, sender, recipients, cc_recipients, bcc_recipients, subject, message_body,
                                           content_dispositions_and_contents),
                        "2[0-9][0-9]*", ssl_connection.get());
        PerformExchange(socket_fd, time_limit, "QUIT", "2[0-9][0-9]*", ssl_connection.get());
    } catch (const SMTPException &smtp_exception) {
        if (perform_logging)
            std::clog << smtp_exception.getDescription() << '\n';
        return smtp_exception.getResponseCode();
    }

    return 200;
}


std::string SMTPResponseCodeToString(const unsigned short response_code) {
    switch (response_code) { // The codes are taken from RFC 5321, section 4.2.3.
    case 211:
        return "System status, or system help reply";
    case 214:
        return "Help message (Information on how to use the receiver or the "
               "meaning of a particular non-standard command; this reply is useful "
               "only to the human user)";
    case 220:
        return "<domain> Service ready";
    case 221:
        return "<domain> Service closing transmission channel";
    case 250:
        return "Requested mail action okay, completed";
    case 251:
        return "User not local; will forward to <forward-path>";
    case 252:
        return "Cannot VRFY user, but will accept message and attempt delivery";
    case 354:
        return "Start mail input; end with <CRLF>.<CRLF>";
    case 421:
        return "<domain> Service not available, closing transmission channel "
               "(This may be a reply to any command if the service knows it must "
               "shut down)";
    case 450:
        return "Requested mail action not taken: mailbox unavailable (e.g., "
               "mailbox busy or temporarily blocked for policy reasons)";
    case 451:
        return "Requested action aborted: local error in processing";
    case 452:
        return "Requested action not taken: insufficient system storage";
    case 455:
        return "Server unable to accommodate parameters";
    case 500:
        return "Syntax error, command unrecognized (This may include errors such "
               "as command line too long)";
    case 501:
        return "Syntax error in parameters or arguments";
    case 502:
        return "Command not implemented";
    case 503:
        return "Bad sequence of commands";
    case 504:
        return "Command parameter not implemented";
    case 550:
        return "Requested action not taken: mailbox unavailable (e.g., mailbox "
               "not found, no access, or command rejected for policy reasons)";
    default:
        return "Unknown response code: " + StringUtil::ToString(response_code);
    }
}


// Make sure that all line ends are <CR><LF> pairs.
// Assumes there are no bare carriage returns.
std::string NormaliseLineEnds(const std::string &text) {
    std::string normalised_text;
    normalised_text.reserve(text.length());

    for (const char ch : text) {
        switch (ch) {
        case '\r':
            break;
        case '\n':
            normalised_text += "\r\n";
            break;
        default:
            normalised_text += ch;
        }
    }

    return normalised_text;
}


bool IsValidEmailAddress(const std::string &email_address_candidate) {
    // The following regex is not very accurate but probably good enough for most purposes.
    // (If you can't help yourself, feel free to improve on it!)
    static auto matcher(RegexMatcher::RegexMatcherFactoryOrDie("^\\S+@\\S+\\.\\S+$"));

    return matcher->matched(email_address_candidate);
}


} // namespace EmailSender
