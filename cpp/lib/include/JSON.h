/** \file   JSON.h
 *  \brief  A simple JSON parser and support classes.
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
#ifndef JSON_H
#define JSON_H


#include <map>
#include <ostream>
#include <string>
#include <vector>


namespace JSON {


class Node {
public:
    enum Type { SCALAR, OBJECT, ARRAY };
private:
    std::string name_;
protected:
    Node(const Node &rhs): name_(rhs.name_) { }
    explicit Node(const std::string &name): name_(name) { }
public:
    virtual ~Node() { }

    const std::string &getName() const { return name_; }
    virtual Node *clone() const = 0;
    virtual Type getType() const = 0;
    virtual void print(std::ostream &output, const unsigned indent) const = 0;
};


class ScalarNode: public Node {
public:
    enum ScalarType { STRING, FLOAT };
private:
    ScalarType scalar_type_;
    std::string string_value_;
    double float_value_;
public:
    explicit ScalarNode(const ScalarNode &rhs)
        : Node(rhs), scalar_type_(rhs.scalar_type_), string_value_(rhs.string_value_),
          float_value_(rhs.float_value_) { }

    ScalarNode(const std::string &label, const std::string &string_value)
        : Node(label), scalar_type_(STRING), string_value_(string_value) { }
    ScalarNode(const std::string &label, const double float_value)
        : Node(label), scalar_type_(FLOAT), float_value_(float_value) { }
    virtual ~ScalarNode() { }

    ScalarType getScalarType() const { return scalar_type_; }
    const std::string &getStringValue() const { return string_value_; }
    double getFloatValue() const { return float_value_; }
    inline virtual ScalarNode *clone() const { return new ScalarNode(*this); }
    inline virtual Node::Type getType() const { return Node::SCALAR; }
    virtual void print(std::ostream &output, const unsigned indent) const override;
};


class Object: public Node {
    std::map<std::string, Node *> fields_;
public:
    Object(): Node("*Object*") { }
    Object(const Object &rhs);
    virtual ~Object();

    inline void addField(const std::string &field_name, const Node &new_node) {
        fields_[field_name] = new_node.clone();
    }

    inline void addField(const std::string &field_name, Node * const new_node) {
        fields_[field_name] = new_node;
    }

    inline virtual Object *clone() const { return new Object(*this); }
    inline virtual Node::Type getType() const { return Node::OBJECT; }
    virtual void print(std::ostream &output, const unsigned indent) const override;
};


class Array: public Node {
    std::vector<Node *> array_;
public:
    Array(): Node("*Array*") { }
    Array(const Array &rhs);
    virtual ~Array() {
        for (auto node_ptr : array_)
            delete node_ptr;
    }

    void addElement(const Node &new_node) { array_.emplace_back(new_node.clone()); }
    void addElement(Node * const new_node) { array_.emplace_back(new_node); }
    size_t size() const { return array_.size(); }
    const Node &operator[](const size_t index) const { return *array_[index]; }
    Node &operator[](const size_t index) { return *array_[index]; }
    inline virtual Array *clone() const { return new Array(*this); }
    inline virtual Node::Type getType() const { return Node::ARRAY; }
    virtual void print(std::ostream &output, const unsigned indent) const override;
};


class Parser {
    enum TokenType { OPEN_BRACE, CLOSE_BRACE, OPEN_BRACKET, CLOSE_BRACKET, COLON, COMMA, FLOAT_CONSTANT,
                     STRING_CONSTANT, END_OF_INPUT, ERROR };

    unsigned line_no_, start_line_no_;
    std::string::const_iterator ch_, end_;
    bool pushed_back_;
    TokenType last_token_type_;
    std::string last_string_value_;
    double last_float_value_;
    std::string last_err_msg_;
public:
    Parser() { }

    /** Example call:
     *<pre>
     *  JSON::Node *tree;
     *  std::string err_msg;
     *  if (not JSON::Parse(json_doc, &tree, &err_msg))
     *      Error("failed to parse document: " + err_msg);
     *  ... // Do something with "tree".
     *  delete tree;
     *</pre>
     */
    bool parse(const std::string &json_document, Node **root, std::string * const err_msg);
private:
    TokenType getToken(std::string * const string_value, double * const float_value, std::string * const err_msg);
    void ungetToken(const Parser::TokenType token_type, const std::string &string_value,
                    const double float_value, const std::string &err_msg);
    void skipWhite();
    bool parseString(std::string * const string_value, std::string * const err_msg);
    bool parseFloat(double * const float_value, std::string * const err_msg);
    bool parseScalar(const std::string &label, Node **root, std::string * const err_msg);
    bool parseObject(Node **root, std::string * const err_msg);
    bool parseArray(Node **root, std::string * const err_msg);
};


} // namespace 


#endif // ifndef JSON_H
