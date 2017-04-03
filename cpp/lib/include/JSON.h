/** \file   JSON.h
 *  \brief  Interface for JSON-related functionality.
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
#ifndef JSON_H
#define JSON_H


#include <string>
#include <unordered_map>
#include <vector>
#include <cinttypes>


namespace JSON {


enum TokenType { COMMA, COLON, OPEN_BRACE, CLOSE_BRACE, OPEN_BRACKET, CLOSE_BRACKET, TRUE_CONST, FALSE_CONST,
                 NULL_CONST, INTEGER_CONST, DOUBLE_CONST, STRING_CONST, END_OF_INPUT, ERROR };


class Scanner {
    std::string last_string_constant_;
    int64_t last_integer_constant_;
    double last_double_constant_;
    std::string last_error_message_;
    unsigned line_no_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
public:
    explicit Scanner(const std::string &json_document)
        : line_no_(1), ch_(json_document.cbegin()), end_(json_document.cend()) { }
    TokenType getToken();
    const std::string &getLastStringConstant() const { return last_string_constant_; }
    int64_t getLastIntegerConstant() const { return last_integer_constant_; }
    double getLastDoubleConstant() const { return last_double_constant_; }
    unsigned getLineNumber() const { return line_no_; }
    const std::string &getLastErrorMessage() const { return last_error_message_; }
private:
    void skipWhite();

    /** \return "success_token" if the characters of "sequence" where scanned, else ERROR.
     *  \note Sets last_error_message_, if it returns ERROR. */
    TokenType expectSequence(const std::string &sequence, const TokenType success_token);

    /** \return Upon success, either INTEGER_CONST, if the scanned number can be represented as a 64-bit integer, o/w
     *          DOUBLE_CONST.  Upon failure ERROR will be returned and last_error_message_ set accordingly. */
    TokenType parseNumber();

    bool UTF16EscapeToUTF8(std::string * const utf8);

    /** \return Either STRING_CONST upon success or ERROR upon failure. */
    TokenType parseStringConstant();
};


class JSONNode {
public:
    enum Type { BOOLEAN_NODE, NULL_NODE, STRING_NODE, OBJECT_NODE, ARRAY_NODE };
public:
    virtual ~JSONNode() { }

    virtual Type getType() const = 0;
    virtual std::string toString() const = 0;
};


class BooleanNode final : public JSONNode {
    bool value_;
public:
    explicit BooleanNode(const bool value): value_(value) { }

    virtual Type getType() const { return BOOLEAN_NODE; }
    virtual std::string toString() const { return value_ ? "true" : "false"; }
    bool getValue() const { return value_; }
};


class NullNode final : public JSONNode {
public:
    NullNode() { }

    virtual Type getType() const { return NULL_NODE; }
    virtual std::string toString() const { return "null"; }
};


class StringNode final : public JSONNode {
    std::string value_;
public:
    explicit StringNode(const std::string value): value_(value) { }

    virtual Type getType() const { return STRING_NODE; }
    virtual std::string toString() const { return value_; }
    const std::string &getValue() const { return value_; }
};


class ObjectNode final : public JSONNode {
    std::unordered_map<std::string, JSONNode *> entries_;
public:
    ObjectNode() { }
    virtual ~ObjectNode();
    virtual Type getType() const { return OBJECT_NODE; }
    virtual std::string toString() const;

    /** \return False if the new node was not inserted because the label already existed, o/w true. */
    bool insert(const std::string &label, JSONNode * const node);

    /** \return False if there was nothing to remove, o/w true. */
    bool remove(const std::string &label);

    // Member accessors, they return NULL if there is no entry for the provided label o/w they return the entry.
    const JSONNode *operator[](const std::string &label) const;
    JSONNode *operator[](const std::string &label);
};


class ArrayNode final : public JSONNode {
    std::vector<JSONNode *> values_;
public:
    typedef std::vector<JSONNode *>::const_iterator const_iterator;
public:
    explicit ArrayNode() { }
    virtual ~ArrayNode();

    virtual Type getType() const { return ARRAY_NODE; }
    virtual std::string toString() const;
    const JSONNode &getValue(const size_t index) const { return *values_[index]; }
    size_t size() const { return values_.size(); }
    const_iterator cbegin() const { return values_.cbegin(); }
    const_iterator cend() const { return values_.cend(); }
    void push_back(JSONNode * const node) { values_.push_back(node); }
};


} // namespace JSON


#endif // ifndef JSON_H
