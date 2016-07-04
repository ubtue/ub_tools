/** \file   JSON.cc
 *  \brief  Implementation of a simple JSON parser as well as support classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "JSON.h"
#include <stdexcept>
#include <iostream>//XXX
#include <cctype>
#include "Compiler.h"
#include "StringUtil.h"
#include "TextUtil.h"


namespace {


std::string EscapeString(const std::string &s) {
    std::string escaped_string;
    escaped_string.reserve(s.size());

    for (const char ch : s) {
        switch (ch) {
        case '\\':
        case '"':
        case '/':
            escaped_string += '\\';
            escaped_string += ch;
            break;
        case '\b':
            escaped_string += "\\\b";
            break;
        case '\t':
            escaped_string += "\\\t";
            break;
        case '\n':
            escaped_string += "\\\n";
            break;
        case '\f':
            escaped_string += "\\\f";
            break;
        case '\r':
            escaped_string += "\\\r";
            break;
        default:
            if (iscntrl(ch))
                escaped_string += "\\u" + StringUtil::ToString(ch, 16, 4);
            else
                escaped_string += ch;
        }
    }

    return escaped_string;
}


bool DecodeEscape(std::string::const_iterator &ch, const std::string::const_iterator &end,
                  std::string * const expanded_escape)
{
    expanded_escape->clear();
    ++ch; // Skip over the backslash.
    if (unlikely(ch == end))
        return false;

    switch (*ch) {
    case '"':
    case '\\':
    case '/':
        *expanded_escape += *ch;
        return true;
    case 'b':
        *expanded_escape += '\b';
        return true;
    case 'f':
        *expanded_escape += '\f';
        return true;
    case 'n':
        *expanded_escape += '\n';
        return true;
    case 'r':
        *expanded_escape += '\r';
        return true;
    case 't':
        *expanded_escape += '\t';
        return true;
    case 'u': {
        std::string unicode;
        for (unsigned i(0); i < 4; ++i) {
            ++ch;
            if (unlikely(ch == end))
                return false;
            unicode += *ch;
        }
        unsigned code_point;
        if (unlikely(StringUtil::ToUnsigned(unicode, &code_point, 16)))
            return false;
	if (not TextUtil::WCharToUTF8String(std::wstring(1, static_cast<wchar_t>(code_point)), expanded_escape))
	    return false;
    }
    default:
        return false;
    }
}


} // namespace


namespace JSON {


void ScalarNode::print(std::ostream &output, const unsigned indent) const {
    output << std::string(indent, ' ') << '"' << EscapeString(getName()) << "\" : ";
    if (scalar_type_ == STRING)
        output << '"' << EscapeString(string_value_) << "\"\n";
    else
        output << float_value_ << '\n';
}


Object::Object(const Object &rhs): Node(rhs) {
    for (const auto &name_and_node : rhs.fields_)
        fields_.emplace(name_and_node.first, name_and_node.second->clone());
}


Object::~Object() {
    for (const auto &name_and_value : fields_)
        delete name_and_value.second;
}


void Object::print(std::ostream &output, const unsigned /*indent*/) const {
    output << "ScalarNode::print NOT IMPLEMENTED!\n";
}


Array::Array(const Array &rhs): Node(rhs) {
    array_.reserve(rhs.array_.size());
    for (const auto &node : rhs.array_)
        array_.emplace_back(node->clone());
}


void Array::print(std::ostream &output, const unsigned indent) const {
    output << std::string(indent, ' ') << "[\n";
    for (unsigned i(0); i < size(); ++i)
        operator[](i).print(output, indent + 2);
    output << std::string(indent, ' ') << "]\n";
}


bool Parser::parse(const std::string &json_document, Node **root, std::string * const err_msg) {
std::cerr << "Entering Parser::parse, last_string_value_=\""<<last_string_value_<<"\"\n";
    line_no_ = 1;
    ch_ = json_document.cbegin();
    end_ = json_document.cend();
    pushed_back_ = false;

    std::string string_value;
    double float_value;
    const TokenType token_type(getToken(&string_value, &float_value, err_msg));
    switch (token_type) {
    case ERROR:
        return false;
    case STRING_CONSTANT:
        return parseScalar(string_value, root, err_msg);
    case OPEN_BRACE:
        return parseObject(root, err_msg);
    case OPEN_BRACKET:
        return parseArray(root, err_msg);
    default:
        *err_msg = "unexpected start of input!";
        return false;
    }
}


Parser::TokenType Parser::getToken(std::string * const string_value, double * const float_value,
                                   std::string * const err_msg)
{
std::cerr << "Entering Parser::getToken\n";
    if (pushed_back_) {
std::cerr << "In Parser::getToken, pushed_back_ was true.\n";
        pushed_back_ = false;
        *string_value = last_string_value_;
        *float_value = last_float_value_;
        *err_msg = last_err_msg_;
        return last_token_type_;
    }

    skipWhite();
    if (ch_ == end_)
        return END_OF_INPUT;

    switch (*ch_) {
    case '{':
        ++ch_;
        return OPEN_BRACE;
    case '}':
        ++ch_;
        return CLOSE_BRACE;
    case '[':
        ++ch_;
        return OPEN_BRACKET;
    case ']':
        ++ch_;
        return CLOSE_BRACKET;
    case ',':
        ++ch_;
        return COMMA;
    case ':':
        ++ch_;
        return COLON;
    case '"':
        ++ch_;
        return likely(parseString(string_value, err_msg)) ? STRING_CONSTANT : ERROR;
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
    case '+':
    case '-':
    case '.':
        return likely(parseFloat(float_value, err_msg)) ? FLOAT_CONSTANT : ERROR;
    default:
        *err_msg = "unexpected character in input '" + std::string(1, *ch_) + "' on line " + std::to_string(line_no_)
                   + "!";
        return ERROR;
    }
}


void Parser::ungetToken(const Parser::TokenType token_type, const std::string &string_value,
                        const double float_value, const std::string &err_msg)
{
    if (unlikely(pushed_back_))
        throw std::runtime_error("in Parser::ungetToken: attempted an illegal 2nd push back in a row!");

    last_token_type_ = token_type;
    last_string_value_ = string_value;
    last_float_value_ = float_value;
    last_err_msg_ = err_msg;

    pushed_back_ = true;
}


void Parser::skipWhite() {
    while (ch_ != end_ and (*ch_ == ' ' or *ch_ == '\t' or *ch_ == '\n')) {
        if (*ch_ == '\n')
            ++line_no_;
        ++ch_;
    }
}


bool Parser::parseString(std::string * const string_value, std::string * const err_msg) {
    string_value->clear();
    start_line_no_ = line_no_;
    while (unlikely(ch_ != end_) and *ch_ != '"') {
        if (*ch_ == '\\') {
            std::string expanded_escape;
            if (unlikely(not DecodeEscape(ch_, end_, &expanded_escape))) {
                *err_msg = "error while decoding an escape sequence on line " + std::to_string(line_no_) + "!";
                return false;
            }
            *string_value += expanded_escape;
        } else
            *string_value += *ch_;
        ++ch_;
    }

    if (unlikely(ch_ == end_)) {
        *err_msg = "unexpected end of input in string constant starting on line " + std::to_string(start_line_no_)
                   + "!";
        return false;
    }
    ++ch_;

    return true;
}


bool Parser::parseFloat(double * const float_value, std::string * const err_msg) {
    std::string float_candidate;
    while (ch_ != end_ and (std::strchr("0123456789.+-eE", *ch_) != nullptr))
        float_candidate += *ch_++;

    if (unlikely(not StringUtil::ToDouble(float_candidate, float_value))) {
        *err_msg = "failed to parse a FLOAT on line " + std::to_string(start_line_no_) + "!";
        return false;
    }

    return true;
}


bool Parser::parseScalar(const std::string &label, Node **root, std::string * const err_msg) {
    skipWhite();
    if (unlikely(ch_ == end_)) {
        *err_msg = "unexpected end of input while looking for a colon on line " + std::to_string(line_no_) + "!";
        return false;
    }

    if (unlikely(*ch_ != ':')) {
        *err_msg = "expected a colon as part of a scalar on line " + std::to_string(line_no_)
                   + " found '" + std::string(1, *ch_) + "' instead!";
        return false;
    }
    ++ch_;

    skipWhite();
    if (unlikely(ch_ == end_)) {
        *err_msg = "unexpected end of input after a colon on line " + std::to_string(line_no_) + "!";
        return false;   
    }

    if (*ch_ == '"') {
        ++ch_;
        std::string value;
        if (not parseString(&value, err_msg))
            return false;
        *root = new ScalarNode(label, value);
    } else {
        double value;
        if (not parseFloat(&value, err_msg))
            return false;
        *root = new ScalarNode(label, value);
    }

    return true;
}


bool Parser::parseObject(Node **/*root*/, std::string * const err_msg) {
    *err_msg = "Parser::parseObject NOT IMPLEMENTED YET!";
    return false;
}


bool Parser::parseArray(Node **root, std::string * const err_msg) {
std::cerr << "Entering Parser::parseArray\n";
    Array * const new_array(new Array());

    std::string string_value;
    double float_value;
    TokenType token_type(getToken(&string_value, &float_value, err_msg));
    if (token_type == CLOSE_BRACKET) { // We have an empty array.
        *root = new_array;
        return true;
    }
std::cerr << "in Parser::parseArray before call to ungetToken()\n";
    ungetToken(token_type, string_value, float_value, *err_msg);
std::cerr << "in Parser::parseArray after call to ungetToken()\n";

    for (;;) {
        Node *new_node(nullptr);

        // Here we should get an array element.
        token_type = getToken(&string_value, &float_value, err_msg);
        switch (token_type) {
        case ERROR: {
            delete new_array;
            return false;
        }
        case STRING_CONSTANT: {
            if (unlikely(not parseScalar(string_value, &new_node, err_msg))) {
                delete new_array;
                return false;
            }
            new_array->addElement(new_node);
            break;
        }
        case OPEN_BRACE: {
            if (unlikely(not parseObject(&new_node, err_msg))) {
                delete new_array;
                return false;
            }
            new_array->addElement(new_node); 
        }
        case OPEN_BRACKET: {
            if (unlikely(not parseArray(&new_node, err_msg))) {
                delete new_array;
                return false;
            }
            new_array->addElement(new_node);
            break;
        }
        default: {
            delete new_array;
            *err_msg = "unexpected input on line " + std::to_string(line_no_) + " while trying to parse an array!";
            return false;
        }
        }

        // Here we should get a comma if there are more array elements or,
        // a closing bracket if we're done w/ the array.
        token_type = getToken(&string_value, &float_value, err_msg);
        switch (token_type) {
        case ERROR: {
            delete new_array;
            *err_msg = "error while parsing an array: " + *err_msg;
            return false;
        }
        case COMMA:
            break;
        case CLOSE_BRACKET:
            *root = new_array;
            return true;
        default: {
            delete new_array;
            *err_msg = "unexpected input on line " + std::to_string(line_no_) + " after an array element!";
            return false;
        }
        }
    }
}


} // namespace JSON
