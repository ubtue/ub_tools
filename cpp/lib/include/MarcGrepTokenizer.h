/** \file   MarcGrepTokenizer.h
 *  \brief  A tokenizer for the marc_grep2 query tool.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef MARC_GREP_TOKENIZER_H
#define MARC_GREP_TOKENIZER_H


#include <string>


enum TokenType {
    END_OF_INPUT, HYPHEN, OPEN_BRACKET, CLOSE_BRACKET, EQUAL, COLON, COMMA, EQUAL_EQUAL, NOT_EQUAL,
    SINGLE_FIELD_EQUAL, SINGLE_FIELD_NOT_EQUAL, STRING_CONSTANT, STAR,
    UNSIGNED_CONSTANT, INVALID_INPUT, LEADER_KW, IF_KW, EXTRACT_KW, EXISTS_KW, IS_MISSING_KW
};


class Tokenizer {
    const std::string input_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
    TokenType pushed_back_token_;
    bool token_has_been_pushed_back_;
    TokenType last_token_;
    std::string last_string_constant_;
    unsigned last_unsigned_constant_;
public:
    explicit Tokenizer(const std::string &input)
        : input_(input), ch_(input_.begin()), end_(input_.end()), token_has_been_pushed_back_(false) { }
    TokenType getToken();
    void ungetToken();
    std::string getLastStringConstant() const { return last_string_constant_; }
    unsigned getLastUnsignedConstant() const { return last_unsigned_constant_; }
    static std::string EscapeString(const std::string &s);
    static std::string TokenTypeToString(const TokenType token);
};


#endif // ifndef MARC_GREP_TOKENIZER_H
