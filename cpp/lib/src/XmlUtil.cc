/** \file   XmlUtil.cc
 *  \brief  Implementation of XML-related utility functionality.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "XmlUtil.h"
#include "Compiler.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace XmlUtil {


inline bool DecodeEntity(const std::string &entity_string, std::string * const decoded_char) {
    if (unlikely(entity_string.empty()))
        return false;

    if (__builtin_strcmp(entity_string.c_str(), "amp") == 0)
        *decoded_char = "&";
    else if (__builtin_strcmp(entity_string.c_str(), "apos") == 0)
        *decoded_char = "'";
    else if (__builtin_strcmp(entity_string.c_str(), "quot") == 0)
        *decoded_char = "\"";
    else if (__builtin_strcmp(entity_string.c_str(), "lt") == 0)
        *decoded_char = "<";
    else if (__builtin_strcmp(entity_string.c_str(), "gt") == 0)
        *decoded_char = ">";
    else if (unlikely(entity_string[0] == '#')) {
        if (entity_string.length() < 2)
            return false;

        unsigned code_point;
        if (entity_string[1] == 'x') {
            if (entity_string.length() < 3 or entity_string.length() > 6)
                return false;
            if (not StringUtil::ToUnsigned(entity_string.substr(2), &code_point, 16))
                return false;
        } else {
            if (entity_string.length() < 2 or entity_string.length() > 6)
                return false;
            if (not StringUtil::ToUnsigned(entity_string.substr(1), &code_point))
                return false;
        }

        if (not TextUtil::WCharToUTF8String(static_cast<wchar_t>(code_point), decoded_char))
            return false;
    } else
        return false;

    return true;
}


bool DecodeEntities(std::string * const data) {
    if (unlikely(data->empty()))
        return true;

    std::string::iterator next(data->begin());
    bool in_entity(false);
    std::string entity;
    for (const auto ch : *data) {
        if (unlikely(in_entity)) {
            if (ch == ';') {
                std::string decoded_char;
                if (not DecodeEntity(entity, &decoded_char)) {
                    LOG_WARNING("can't decode \"" + entity + "\"!");
                    return false;
                }
                for (const auto dch : decoded_char)
                    *next++ = dch;
                in_entity = false;
            } else
                entity += ch;
        } else if (unlikely(ch == '&')) {
            in_entity = true;
            entity.clear();
        } else
            *next++ = ch;
    }

    if (unlikely(next != data->end())) // We had some non-length-preserving entity replacements.
        data->resize(next - data->begin());

    return not in_entity;
}


void XmlEscape(std::string * const data) {
    std::string escaped_data;
    escaped_data.reserve(data->length() * 2);

    for (const char ch : *data) {
        switch (ch) {
        case '"':
            escaped_data += "&quot;";
            break;
        case '\'':
            escaped_data += "&apos;";
            break;
        case '<':
            escaped_data += "&lt;";
            break;
        case '>':
            escaped_data += "&gt;";
            break;
        case '&':
            escaped_data += "&amp;";
            break;
        default:
            escaped_data += ch;
        }
    }

    escaped_data.swap(*data);
}


} // namespace XmlUtil
