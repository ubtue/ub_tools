/** \file   EmailSender.h
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <string>
#include <vector>


namespace EmailSender {


enum Priority { DO_NOT_SET_PRIORITY = 0, VERY_LOW = 5, LOW = 4, MEDIUM = 3, HIGH = 2, VERY_HIGH = 1 };
enum Format { PLAIN_TEXT, HTML };


/*
 * Unless you actually need the horrendously complex general SendEmail function you should use
 * one of the following convenience wrapper functions!
 */
unsigned short SimplerSendEmail(const std::string &sender, const std::vector<std::string> &recipients, const std::string &subject,
                                const std::string &message_body, const Priority priority = DO_NOT_SET_PRIORITY,
                                const Format format = PLAIN_TEXT);
inline unsigned short SimplerSendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
                                       const std::string &message_body, const Priority priority = DO_NOT_SET_PRIORITY,
                                       const Format format = PLAIN_TEXT) {
    return SimplerSendEmail(sender, std::vector<std::string>{ recipient }, subject, message_body, priority, format);
}


unsigned short SendEmailWithFileAttachments(const std::string &sender, const std::vector<std::string> &recipients,
                                            const std::string &subject, const std::string &message_body,
                                            const std::vector<std::string> &attachment_filenames,
                                            const Priority priority = DO_NOT_SET_PRIORITY, const Format format = PLAIN_TEXT);
unsigned short SendEmailWithInlineAttachments(const std::string &sender, const std::vector<std::string> &recipients,
                                              const std::string &subject, const std::string &message_body,
                                              const std::vector<std::string> &attachments, const Priority priority = DO_NOT_SET_PRIORITY,
                                              const Format format = PLAIN_TEXT);


enum AttachmentType {
    AT_FILENAMES, // The attachment parameter contains paths to the files that should be attached.
    AT_DATA,      // The attachment parameter contains the actual data to be attached.
    AT_INVALID    // The default to signal that there are no attachments.
};


/** \note Please note that "sender", "recipient", and "cc" email addresses may either be regular email addresses or of the
 *        form "Name<email_address>".  Also "subject" and "message_body" are assumed to be in UTF-8.  Also, at least
 *        one of "sender" or "reply_to" have to be specified.
 *  \note The message body must be UTF-8!
 *  \return In order to understand the significance of the return codes, read https://www.ietf.org/rfc/rfc5321.txt
 *          especially starting at the section titled "Reply Code Severities and Theory" (4.2.1.).
 *  \note It is probably Ok to test for <= 299 as a successful return code.
 */
unsigned short SendEmail(const std::string &sender, const std::vector<std::string> &recipients,
                         const std::vector<std::string> &cc_recipients, const std::vector<std::string> &bcc_recipients,
                         const std::string &subject, const std::string &message_body, const Priority priority = DO_NOT_SET_PRIORITY,
                         const Format format = PLAIN_TEXT, const std::string &reply_to = "",
                         const std::vector<std::string> &attachments = {}, const AttachmentType attachment_type = AT_INVALID,
                         const bool use_ssl = true, const bool use_authentication = true);


std::string SMTPResponseCodeToString(const unsigned short response_code);


/** Ensures that all line ends are <CR><LF> pairs.
 *  Assumes there are no bare carriage returns.
 */
std::string NormaliseLineEnds(const std::string &text);


bool IsValidEmailAddress(const std::string &email_address_candidate);


} // namespace EmailSender
