/** \file    MailUtil.h
    \brief   Mail related utility functions
 */


/*
 *  Copyright 2021 Universitätsbibliothek Tübingen
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

#include "MailUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"

namespace MailUtil {
std::string DecodeBodyPart(const MBox::BodyPart &body_part) {
    std::string charset("utf-8");
    bool is_base64_encoded(false);

    for (const auto &header : body_part.getMIMEHeaders()) {
        if (header.first == "content-type") {
            const auto charset_start_pos(header.second.find("charset="));
            if (charset_start_pos != std::string::npos) {
                charset = header.second.substr(charset_start_pos + __builtin_strlen("charset="));
                StringUtil::Trim(&charset);
                StringUtil::Trim(&charset, '"');
            }
        } else if (header.first == "content-transfer-encoding" and ::strcasecmp(header.second.c_str(), "base64") == 0)
            is_base64_encoded = true;
    }

    std::string body;
    for (const auto &body_line : StringUtil::SplitIntoLines(body_part.getBody())) {
        body += body_line;
        if (not is_base64_encoded)
            body += '\n';
    }

    if (::strcasecmp(charset.c_str(), "utf-8") != 0) {
        std::string error_message;
        const auto encoding_converter(
            TextUtil::EncodingConverter::Factory(charset, TextUtil::EncodingConverter::CANONICAL_UTF8_NAME, &error_message));
        if (unlikely(not error_message.empty()))
            LOG_ERROR("failed to create an encoding converter from \"" + charset + "\" to UTF-8!");

        std::string utf8_body;
        if (unlikely(not encoding_converter->convert(body, &utf8_body)))
            LOG_ERROR("couldn't convert the body from \"" + charset + " UTF-8!");
        utf8_body.swap(body);
    }

    return is_base64_encoded ? TextUtil::Base64Decode(body) : body;
}


} // namespace MailUtil
