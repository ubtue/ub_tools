/** \file   JSON.h
 *  \brief  Interface for JSON-related functionality.
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
#pragma once


#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cinttypes>
#include "util.h"


namespace JSON {


enum TokenType {
    COMMA,
    COLON,
    OPEN_BRACE,
    CLOSE_BRACE,
    OPEN_BRACKET,
    CLOSE_BRACKET,
    TRUE_CONST,
    FALSE_CONST,
    NULL_CONST,
    INTEGER_CONST,
    DOUBLE_CONST,
    STRING_CONST,
    END_OF_INPUT,
    ERROR
};


class Scanner {
    std::string last_string_constant_;
    int64_t last_integer_constant_;
    double last_double_constant_;
    std::string last_error_message_;
    unsigned line_no_;
    std::string::const_iterator ch_;
    const std::string::const_iterator begin_;
    const std::string::const_iterator end_;
    bool pushed_back_;
    TokenType pushed_back_token_;

public:
    explicit Scanner(const std::string &json_document)
        : line_no_(1), ch_(json_document.cbegin()), begin_(json_document.cbegin()), end_(json_document.cend()), pushed_back_(false) { }
    TokenType getToken();
    void ungetToken(const TokenType token);
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


// Forward declarations:
class ArrayNode;
class BooleanNode;
class DoubleNode;
class IntegerNode;
class ObjectNode;
class StringNode;


class JSONNode {
public:
    enum Type { BOOLEAN_NODE, NULL_NODE, STRING_NODE, INT64_NODE, DOUBLE_NODE, OBJECT_NODE, ARRAY_NODE };

public:
    virtual ~JSONNode() { }

    virtual Type getType() const = 0;
    virtual std::shared_ptr<JSONNode> clone() const = 0;
    virtual std::string toString() const = 0;
    static std::string TypeToString(const Type type);

private:
    template <typename NodeType>
    static std::shared_ptr<const NodeType> CastToConstNodeOrDie(const std::string &node_name, const Type node_type,
                                                                const std::shared_ptr<const JSONNode> &node) {
        if (unlikely(node->getType() != node_type))
            LOG_ERROR("expected \"" + node_name + "\" to be " + JSONNode::TypeToString(node_type) + "!");
        return std::static_pointer_cast<const NodeType>(node);
    }

    template <typename NodeType>
    static std::shared_ptr<NodeType> CastToNodeOrDie(const std::string &node_name, const Type node_type,
                                                     const std::shared_ptr<JSONNode> &node) {
        if (unlikely(node->getType() != node_type))
            LOG_ERROR("expected \"" + node_name + "\" to be " + JSONNode::TypeToString(node_type) + "!");
        return std::static_pointer_cast<NodeType>(node);
    }

public:
    static std::shared_ptr<const ArrayNode> CastToArrayNodeOrDie(const std::string &node_name,
                                                                 const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<ArrayNode>(node_name, ARRAY_NODE, node);
    };
    static std::shared_ptr<ArrayNode> CastToArrayNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<ArrayNode>(node_name, ARRAY_NODE, node);
    };
    static std::shared_ptr<const BooleanNode> CastToBooleanNodeOrDie(const std::string &node_name,
                                                                     const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<BooleanNode>(node_name, BOOLEAN_NODE, node);
    };
    static std::shared_ptr<BooleanNode> CastToBooleanNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<BooleanNode>(node_name, BOOLEAN_NODE, node);
    };
    static std::shared_ptr<const DoubleNode> CastToDoubleNodeOrDie(const std::string &node_name,
                                                                   const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<DoubleNode>(node_name, DOUBLE_NODE, node);
    };
    static std::shared_ptr<DoubleNode> CastToDoubleNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<DoubleNode>(node_name, DOUBLE_NODE, node);
    };
    static std::shared_ptr<const IntegerNode> CastToIntegerNodeOrDie(const std::string &node_name,
                                                                     const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<IntegerNode>(node_name, INT64_NODE, node);
    };
    static std::shared_ptr<IntegerNode> CastToIntegerNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<IntegerNode>(node_name, INT64_NODE, node);
    };
    static std::shared_ptr<const ObjectNode> CastToObjectNodeOrDie(const std::string &node_name,
                                                                   const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<ObjectNode>(node_name, OBJECT_NODE, node);
    };
    static std::shared_ptr<ObjectNode> CastToObjectNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<ObjectNode>(node_name, OBJECT_NODE, node);
    };
    static std::shared_ptr<const StringNode> CastToStringNodeOrDie(const std::string &node_name,
                                                                   const std::shared_ptr<const JSONNode> &node) {
        return CastToConstNodeOrDie<StringNode>(node_name, STRING_NODE, node);
    };
    static std::shared_ptr<StringNode> CastToStringNodeOrDie(const std::string &node_name, const std::shared_ptr<JSONNode> &node) {
        return CastToNodeOrDie<StringNode>(node_name, STRING_NODE, node);
    };
};


class BooleanNode final : public JSONNode {
    bool value_;

public:
    explicit BooleanNode(const bool value): value_(value) { }

    inline virtual Type getType() const override { return BOOLEAN_NODE; }
    inline virtual std::shared_ptr<JSONNode> clone() const override { return std::make_shared<BooleanNode>(value_); }
    inline virtual std::string toString() const override { return value_ ? "true" : "false"; }
    inline bool getValue() const { return value_; }
    void setValue(const bool value) { value_ = value; }
};


class NullNode final : public JSONNode {
public:
    NullNode() { }

    inline virtual Type getType() const override { return NULL_NODE; }
    inline virtual std::shared_ptr<JSONNode> clone() const override { return std::make_shared<NullNode>(); }
    inline virtual std::string toString() const override { return "null"; }
};


class StringNode final : public JSONNode {
    std::string value_;

public:
    explicit StringNode(const std::string &value): value_(value) { }

    inline virtual Type getType() const override { return STRING_NODE; }
    inline virtual std::shared_ptr<JSONNode> clone() const override { return std::make_shared<StringNode>(value_); }
    virtual std::string toString() const override;
    inline const std::string &getValue() const { return value_; }
    inline void setValue(const std::string &new_value) { value_ = new_value; }
};


class IntegerNode final : public JSONNode {
    int64_t value_;

public:
    explicit IntegerNode(const int64_t value): value_(value) { }

    inline virtual std::shared_ptr<JSONNode> clone() const override { return std::make_shared<IntegerNode>(value_); }
    inline virtual Type getType() const override { return INT64_NODE; }
    inline virtual std::string toString() const override { return std::to_string(value_); }
    inline int64_t getValue() const { return value_; }
    void setValue(const int64_t value) { value_ = value; }
};


class DoubleNode final : public JSONNode {
    double value_;

public:
    explicit DoubleNode(const double value): value_(value) { }

    virtual std::shared_ptr<JSONNode> clone() const override { return std::make_shared<DoubleNode>(value_); }
    virtual Type getType() const override { return DOUBLE_NODE; }
    virtual std::string toString() const override;
    double getValue() const { return value_; }
    void setValue(const double value) { value_ = value; }
};


class ObjectNode final : public JSONNode {
    std::unordered_map<std::string, std::shared_ptr<JSONNode>> entries_;

public:
    typedef std::unordered_map<std::string, std::shared_ptr<JSONNode>>::const_iterator const_iterator;
    template <typename ReturnType>
    std::shared_ptr<ReturnType> getNode(const std::string &label, const Type node_type) const {
        const auto entry(entries_.find(label));
        if (unlikely(entry == entries_.cend()))
            LOG_ERROR("label \"" + label + "\" not found!");
        if (unlikely(entry->second->getType() != node_type))
            LOG_ERROR("node for label \"" + label + "\" is not of type " + JSONNode::TypeToString(node_type) + "!");
        return std::static_pointer_cast<ReturnType>(entry->second);
    }
    template <typename ReturnType>
    std::shared_ptr<ReturnType> getOptionalNode(const std::string &label, const Type node_type) const {
        const auto entry(entries_.find(label));
        if (unlikely(entry == entries_.cend()))
            return nullptr;
        if (unlikely(entry->second->getType() != node_type))
            LOG_ERROR("node for label \"" + label + "\" is not of type " + JSONNode::TypeToString(node_type) + "!");
        return std::static_pointer_cast<ReturnType>(entry->second);
    }
    template <typename ReturnType, typename NodeType>
    ReturnType getValue(const std::string &label, const Type node_type) const {
        std::shared_ptr<const NodeType> node(getNode<NodeType>(label, node_type));
        return node->getValue();
    }
    template <typename ReturnType, typename NodeType>
    ReturnType getOptionalValue(const std::string &label, const ReturnType default_value, const Type node_type) const {
        const auto entry(entries_.find(label));
        if (entry == entries_.cend())
            return default_value;
        if (unlikely(entry->second->getType() != node_type))
            LOG_ERROR("node for label \"" + label + "\" is not of type " + JSONNode::TypeToString(node_type) + "!");
        return std::static_pointer_cast<NodeType>(entry->second)->getValue();
    }

public:
    ObjectNode(const std::string &object_as_string = "");
    ObjectNode(const std::unordered_map<std::string, std::string> &map);
    ObjectNode(const std::map<std::string, std::string> &map);


    virtual std::shared_ptr<JSONNode> clone() const override;
    virtual Type getType() const override { return OBJECT_NODE; }
    virtual std::string toString() const override;
    bool empty() const { return entries_.empty(); }

    /** \return False if the new node was not inserted because the label already existed, o/w true. */
    bool insert(const std::string &label, std::shared_ptr<JSONNode> const node);

    /** \return False if there was nothing to remove, o/w true. */
    bool remove(const std::string &label);

    /** \return False if no entry for the provided label exists, o/w true. */
    bool hasNode(const std::string &label) const;

    // Member accessors, they return NULL if there is no entry for the provided label o/w they return the entry.
    std::shared_ptr<const JSONNode> getNode(const std::string &label) const;
    std::shared_ptr<JSONNode> getNode(const std::string &label);

    /** \brief  Recursive lookup
     *  \param  path a path relative to the current object
     *  \return The resolved node or NULL if no entity referenced by the path was found.
     */
    std::shared_ptr<const JSONNode> deepResolveNode(const std::string &path) const;

    // Automatic cast value retrieval.  If the requested type is not applicable, the functions abort.
    std::shared_ptr<const ArrayNode> getArrayNode(const std::string &label) const { return getNode<ArrayNode>(label, ARRAY_NODE); }
    std::shared_ptr<ArrayNode> getArrayNode(const std::string &label) { return getNode<ArrayNode>(label, ARRAY_NODE); }
    std::shared_ptr<const BooleanNode> getBooleanNode(const std::string &label) const { return getNode<BooleanNode>(label, BOOLEAN_NODE); }
    std::shared_ptr<BooleanNode> getBooleanNode(const std::string &label) { return getNode<BooleanNode>(label, BOOLEAN_NODE); }
    std::shared_ptr<const DoubleNode> getDoubleNode(const std::string &label) const { return getNode<DoubleNode>(label, DOUBLE_NODE); }
    std::shared_ptr<DoubleNode> getDoubleNode(const std::string &label) { return getNode<DoubleNode>(label, DOUBLE_NODE); }
    std::shared_ptr<const IntegerNode> getIntegerNode(const std::string &label) const { return getNode<IntegerNode>(label, INT64_NODE); }
    std::shared_ptr<IntegerNode> getIntegerNode(const std::string &label) { return getNode<IntegerNode>(label, INT64_NODE); }
    std::shared_ptr<const ObjectNode> getObjectNode(const std::string &label) const { return getNode<ObjectNode>(label, OBJECT_NODE); }
    std::shared_ptr<ObjectNode> getObjectNode(const std::string &label) { return getNode<ObjectNode>(label, OBJECT_NODE); }
    std::shared_ptr<const StringNode> getStringNode(const std::string &label) const { return getNode<StringNode>(label, STRING_NODE); }
    std::shared_ptr<StringNode> getStringNode(const std::string &label) { return getNode<StringNode>(label, STRING_NODE); }
    bool isNullNode(const std::string &label) const;

    // Automatic cast value retrieval.  Returns nullptr if node not found.  If the requested type is not applicable, the functions abort.
    std::shared_ptr<const ArrayNode> getOptionalArrayNode(const std::string &label) const {
        return getOptionalNode<ArrayNode>(label, ARRAY_NODE);
    }
    std::shared_ptr<ArrayNode> getOptionalArrayNode(const std::string &label) { return getOptionalNode<ArrayNode>(label, ARRAY_NODE); }
    std::shared_ptr<const BooleanNode> getOptionalBooleanNode(const std::string &label) const {
        return getOptionalNode<BooleanNode>(label, BOOLEAN_NODE);
    }
    std::shared_ptr<BooleanNode> getOptionalBooleanNode(const std::string &label) {
        return getOptionalNode<BooleanNode>(label, BOOLEAN_NODE);
    }
    std::shared_ptr<const DoubleNode> getOptionalDoubleNode(const std::string &label) const {
        return getNode<DoubleNode>(label, DOUBLE_NODE);
    }
    std::shared_ptr<DoubleNode> getOptionalDoubleNode(const std::string &label) { return getOptionalNode<DoubleNode>(label, DOUBLE_NODE); }
    std::shared_ptr<const IntegerNode> getOptionalIntegerNode(const std::string &label) const {
        return getOptionalNode<IntegerNode>(label, INT64_NODE);
    }
    std::shared_ptr<IntegerNode> getOptionalIntegerNode(const std::string &label) {
        return getOptionalNode<IntegerNode>(label, INT64_NODE);
    }
    std::shared_ptr<const ObjectNode> getOptionalObjectNode(const std::string &label) const {
        return getOptionalNode<ObjectNode>(label, OBJECT_NODE);
    }
    std::shared_ptr<ObjectNode> getOptionalObjectNode(const std::string &label) { return getOptionalNode<ObjectNode>(label, OBJECT_NODE); }
    std::shared_ptr<const StringNode> getOptionalStringNode(const std::string &label) const {
        return getOptionalNode<StringNode>(label, STRING_NODE);
    }
    std::shared_ptr<StringNode> getOptionalStringNode(const std::string &label) { return getOptionalNode<StringNode>(label, STRING_NODE); }

    bool getBooleanValue(const std::string &label) const { return getValue<bool, BooleanNode>(label, BOOLEAN_NODE); }
    double getDoubleValue(const std::string &label) const { return getValue<double, DoubleNode>(label, DOUBLE_NODE); }
    int64_t getIntegerValue(const std::string &label) const { return getValue<int64_t, IntegerNode>(label, INT64_NODE); }
    std::string getStringValue(const std::string &label) const { return getValue<std::string, StringNode>(label, STRING_NODE); }

    bool getOptionalBooleanValue(const std::string &label, const bool default_value) const {
        return getOptionalValue<bool, BooleanNode>(label, default_value, BOOLEAN_NODE);
    }
    double getOptionalDoubleValue(const std::string &label, const double default_value) const {
        return getOptionalValue<double, DoubleNode>(label, default_value, DOUBLE_NODE);
    }
    int64_t getOptionalIntegerValue(const std::string &label, const int64_t default_value) const {
        return getOptionalValue<int64_t, IntegerNode>(label, default_value, INT64_NODE);
    }
    std::string getOptionalStringValue(const std::string &label, const std::string &default_value = "") const;

    const_iterator begin() const { return entries_.cbegin(); }
    const_iterator end() const { return entries_.cend(); }
};


class ArrayNode final : public JSONNode {
    std::vector<std::shared_ptr<JSONNode>> values_;
    template <typename NodeType>
    std::shared_ptr<NodeType> getNode(const size_t index, const JSONNode::Type node_type) const {
        if (unlikely(index >= values_.size()))
            LOG_ERROR("index " + std::to_string(index) + " out of range [0," + std::to_string(values_.size()) + ")!");
        if (unlikely(values_[index]->getType() != node_type))
            LOG_ERROR("entry with index \"" + std::to_string(index) + "\" is not a " + JSONNode::TypeToString(node_type) + " node!");
        return (std::static_pointer_cast<NodeType>(values_[index]));
    }
    template <typename NodeType>
    std::shared_ptr<NodeType> getOptionalNode(const size_t index, const JSONNode::Type node_type) const {
        if (unlikely(index >= values_.size()))
            return nullptr;
        if (unlikely(values_[index]->getType() != node_type))
            LOG_ERROR("entry with index \"" + std::to_string(index) + "\" is not a " + JSONNode::TypeToString(node_type) + " node!");
        return (std::static_pointer_cast<NodeType>(values_[index]));
    }

public:
    typedef std::vector<std::shared_ptr<JSONNode>>::const_iterator const_iterator;

public:
    explicit ArrayNode() { }

    virtual std::shared_ptr<JSONNode> clone() const override;
    virtual Type getType() const override { return ARRAY_NODE; }
    virtual std::string toString() const override;
    bool empty() const { return values_.empty(); }
    std::shared_ptr<const JSONNode> getNode(const size_t index) const;
    std::shared_ptr<JSONNode> getNode(const size_t index);

    // Automatic cast value retrieval.  If the requested type is not applicable, the functions abort.
    bool getBooleanValue(const size_t index) const { return this->getNode<BooleanNode>(index, BOOLEAN_NODE)->getValue(); }
    std::string getStringValue(const size_t index) const { return this->getNode<StringNode>(index, STRING_NODE)->getValue(); }
    int64_t getIntegerValue(const size_t index) const { return this->getNode<IntegerNode>(index, INT64_NODE)->getValue(); }
    double getDoubleValue(const size_t index) const { return this->getNode<DoubleNode>(index, DOUBLE_NODE)->getValue(); }
    std::shared_ptr<const ObjectNode> getObjectNode(const size_t index) const { return this->getNode<ObjectNode>(index, OBJECT_NODE); }
    std::shared_ptr<ObjectNode> getObjectNode(const size_t index) { return this->getNode<ObjectNode>(index, OBJECT_NODE); };
    std::shared_ptr<const StringNode> getStringNode(const size_t index) const { return this->getNode<StringNode>(index, STRING_NODE); }
    std::shared_ptr<StringNode> getStringNode(const size_t index) { return this->getNode<StringNode>(index, STRING_NODE); }
    std::shared_ptr<const ArrayNode> getArrayNode(const size_t index) const { return this->getNode<ArrayNode>(index, ARRAY_NODE); }
    std::shared_ptr<ArrayNode> getArrayNode(const size_t index) { return this->getNode<ArrayNode>(index, ARRAY_NODE); }
    bool isNullNode(const size_t index) const;

    // Automatic cast value retrieval.  Returns nullptr if node not found.  If the requested type is not applicable, the functions abort.
    std::shared_ptr<const ObjectNode> getOptionalObjectNode(const size_t index) const {
        return this->getOptionalNode<ObjectNode>(index, OBJECT_NODE);
    }
    std::shared_ptr<ObjectNode> getOptionalObjectNode(const size_t index) { return this->getOptionalNode<ObjectNode>(index, OBJECT_NODE); };
    std::shared_ptr<const StringNode> getOptionalStringNode(const size_t index) const {
        return this->getOptionalNode<StringNode>(index, STRING_NODE);
    }
    std::shared_ptr<StringNode> getOptionalStringNode(const size_t index) { return this->getOptionalNode<StringNode>(index, STRING_NODE); }
    std::shared_ptr<const ArrayNode> getOptionalArrayNode(const size_t index) const {
        return this->getOptionalNode<ArrayNode>(index, ARRAY_NODE);
    }
    std::shared_ptr<ArrayNode> getOptionalArrayNode(const size_t index) { return this->getOptionalNode<ArrayNode>(index, ARRAY_NODE); }

    size_t size() const { return values_.size(); }
    const_iterator begin() const { return values_.cbegin(); }
    const_iterator end() const { return values_.cend(); }
    void push_back(std::shared_ptr<JSONNode> const node) { values_.push_back(node); }
};


class Parser {
    Scanner scanner_;
    std::string error_message_;

public:
    explicit Parser(const std::string &json_document): scanner_(json_document) { }

    // Typical use case:
    //
    // std::shared_ptr<JSONNode> tree_root;
    // if (not (parser.parse(&tree_root)))
    //     LOG_ERROR(...);
    //  ...
    bool parse(std::shared_ptr<JSONNode> * const tree_root);

    const std::string &getErrorMessage() const { return error_message_; }

private:
    bool parseObject(std::shared_ptr<JSONNode> * const new_object_node);
    bool parseArray(std::shared_ptr<JSONNode> * const new_array_node);
    bool parseAny(std::shared_ptr<JSONNode> * const new_node);
};


std::string TokenTypeToString(const TokenType token);


/** \brief Locates a JSON node from in JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.
 *  \param tree           The root of a JSON tree structure.
 *  \return The referenced node if found or NULL o/w.
 */
std::shared_ptr<const JSONNode> LookupNode(const std::string &path, const std::shared_ptr<const JSONNode> &tree);


/** \brief Extracts a string datum from a JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.
 *  \param tree           The root of a JSON tree structure.
 *  \return The string value of the referenced node.
 *  \throws std::runtime_error if the datum is not found.
 *  \note Should "path" reference a scalar node that is not a string, a string representation thereof will be
 *        returned.
 */
std::string LookupString(const std::string &path, const std::shared_ptr<const JSONNode> &tree);


/** \brief Extracts a string datum from a JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.
 *  \param tree           The root of a JSON tree structure.
 *  \param default_value  If "path" can't be found, return this.
 *  \return The datum, if found, "default_value" if not found and "default_value" is not NULL.
 *  \note Should "path" reference a scalar node that is not a string, a string representation thereof will be
 *        returned.
 */
std::string LookupString(const std::string &path, const std::shared_ptr<const JSONNode> &tree, const std::string &default_value);


/** \brief Extracts a list of strings from a JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.  Array path components can be specified w/ an asterisk.
 *  \param tree           The root of a JSON tree structure.
 *  \return The list of extracted values.
 *  \note Should "path" reference a scalar nodes that are not strings, a string representations thereof will be
 *        returned.
 */
std::vector<std::string> LookupStrings(const std::string &path, const std::shared_ptr<const JSONNode> &tree);


/** \brief Extracts an integer datum from a JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.
 *  \param tree           The root of a JSON tree structure.
 *  \param default_value  If "path" can't be found, return this.
 *  \return The datum, if found, "default_value" if not found and "default_value" is not NULL.
 *  \throws std::runtime_error if path refers to an existing non-integer node.
 */
int64_t LookupInteger(const std::string &path, const std::shared_ptr<const JSONNode> &tree, const int64_t default_value);


/** \brief Extracts an integer datum from a JSON tree structure.
 *  \param path           A path of the form /X/Y/X...  Individual path components may contain slashes if they are
 *                        backslash escaped.  Literal backslashes also have to be escaped.  No other escapes are
 *                        supported.
 *  \param tree           The root of a JSON tree structure.
 *  \return The datum, if found, "default_value" if not found and "default_value" is not NULL.
 *  \throws std::runtime_error if "path" refers to an existing non-integer node or the node "path" refers to does not exist.
 */
int64_t LookupInteger(const std::string &path, const std::shared_ptr<const JSONNode> &tree);


// Escapes control codes, backslashes, double quotes, form feeds, newlines, carriage returns, and tab characters.
std::string EscapeString(const std::string &unescaped_string);


bool IsValidUTF8(const JSONNode &node);


// Iterates through a JSON node depth-first and invokes a callback on leaf nodes.
// The callback function takes the name of the leaf node and a pointer to the same as its
// first two parameters, and then a list of optional parameters.
template <class... ParamTypes, class CallbackType = void(const std::string &, const std::shared_ptr<JSON::JSONNode> &, ParamTypes...)>
void VisitLeafNodes(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node, const CallbackType callback,
                    ParamTypes... params) {
    switch (node->getType()) {
    case JSON::JSONNode::OBJECT_NODE:
        for (const auto &key_and_node : *static_cast<JSON::ObjectNode *>(node.get()))
            VisitLeafNodes(key_and_node.first, key_and_node.second, callback, params...);
        break;
    case JSON::JSONNode::ARRAY_NODE: {
        for (const auto &element : *static_cast<JSON::ArrayNode *>(node.get())) {
            if (element->getType() != JSON::JSONNode::OBJECT_NODE)
                continue;

            const auto object_node(static_cast<JSON::ObjectNode *>(element.get()));
            for (auto &key_and_node : *object_node)
                VisitLeafNodes(key_and_node.first, key_and_node.second, callback, params...);
        }
        break;
    }
    case JSON::JSONNode::NULL_NODE:
        /* intentionally empty */ break;
    default:
        callback(node_name, node, params...);
    }
}


} // namespace JSON
