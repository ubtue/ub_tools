/** \file   JSON.cc
 *  \brief  Implementation of JSON-related functionality.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <deque>
#include <stdexcept>
#include <string>
#include <cctype>
#include <cstdio>
#include "Compiler.h"
#include "StringUtil.h"
#include "TextUtil.h"


namespace JSON {


TokenType Scanner::getToken() {
    if (pushed_back_) {
        pushed_back_ = false;
        return pushed_back_token_;
    }

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
        return CLOSE_BRACE;
    case '[':
        ++ch_;
        return OPEN_BRACKET;
    case ']':
        ++ch_;
        return CLOSE_BRACKET;
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
        const std::string bad_char(::isprint(*ch_) ? std::string(1, *ch_)
                                   : "\\x" + StringUtil::ToHexString(static_cast<unsigned char>(*ch_)));
        last_error_message_ = "unexpected character '" + bad_char + "', offset into the input is " + std::to_string(ch_ - begin_)
                              + " bytes!";
        return ERROR;
    }
}


void Scanner::ungetToken(const TokenType token) {
    if (unlikely(pushed_back_))
        throw std::runtime_error("in JSON::Scanner::ungetToken: can't push back two tokens in a row!");
    pushed_back_token_ = token;
    pushed_back_ = true;
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

    if (ch_ == end_ or (*ch_ != '.' and *ch_ != 'e' and *ch_ != 'E')) {
        int64_t value;
        if (unlikely(not StringUtil::ToInt64T(number_as_string, &value))) {
            last_error_message_ = "failed to convert \"" + number_as_string + "\" to a 64-bit integer!";
            return ERROR;
        }

        last_integer_constant_ = value;
        return INTEGER_CONST;
    }

    if (*ch_ == '.') {
        number_as_string += '.';
        for (++ch_; ch_ != end_ and StringUtil::IsDigit(*ch_); ++ch_)
            number_as_string += *ch_;
    }

    if (ch_ == end_ or (*ch_ != 'e' and  *ch_ != 'E')) {
        double value;
        if (unlikely(not StringUtil::ToDouble(number_as_string, &value))) {
            last_error_message_ = "failed to convert \"" + number_as_string + "\" to a floating point value!";
            return ERROR;
        }

        last_double_constant_ = value;
        return DOUBLE_CONST;
    }

    if (*ch_ == 'e' or *ch_ == 'E')
        number_as_string += *ch_;

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


// Helper for parseStringConstant; copies a singe Unicode codepoint from ch to s.
// See https://en.wikipedia.org/wiki/UTF-8 in order to understand the implementation.
inline static void UTF8Advance(std::string::const_iterator &ch, std::string * const s) {
    if ((static_cast<unsigned char>(*ch) & 0b10000000u) == 0) { // High bit is not set.
        *s += *ch++;
    } else if ((static_cast<unsigned char>(*ch) & 0b11100000u) == 0b11000000u) {
        *s += *ch++;
        *s += *ch++;
    } else if ((static_cast<unsigned char>(*ch) & 0b11110000u) == 0b11100000u) {
        *s += *ch++;
        *s += *ch++;
        *s += *ch++;
    } else if ((static_cast<unsigned char>(*ch) & 0b11111000u) == 0b11110000u) {
        *s += *ch++;
        *s += *ch++;
        *s += *ch++;
        *s += *ch++;
    }
}


TokenType Scanner::parseStringConstant() {
    ++ch_; // Skip over initial double quote.

    const unsigned start_line_no(line_no_);
    std::string string_value;
    while (ch_ != end_ and *ch_ != '"') {
        if (*ch_ != '\\')
            UTF8Advance(ch_, &string_value);
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
                ++ch_;
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


std::string JSONNode::TypeToString(const Type type) {
    switch (type) {
    case BOOLEAN_NODE:
        return "BOOLEAN_NODE";
    case NULL_NODE:
        return "NULL_NODE";
    case STRING_NODE:
        return "STRING_NODE";
    case INT64_NODE:
        return "INT64_NODE";
    case DOUBLE_NODE:
        return "DOUBLE_NODE";
    case OBJECT_NODE:
        return "OBJECT_NODE";
    case ARRAY_NODE:
        return "ARRAY_NODE";
    default:
        throw std::runtime_error("in JSON::JSONNode::TypeToString: we should never get here!");
    };
}


std::string DoubleNode::toString() const {
    char as_string[30];
    std::sprintf(as_string, "%20G", value_);
    return as_string;
}


std::string StringNode::toString() const {
    return "\"" + EscapeString(value_) + "\"";
}


ObjectNode::ObjectNode(const std::string &object_as_string) {
    if (object_as_string.empty())
        return;

    std::shared_ptr<JSONNode> tree_root;
    Parser parser(object_as_string);
    if (unlikely(not parser.parse(&tree_root)))
        LOG_ERROR("failed to construct an ObjectNode instance from a string: " + parser.getErrorMessage());

    if (unlikely(tree_root->getType() != OBJECT_NODE))
        LOG_ERROR("incompatible JSON node type!");

    entries_.swap(reinterpret_cast<ObjectNode *>(tree_root.get())->entries_);
}


ObjectNode::ObjectNode(const std::unordered_map<std::string, std::string> &map) {
    for (const auto &key_and_value : map) {
        std::shared_ptr<JSON::StringNode> value_node(new JSON::StringNode(key_and_value.second));
        insert(key_and_value.first, value_node);
    }
}


ObjectNode::ObjectNode(const std::map<std::string, std::string> &map) {
    for (const auto &key_and_value : map) {
        std::shared_ptr<JSON::StringNode> value_node(new JSON::StringNode(key_and_value.second));
        insert(key_and_value.first, value_node);
    }
}

std::shared_ptr<JSONNode> ObjectNode::clone() const {
    std::shared_ptr<ObjectNode> the_clone(new ObjectNode);
    for (const auto &entry : entries_)
        the_clone->entries_[entry.first] = entry.second->clone();

    return the_clone;
}


std::string ObjectNode::toString() const {
    std::string as_string;
    as_string += "{ ";
    for (const auto &entry : entries_) {
        as_string += "\"" + EscapeString(entry.first) + "\"";
        as_string += ": ";
        as_string += entry.second->toString();
        as_string += ", ";
    }
    if (not entries_.empty())
        as_string.resize(as_string.size() - 2); // Strip off final comma+space.
    as_string += " }";

    return as_string;
}


bool ObjectNode::insert(const std::string &label, std::shared_ptr<JSONNode> node) {
    if (entries_.find(label) != entries_.end())
        return false;
    entries_.insert(std::make_pair(label, node));

    return true;
}


bool ObjectNode::remove(const std::string &label) {
    const auto entry(entries_.find(label));
    if (entry == entries_.cend())
        return false;
    entries_.erase(entry);
    return true;
}


bool ObjectNode::hasNode(const std::string &label) const {
    const auto entry(entries_.find(label));
    return entry != entries_.cend();
}


std::shared_ptr<const JSONNode> ObjectNode::getNode(const std::string &label) const {
    const auto entry(entries_.find(label));
    return entry == entries_.cend() ? nullptr : entry->second;
}


std::shared_ptr<JSONNode> ObjectNode::getNode(const std::string &label) {
    const auto entry(entries_.find(label));
    return entry == entries_.cend() ? nullptr : entry->second;
}


std::string ObjectNode::getOptionalStringValue(const std::string &label, const std::string &default_value) const {
    const auto entry(entries_.find(label));
    if (entry == entries_.cend())
        return default_value;

    const Type node_type(entry->second->getType());
    if (node_type == STRING_NODE) {
        const auto string_node(reinterpret_cast<const StringNode *>(entry->second.get()));
        return string_node->getValue(); // Can't use toString() as that would add quotes!
    } else if (node_type == OBJECT_NODE or node_type == ARRAY_NODE)
        LOG_ERROR("node for label \"" + label + "\" is not a scalar type!");
    else
        return entry->second->toString();
}


static size_t ParsePath(const std::string &path, std::deque<std::string> * const components, const bool path_is_absolute) {
    if (unlikely(path_is_absolute and not StringUtil::StartsWith(path, "/")))
        throw std::runtime_error("in JSON::ParsePath: path must start with a slash!");

    std::string component;
    bool escaped(false);
    for (const char ch : (path_is_absolute ? path.substr(1) : path)) {
        if (escaped) {
            component += ch;
            escaped = false;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == '/') {
            if (unlikely(component.empty()))
                throw std::runtime_error("in JSON::ParsePath: detected an empty path component!");
            components->emplace_back(component);
            component.clear();
        } else
            component += ch;
    }

    if (not component.empty())
        components->emplace_back(component);

    return components->size();
}


std::shared_ptr<const JSONNode> ObjectNode::deepResolveNode(const std::string &path) const {
    std::deque<std::string> path_components;
    if (unlikely(ParsePath(path, &path_components, /* path_is_absolute = */false) == 0))
        throw std::runtime_error("in JSON::ObjectNode::deepResolveNode: an empty path is invalid!");

    std::shared_ptr<const JSONNode> next_node(this->getNode(path_components.front()));
    path_components.pop_front();
    for (const auto &path_component : path_components) {
        if (next_node == nullptr)
            return nullptr;

        switch (next_node->getType()) {
        case JSONNode::OBJECT_NODE:
            if ((next_node = JSONNode::CastToObjectNodeOrDie("next_node", next_node)->getNode(path_component)) == nullptr) {
                    throw std::runtime_error("in JSON::ObjectNode::deepResolveNode: path component \"" + path_component
                                             + " is not a key in an object node! (path: " + path+ ")");
            }
            break;
        case JSONNode::ARRAY_NODE: {
            unsigned index;
            if (unlikely(not StringUtil::ToUnsigned(path_component, &index))) {
                throw std::runtime_error("in JSON::ObjectNode::deepResolveNode: path component \"" + path_component
                                         + "\" in path \"" + path + "\" can't be converted to an array index!");
            }
            const std::shared_ptr<const ArrayNode> array_node(JSONNode::CastToArrayNodeOrDie("next_node", next_node));
            if (unlikely(index >= array_node->size()))
                return nullptr;
            next_node = array_node->getNode(index);
            break;
        }
        case JSONNode::NULL_NODE:
            return nullptr;
        default:
            throw std::runtime_error("in JSON::ObjectNode::deepResolveNode: can't descend into a scalar node! (path: "
                                     + path + ")");
            return nullptr;
        }
    }

    return next_node;
}


bool ObjectNode::isNullNode(const std::string &label) const {
    const auto entry(entries_.find(label));
    if (unlikely(entry == entries_.cend()))
        LOG_ERROR("label \"" + label + "\" not found!");
    return entry->second->getType() == NULL_NODE;
}


std::shared_ptr<JSONNode> ArrayNode::clone() const {
    std::shared_ptr<ArrayNode> the_clone(new ArrayNode);
    the_clone->values_.reserve(values_.size());
    for (const auto &node : values_)
        the_clone->values_.emplace_back(node->clone());

    return the_clone;
}


std::string ArrayNode::toString() const {
    std::string as_string;
    as_string += "[ ";
    for (const auto &value : values_) {
        as_string += value->toString();
        as_string += ", ";
    }
    if (not values_.empty())
        as_string.resize(as_string.size() - 2); // Strip off final comma+space.
    as_string += " ]";

    return as_string;
}


std::shared_ptr<const JSONNode> ArrayNode::getNode(const size_t index) const {
    if (unlikely(index >= values_.size()))
        LOG_ERROR("index (" + std::to_string(index) + ") exceeds size of ArrayNode ("
                  + std::to_string(values_.size()) + ")!");

    return values_[index];
}


std::shared_ptr<JSONNode> ArrayNode::getNode(const size_t index) {
    if (unlikely(index >= values_.size()))
        LOG_ERROR("index (" + std::to_string(index) + ") exceeds size of ArrayNode ("
                  + std::to_string(values_.size()) + ")!");

    return values_[index];
}


bool ArrayNode::isNullNode(const size_t index) const {
    if (unlikely(index >= values_.size()))
        LOG_ERROR("index " + std::to_string(index) + " out of range [0," + std::to_string(values_.size()) + ")!");
    return values_[index]->getType() == NULL_NODE;
}


bool Parser::parseObject(std::shared_ptr<JSONNode> * const new_object_node) {
    *new_object_node = std::shared_ptr<ObjectNode>(new ObjectNode());
    TokenType token(scanner_.getToken());
    if (unlikely(token == CLOSE_BRACE))
        return true; // We have an empty object.

    for (;;) {
        if (unlikely(token != STRING_CONST)) {
            error_message_ = "label expected on line " + std::to_string(scanner_.getLineNumber())
                             + " found '" + TokenTypeToString(token) + "' instead!";
            return false;
        }
        const std::string label(scanner_.getLastStringConstant());

        token = scanner_.getToken();
        if (unlikely(token != COLON)) {
            error_message_ = "colon expected after label on line " + std::to_string(scanner_.getLineNumber())
                + " found '" + TokenTypeToString(token) + "' instead!";
            return false;
        }

        std::shared_ptr<JSONNode> new_node;
        if (unlikely(not parseAny(&new_node)))
            return false;

        JSONNode::CastToObjectNodeOrDie("new_object_node", *new_object_node)->insert(label, new_node);

        token = scanner_.getToken();
        if (token == COMMA)
            token = scanner_.getToken();
        else if (token == CLOSE_BRACE)
            return true;
        else {
            error_message_ = "expected ',' or '}' on line " + std::to_string(scanner_.getLineNumber())
                             + " but found '" + TokenTypeToString(token) + "!";
            return false;
        }
    }
}


bool Parser::parseArray(std::shared_ptr<JSONNode> * const new_array_node) {
    *new_array_node = std::shared_ptr<ArrayNode>(new ArrayNode());
    TokenType token(scanner_.getToken());
    if (unlikely(token == CLOSE_BRACKET))
        return true; // Empty array.
    scanner_.ungetToken(token);

    for (;;) {
        std::shared_ptr<JSONNode> new_node(nullptr);
        if (unlikely(not parseAny(&new_node)))
            return false;
        JSONNode::CastToArrayNodeOrDie("new_array_node", *new_array_node)->push_back(new_node);


        token = scanner_.getToken();
        if (token == COMMA)
            /* Intentionally empty! */;
        else if (token == CLOSE_BRACKET)
            return true;
        else {
            error_message_ = "expected ',' or ']' on line " + std::to_string(scanner_.getLineNumber())
                             + " but found '" + TokenTypeToString(token) + "!";
            return false;
        }
    }

    return true;
}


bool Parser::parseAny(std::shared_ptr<JSONNode> * const new_node) {
    *new_node = nullptr;

    TokenType token(scanner_.getToken());
    switch (token) {
    case OPEN_BRACE:
        return parseObject(new_node);
    case OPEN_BRACKET:
        return parseArray(new_node);
    case INTEGER_CONST:
        *new_node = std::make_shared<IntegerNode>(scanner_.getLastIntegerConstant());
        return true;
    case DOUBLE_CONST:
        *new_node = std::make_shared<DoubleNode>(scanner_.getLastDoubleConstant());
        return true;
    case STRING_CONST:
        *new_node = std::make_shared<StringNode>(scanner_.getLastStringConstant());
        return true;
    case TRUE_CONST:
        *new_node = std::make_shared<BooleanNode>(true);
        return true;
    case FALSE_CONST:
        *new_node = std::make_shared<BooleanNode>(false);
        return true;
    case NULL_CONST:
        *new_node = std::make_shared<NullNode>();
        return true;
    case ERROR:
        error_message_ = scanner_.getLastErrorMessage() + "(line: " + std::to_string(scanner_.getLineNumber())
                         + ")";
        return false;
    case END_OF_INPUT:
        error_message_ = "unexpected end of input!";
        return false;
    default:
        error_message_ = "syntax error, found '" + TokenTypeToString(token)
                         + "' but expected some kind of object on line " + std::to_string(scanner_.getLineNumber())
                         + "!";
        return false;
    }
}


bool Parser::parse(std::shared_ptr<JSONNode> * const tree_root) {
    if (unlikely(not parseAny(tree_root)))
        return false;

    const TokenType token(scanner_.getToken());
    if (likely(token == END_OF_INPUT))
        return true;

    error_message_ = "found trailing garbage " + TokenTypeToString(token) + " on line "
                     + std::to_string(scanner_.getLineNumber()) + "!";
    return false;
}


std::string TokenTypeToString(const TokenType token) {
    switch (token) {
    case COMMA:
        return ",";
    case COLON:
        return ":";
    case OPEN_BRACE:
        return "{";
    case CLOSE_BRACE:
        return "}";
    case OPEN_BRACKET:
        return "[";
    case CLOSE_BRACKET:
        return "]";
    case TRUE_CONST:
        return "true";
    case FALSE_CONST:
        return "false";
    case NULL_CONST:
        return "null";
    case INTEGER_CONST:
        return "integer";
    case DOUBLE_CONST:
        return "double";
    case STRING_CONST:
        return "string";
    case END_OF_INPUT:
        return "end-of-input";
    case ERROR:
        return "error";
    }

    __builtin_unreachable();
}


// N.B. If "throw_if_not_found" is true an exception will be thrown if "path" can't be resolved o/w NULL will be returned.
// Please note that a syntactically incorrect "path" will trigger an exception irrespective of the value of "throw_if_not_found."
template<class JSONNode> static std::shared_ptr<JSONNode> GetLastPathComponent(
    const std::string &path, const std::shared_ptr<JSONNode> &tree, const bool throw_if_not_found)
{
    std::deque<std::string> path_components;
    if (unlikely(ParsePath(path, &path_components, /* path_is_absolute = */true) == 0))
        throw std::runtime_error("in JSON::GetLastPathComponent: an empty path is invalid!");

    std::shared_ptr<JSONNode> next_node(tree);
    for (const auto &path_component : path_components) {
        if (next_node == nullptr) {
            if (throw_if_not_found)
                throw std::runtime_error("in JSON::GetLastPathComponent: can't find \"" + path + "\" in our JSON tree!");
            return nullptr;
        }

        switch (next_node->getType()) {
        case JSONNode::OBJECT_NODE:
            if ((next_node = JSONNode::CastToObjectNodeOrDie("next_node", next_node)->getNode(path_component)) == nullptr) {
                if (throw_if_not_found)
                    throw std::runtime_error("in JSON::GetLastPathComponent: path component \"" + path_component
                                             + " is not a key in an object node! (path: " + path + ")");
            }
            break;
        case JSONNode::ARRAY_NODE: {
            unsigned index;
            if (unlikely(not StringUtil::ToUnsigned(path_component, &index))) {
                if (throw_if_not_found)
                    throw std::runtime_error("in JSON::GetLastPathComponent: path component \"" + path_component
                                             + "\" in path \"" + path + "\" can't be converted to an array index!");
                return nullptr;
            }
            const std::shared_ptr<const ArrayNode> array_node(JSONNode::CastToArrayNodeOrDie("next_node", next_node));
            if (unlikely(index >= array_node->size())) {
                if (throw_if_not_found)
                    throw std::runtime_error("in JSON::GetLastPathComponent: path component \"" + path_component
                                             + "\" in path \"" + path + "\" is too large as an array index!");
                return nullptr;
            }
            next_node = array_node->getNode(index);
            break;
        }
        default:
            if (throw_if_not_found)
                throw std::runtime_error("in JSON::GetLastPathComponent: can't descend into a scalar node! (path: "
                                         + path + ")");
            return nullptr;
        }
    }

    return next_node;
}


std::shared_ptr<const JSONNode> LookupNode(const std::string &path, const std::shared_ptr<const JSONNode> &tree) {
    return GetLastPathComponent(path, tree, /* throw_if_not_found = */true);
}


static std::string LookupString(const std::string &path, const std::shared_ptr<const JSONNode> &tree,
                                const std::string &default_value, const bool use_default_value)
{
    const std::shared_ptr<const JSONNode> bottommost_node(GetLastPathComponent(path, tree, /* throw_if_not_found = */not use_default_value));
    if (bottommost_node == nullptr) {
        if (use_default_value)
            return default_value;
        throw std::runtime_error("in JSON::LookupString: missing bottom node!");
    }

    switch (bottommost_node->getType()) {
    case JSONNode::BOOLEAN_NODE:
        return JSONNode::CastToBooleanNodeOrDie("bottommost_node", bottommost_node)->getValue() ? "true" : "false";
    case JSONNode::NULL_NODE:
        return "null";
    case JSONNode::STRING_NODE:
        return JSONNode::CastToStringNodeOrDie("bottommost_node", bottommost_node)->getValue();
    case JSONNode::INT64_NODE:
        return std::to_string(JSONNode::CastToIntegerNodeOrDie("bottommost_node", bottommost_node)->getValue());
    case JSONNode::DOUBLE_NODE:
        return std::to_string(JSONNode::CastToDoubleNodeOrDie("bottommost_node", bottommost_node)->getValue());
    case JSONNode::OBJECT_NODE:
        throw std::runtime_error("in JSON::LookupString: can't get a unique value from an object node!");
    case JSONNode::ARRAY_NODE:
        throw std::runtime_error("in JSON::LookupString: can't get a unique value from an array node!");
    }

    __builtin_unreachable();
}


std::string LookupString(const std::string &path, const std::shared_ptr<const JSONNode> &tree) {
    return LookupString(path, tree, "", /* use_default_value = */ false);
}


std::string LookupString(const std::string &path, const std::shared_ptr<const JSONNode> &tree, const std::string &default_value) {
    return LookupString(path, tree, default_value, /* use_default_value = */ true);
}


static bool LookupStringsHelper(std::deque<std::string> path_components, const std::shared_ptr<const JSONNode> &tree,
                                std::vector<std::string> * const results)
{
    std::shared_ptr<const JSONNode> next_node(tree);
    while (not path_components.empty()) {
        if (next_node == nullptr)
            return false;

        const std::string path_component(path_components.front());
        path_components.pop_front();

        switch (next_node->getType()) {
        case JSONNode::OBJECT_NODE:
            next_node = JSONNode::CastToObjectNodeOrDie(path_component, next_node)->getNode(path_component);
            if (next_node == nullptr)
                return true;
            break;
        case JSONNode::ARRAY_NODE:
            if (path_component == "*") {
                const auto array_node(JSONNode::CastToArrayNodeOrDie(path_component, next_node));
                for (const auto &entry : *array_node) {
                    if (not LookupStringsHelper(path_components, entry, results))
                        return false;
                }
                return true;
            } else { // Must be an array index.
                unsigned index;
                if (unlikely(not StringUtil::ToUnsigned(path_component, &index))) {
                    LOG_WARNING("bad index: " + path_component);
                    return false;
                }
                const std::shared_ptr<const ArrayNode> array_node(JSONNode::CastToArrayNodeOrDie(path_component, next_node));
                if (unlikely(index >= array_node->size())) {
                    LOG_WARNING("index too large: " + path_component);
                    return false;
                }
                next_node = array_node->getNode(index);
            }
            break;
        default:
            return false;
        }
    }

    results->emplace_back(next_node->toString());
    return true;
}


std::vector<std::string> LookupStrings(const std::string &path, const std::shared_ptr<const JSONNode> &tree) {
    std::deque<std::string> path_components;
    if (unlikely(ParsePath(path, &path_components, /* path_is_absolute = */true) == 0))
        throw std::runtime_error("in JSON::LookupStringsHelper: an empty path is invalid!");

    std::vector<std::string> results;
    return LookupStringsHelper(path_components, tree, &results) ? results : std::vector<std::string>{};
}


static int64_t LookupInteger(const std::string &path, const std::shared_ptr<const JSONNode> &tree, const int64_t default_value,
                             const bool use_default_value)
{
    const std::shared_ptr<const JSONNode> bottommost_node(GetLastPathComponent(path, tree, /* throw_if_not_found = */not use_default_value));
    if (bottommost_node == nullptr)
        return default_value;

    switch (bottommost_node->getType()) {
    case JSONNode::BOOLEAN_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't convert a boolean value to an integer!");
    case JSONNode::NULL_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't convert \"null\" to an integer!");
    case JSONNode::STRING_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't convert a string value to an integer!");
    case JSONNode::INT64_NODE:
        return JSONNode::CastToIntegerNodeOrDie("bottommost_node", bottommost_node)->getValue();
    case JSONNode::DOUBLE_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't convert a double value to an integer!");
    case JSONNode::OBJECT_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't get a unique value from an object node!");
    case JSONNode::ARRAY_NODE:
        throw std::runtime_error("in JSON::LookupInteger: can't get a unique value from an array node!");
    }

    __builtin_unreachable();
}


int64_t LookupInteger(const std::string &path, const std::shared_ptr<const JSONNode> &tree) {
    return LookupInteger(path, tree, 0L, /* use_default_value = */ false);
}


int64_t LookupInteger(const std::string &path, const std::shared_ptr<const JSONNode> &tree, const int64_t default_value) {
    return LookupInteger(path, tree, default_value, /* use_default_value = */ true);
}


// See https://www.ietf.org/rfc/rfc4627.txt section 2.5 in order to understand this.
std::string EscapeString(const std::string &unescaped_string) {
    std::string escaped_string;
    for (const char ch : unescaped_string) {
        switch (ch) {
        case '\\':
            escaped_string += "\\\\";
            break;
        case '"':
            escaped_string += "\\\"";
            break;
        case '/':
            escaped_string += "\\/";
            break;
        case '\b':
            escaped_string += "\\b";
            break;
        case '\f':
            escaped_string += "\\f";
            break;
        case '\n':
            escaped_string += "\\n";
            break;
        case '\r':
            escaped_string += "\\r";
            break;
        case '\t':
            escaped_string += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(ch) > 0x1Fu)
                escaped_string += ch;
            else { // Escape control characters.
                escaped_string += "\\u00";
                escaped_string += StringUtil::ToHex(static_cast<unsigned char>(ch) >> 4u);
                escaped_string += StringUtil::ToHex(static_cast<unsigned char>(ch) & 0xFu);
            }
        }
    }

    return escaped_string;
}


bool IsValidUTF8(const JSONNode &node) {
    switch (node.getType()) {
    case JSONNode::OBJECT_NODE: {
        for (const auto &key_and_node : reinterpret_cast<const ObjectNode &>(node)) {
            if (unlikely(not TextUtil::IsValidUTF8(key_and_node.first) or not IsValidUTF8(*key_and_node.second)))
                 return false;
        }
        return true;

    }
    case JSONNode::ARRAY_NODE: {
        for (const auto &entry : reinterpret_cast<const ArrayNode &>(node)) {
            if (unlikely(not IsValidUTF8(*entry)))
                return false;
        }
        return true;
    }
    case JSONNode::BOOLEAN_NODE:
    case JSONNode::NULL_NODE:
    case JSONNode::INT64_NODE:
    case JSONNode::DOUBLE_NODE:
        return true;
    case JSONNode::STRING_NODE:
        return TextUtil::IsValidUTF8(reinterpret_cast<const StringNode &>(node).getValue());
    }

    LOG_ERROR("we should *never* get here!");
}


} // namespace JSON
