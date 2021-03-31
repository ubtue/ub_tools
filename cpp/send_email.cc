/** \brief Command-line utility to send email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <cstdlib>
#include "IniFile.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << " [--sender=sender] [-reply-to=reply_to] --recipients=recipients\n"
              << "  [--cc-recipients=cc_recipients] [--bcc-recipients=bcc_recipients] [--expand-newline-escapes]\n"
              << "  --subject=subject (--message-body=message_body | --message-body-file=path) [--priority=priority] [--format=format]\n"
              << "  [--attachment=file1 --attachment=file2 .. --inline-attachment=fileN|--inline-attachment=file1 --inline-attachment=file2 .. --inline-attachment=fileN]\n\n"
              << "       \"priority\" has to be one of \"very_low\", \"low\", \"medium\", \"high\", or\n"
              << "       \"very_high\".  \"format\" has to be one of \"plain_text\" or \"html\"  At least one\n"
              << "       of \"sender\" or \"reply-to\" has to be specified. If \"--expand-newline-escapes\" has\n"
              << "       been specified, all occurrences of \\n in the message body will be replaced by a line feed\n"
              << "       and a double backslash by a single backslash.  The message body is assumed to be UTF-8!\n"
              << "       Please note that you can either specify one or more file attachments or inline attachments but not both!\n\n";
    std::exit(EXIT_FAILURE);
}


EmailSender::Priority StringToPriority(const std::string &priority_candidate) {
    if (priority_candidate == "very_low")
        return EmailSender::VERY_LOW;
    if (priority_candidate == "low")
        return EmailSender::LOW;
    if (priority_candidate == "medium")
        return EmailSender::MEDIUM;
    if (priority_candidate == "high")
        return EmailSender::HIGH;
    if (priority_candidate == "very_high")
        return EmailSender::VERY_HIGH;
    LOG_ERROR("\"" + priority_candidate + "\" is an unknown priority!");
}


EmailSender::Format StringToFormat(const std::string &format_candidate) {
    if (format_candidate == "plain_text")
        return EmailSender::PLAIN_TEXT;
    else if (format_candidate == "html")
        return EmailSender::HTML;
    LOG_ERROR("\"" + format_candidate + "\" is an unknown format!");
}


bool ExtractArg(const char * const argument, const std::string &arg_name, std::string * const arg_value) {
    if (StringUtil::StartsWith(argument, "--" + arg_name + "=")) {
        *arg_value = argument + arg_name.length() + 3 /* two dashes and one equal sign */;
        if (arg_value->empty())
            LOG_ERROR(arg_name + " is missing!");

        return true;
    }

    return false;
}


enum AttachmentType { NONE, FILE_ATTACHMENT, INLINE_ATTACHMENT };


void ParseCommandLine(char **argv, std::string * const sender, std::string * const reply_to, std::string * const recipients,
                      std::string * const cc_recipients, std::string * const bcc_recipients, std::string * const subject,
                      std::string * const message_body, std::string * const priority, std::string * const format,
                      bool * const expand_newline_escapes, AttachmentType * const attachment_type,
                      std::vector<std::string> * const attachments)
{
    *attachment_type = NONE;
    *expand_newline_escapes = false;
    std::string attachment, message_body_path;
    while (*argv != nullptr) {
        if (std::strcmp(*argv, "--expand-newline-escapes") == 0) {
            *expand_newline_escapes = true;
            ++argv;
        } else if (ExtractArg(*argv, "attachment", &attachment) or ExtractArg(*argv, "inline-attachment", &attachment)) {
            const AttachmentType current_attachment_type(StringUtil::StartsWith(*argv, "--attachment=")
                                                         ? FILE_ATTACHMENT : INLINE_ATTACHMENT);
            if (*attachment_type == NONE)
                *attachment_type = current_attachment_type;
            else if (current_attachment_type != *attachment_type)
                LOG_ERROR("file and inline attachment parameters cannot be mixed!");
            attachment = FileUtil::ExpandTildePath(attachment);
            if (not FileUtil::IsReadable(attachment))
                LOG_ERROR("attachment \"" + attachment + "\" does not exist or isn't readable!");
            attachments->emplace_back(current_attachment_type == INLINE_ATTACHMENT
                                      ? FileUtil::ReadStringOrDie(attachment) : attachment);
            ++argv;
        } else if (ExtractArg(*argv, "sender", sender) or ExtractArg(*argv, "reply-to", reply_to)
            or ExtractArg(*argv, "recipients", recipients) or ExtractArg(*argv, "cc-recipients", cc_recipients)
            or ExtractArg(*argv, "bcc-recipients", bcc_recipients) or ExtractArg(*argv, "subject", subject)
            or ExtractArg(*argv, "message-body", message_body) or ExtractArg(*argv, "message-body-file", &message_body_path)
            or ExtractArg(*argv, "priority", priority) or ExtractArg(*argv, "format", format)
            or ExtractArg(*argv, "format", format))
            ++argv;
        else
            LOG_ERROR("unknown argument: " + std::string(*argv));
    }

    if (not message_body->empty() and not message_body_path.empty())
        LOG_ERROR("you must not specify both, --message-body and --message-body-file!");
    if (not message_body_path.empty())
        FileUtil::ReadStringOrDie(message_body_path, message_body);
    if (recipients->empty() and cc_recipients->empty() and bcc_recipients->empty())
        LOG_ERROR("you must specify a recipient!");
    if (subject->empty())
        LOG_ERROR("you must specify a subject!");
    if (message_body->empty())
        LOG_ERROR("you must specify a message-body!");
}


std::vector<std::string> SplitRecipients(const std::string &recipients) {
    std::vector<std::string> individual_recipients;
    StringUtil::Split(recipients, ',', &individual_recipients, /* suppress_empty_components = */true);
    return individual_recipients;
}


// "text" is assumed to be UTF-8 encoded.
std::string ExpandNewlineEscapes(const std::string &text) {
    std::wstring escaped_string;
    if (unlikely(not TextUtil::UTF8ToWCharString(text, &escaped_string)))
        LOG_ERROR("can't convert a supposed UTF-8 string to a wide string!");

    std::wstring unescaped_string;
    bool backslash_seen(false);
    for (auto ch : escaped_string) {
        if (backslash_seen) {
            if (ch == '\\')
                unescaped_string += '\\';
            else if (ch == 'n')
                unescaped_string += '\n';
            else {
                std::string utf8_string;
                TextUtil::WCharToUTF8String(ch, &utf8_string);
                LOG_ERROR("unknown escape: \\" + utf8_string + "!");
            }
            backslash_seen = false;
        } else if (ch == '\\')
            backslash_seen = true;
        else
            unescaped_string += ch;
    }

    std::string utf8_string;
    if (unlikely(not TextUtil::WCharToUTF8String(unescaped_string, &utf8_string)))
        LOG_ERROR("can't convert a supposed wide string to a UTF-8 string!");

    return utf8_string;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc == 1)
        Usage();

    EmailSender::Priority priority(EmailSender::DO_NOT_SET_PRIORITY);
    EmailSender::Format format(EmailSender::PLAIN_TEXT);

    std::string sender, reply_to, recipients, cc_recipients, bcc_recipients, subject, message_body, priority_as_string,
        format_as_string;
    bool expand_newline_escapes;
    AttachmentType attachment_type;
    std::vector<std::string> attachments;
    ParseCommandLine(++argv, &sender, &reply_to, &recipients, &cc_recipients, &bcc_recipients, &subject, &message_body,
                     &priority_as_string, &format_as_string, &expand_newline_escapes, &attachment_type, &attachments);

    if (sender.empty() and reply_to.empty())
        sender = "no-reply@ub.uni-tuebingen.de";

    if (not priority_as_string.empty())
        priority = StringToPriority(priority_as_string);
    if (not format_as_string.empty())
        format = StringToFormat(format_as_string);

    if (expand_newline_escapes)
        message_body = ExpandNewlineEscapes(message_body);

    const auto response_code(EmailSender::SendEmail(sender, SplitRecipients(recipients), SplitRecipients(cc_recipients),
                                                    SplitRecipients(bcc_recipients), subject, message_body, priority, format,
                                                    reply_to, attachments,
                                                    (attachments.empty() or attachment_type == FILE_ATTACHMENT)
                                                        ? EmailSender::AT_FILENAMES : EmailSender::AT_DATA));
    if (response_code >= 300) {
        if (not MiscUtil::EnvironmentVariableExists("ENABLE_SMTP_CLIENT_PERFORM_LOGGING"))
            LOG_ERROR("failed to send your email, the response code was: " + std::to_string(response_code)
                      + " (You may want to set the ENABLE_SMTP_CLIENT_PERFORM_LOGGING to debug the problem.)");
        else
            LOG_ERROR("failed to send your email, the response code was: " + std::to_string(response_code));
    }

    return EXIT_SUCCESS;
}
