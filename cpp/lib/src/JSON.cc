/** \file   JSON.cc
 *  \brief  Implementation of JSON-related functionality.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "JSON.h"
#include <cctype>
#include "Compiler.h"
#include "StringUtil.h"
#include "TextUtil.h"


namespace JSON {


TokenType Scanner::getToken() {
    skipWhite();

    if (unlikely(ch_ == end_))
        return END_OF_INPUT;

    switch (*ch_) {
    case ',':
        ++ch_;
        return COMMA;
    case ':':
        ++ch_;
        return COLON;
    case '{':
        ++ch_;
        return OPEN_BRACE;
    case '}':
        ++ch_;
        return CLOSE_BRACKET;
    case '[':
        ++ch_;
        return OPEN_BRACKET;
    case ']':
        ++ch_;
        return CLOSE_BRACE;
    case '"':
        return parseStringConstant();
    case 't':
        return expectSequence("true", TRUE_CONST);
    case 'f':
        return expectSequence("false", FALSE_CONST);
    case 'n':
        return expectSequence("null", NULL_CONST);
    case '+':
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return parseNumber();
    default:
        last_error_message_ = "unexpected character '" + std::string(1, *ch_) + "'!";
        return ERROR;
    }
}


void Scanner::skipWhite() {
    while (ch_ != end_ and isspace(*ch_)) {
        if (*ch_ == '\n')
            ++line_no_;
        ++ch_;
    }
}


TokenType Scanner::expectSequence(const std::string &sequence, const TokenType success_token) {
    for (const char expected_ch : sequence) {
        if (unlikely(ch_ == end_)) {
            last_error_message_ = "expected \"" + sequence + "\" but reached end-of-input!";
            return ERROR;
        } else if (unlikely(*ch_ != expected_ch)) {
            last_error_message_ = "expected \"" + sequence + "\" but found something else!";
            return ERROR;
        }

        ++ch_;
    }

    return success_token;
}


TokenType Scanner::parseNumber() {
    std::string number_as_string;
    if (*ch_ == '+' or *ch_ == '-')
        number_as_string += *ch_++;

    for (; ch_ != end_ and StringUtil::IsDigit(*ch_); ++ch_)
        number_as_string += *ch_;
    if (unlikely(number_as_string.empty())) {
        last_error_message_ = "missing digit or digits after a sign!";
        return ERROR;
    }

    if (ch_ == end_ or (*ch_ != '.' and *ch_ != 'e')) {
        int64_t value;
        if (unlikely(not StringUtil::ToInt64T(number_as_string, &value))) {
            last_error_message_ = "failed to convert \"" + number_as_string + "\" to a 64-bit integer!";
            return ERROR;
        }

        last_integer_constant_ = value;
        return INTEGER_CONST;
    }

    if (*ch_ == '.') {
        for (; ch_ != end_ and StringUtil::IsDigit(*ch_); ++ch_)
            number_as_string += *ch_;
    }

    if (ch_ == end_ or *ch_ != 'e') {
        double value;
        if (unlikely(not StringUtil::ToDouble(number_as_string, &value))) {
            last_error_message_ = "failed to convert \"" + number_as_string + "\" to a floating point value!";
            return ERROR;
        }

        last_double_constant_ = value;
        return DOUBLE_CONST;
    }

    ++ch_;
    if (*ch_ == '+' or *ch_ == '-')
        number_as_string += *ch_++;
    if (unlikely(not StringUtil::IsDigit(*ch_))) {
        last_error_message_ = "missing digits for the exponent!";
        return ERROR;
    }
    for (; ch_ != end_ and StringUtil::IsDigit(*ch_); ++ch_)
        number_as_string += *ch_;

    double value;
    if (unlikely(not StringUtil::ToDouble(number_as_string, &value))) {
        last_error_message_ = "failed to convert \"" + number_as_string + "\" to a floating point value!";
        return ERROR;
    }

    last_double_constant_ = value;
    return DOUBLE_CONST;
}


// Converts the nnnn part of \unnnn to UTF-8. */
bool Scanner::UTF16EscapeToUTF8(std::string * const utf8) {
    std::string hex_codes;
    for (unsigned i(0); i < 4; ++i) {
        if (unlikely(ch_ == end_)) {
            last_error_message_ = "unexpected end-of-input while looking for a \\unnnn escape!";
            return false;
        }
        hex_codes += *ch_++;
    }

    uint16_t u1;
    if (unlikely(not StringUtil::ToUnsignedShort(hex_codes, &u1, 16))) {
        last_error_message_ = "invalid hex sequence \\u" + hex_codes + "!";
        return false;
    }

    if (TextUtil::IsValidSingleUTF16Char(u1)) {
        *utf8 = TextUtil::UTF32ToUTF8(TextUtil::UTF16ToUTF32(u1));
        return true;
    }

    if (unlikely(not TextUtil::IsFirstHalfOfSurrogatePair(u1))) {
        last_error_message_ = "\\u" + hex_codes + " is neither a standalone UTF-8 character nor a valid first half "
                              "of a UTF-16 surrogate pair!";
        return false;
    }

    if (unlikely(ch_ == end_ or *ch_++ != '\\')) {
        last_error_message_ = "could not find expected '\\' as part of the 2nd half of a surrogate pair!";
        return false;
    }
    if (unlikely(ch_ == end_ or *ch_++ != 'u')) {
        last_error_message_ = "could not find expected 'u' as part of the 2nd half of a surrogate pair!";
        return false;
    }

    hex_codes.clear();
    for (unsigned i(0); i < 4; ++i) {
        if (unlikely(ch_ == end_)) {
            last_error_message_ = "unexpected end of input while attempting to read the 2nd half of a surrogate "
                                  "pair!";
            return false;
        }
        hex_codes += *ch_++;
    }

    uint16_t u2;
    if (unlikely(not StringUtil::ToUnsignedShort(hex_codes, &u2, 16))) {
        last_error_message_ = "invalid hex sequence \\u" + hex_codes + " for the 2nd half of a surrogate pair!";
        return false;
    }
    if (unlikely(not TextUtil::IsSecondHalfOfSurrogatePair(u2))) {
        last_error_message_ = "invalid 2nd half of a surrogate pair: \\u" + hex_codes + "!";
        return false;
    }

    *utf8 = TextUtil::UTF32ToUTF8(TextUtil::UTF16ToUTF32(u1, u2));
    return true;
}


TokenType Scanner::parseStringConstant() {
    ++ch_; // Skip over initial double quote.

    const unsigned start_line_no(line_no_);
    std::string string_value;
    while (ch_ != end_ and *ch_ != '"') {
        if (*ch_ != '\\')
            string_value += *ch_++;
        else { // Deal w/ an escape sequence.
            if (unlikely(ch_ + 1 == end_)) {
                last_error_message_ = "end-of-input encountered while parsing a string constant, starting on line "
                                      + std::to_string(start_line_no) + "!";
                return ERROR;
            }
            ++ch_;
            switch (*ch_) {
            case '/':
            case '"':
            case '\\':
                string_value += *ch_++;
                break;
            case 'b':
                ++ch_;
                string_value += '\b';
                break;
            case 'f':
                ++ch_;
                string_value += '\f';
                break;
            case 'n':
                ++ch_;
                string_value += '\n';
                break;
            case 'r':
                ++ch_;
                string_value += '\r';
                break;
            case 't':
                ++ch_;
                string_value += '\t';
                break;
            case 'u': {
                std::string utf8;
                if (unlikely(not UTF16EscapeToUTF8(&utf8)))
                    return ERROR;
                string_value += utf8;
                break;
            }
            default:
                last_error_message_ = "unexpected escape \\" + std::string(1, *ch_) + " in string constant!";
                return ERROR;
            }
        }
    }

    if (unlikely(ch_ == end_)) {
        last_error_message_ = "end-of-input encountered while parsing a string constant, starting on line "
                              + std::to_string(start_line_no) + "!";
        return ERROR;
    }
    ++ch_; // Skip over closing double quote.

    last_string_constant_ = string_value;
    return STRING_CONST;
}


} // namespace JSON
