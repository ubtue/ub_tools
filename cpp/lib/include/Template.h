/** \file    Template.h
 *  \brief   Declarations of template-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2016-2018 Universitätsbibliothek Tübingen.
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
#pragma once


#include <istream>
#include <unordered_map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>


namespace Template {


class Value {
    std::string name_;
public:
    explicit Value(const std::string &name): name_(name) { }
    virtual ~Value() { }
    inline const std::string &getName() const { return name_; }
    virtual size_t size() const = 0;
};


class ScalarValue final : public Value {
    std::string value_;
public:
    ScalarValue(const std::string &name, const std::string &value): Value(name), value_(value) { }
    virtual ~ScalarValue() { }
    virtual size_t size() const final { return 1; }
    inline const std::string &getValue() const { return value_; }
    static std::shared_ptr<Value> Factory(const std::string &name, const std::string &value)
        { return std::shared_ptr<Value>(new ScalarValue(name, value)); }
};


class ArrayValue final : public Value {
    std::vector<std::shared_ptr<Value>> values_;
public:
    ArrayValue(const std::string &name, const std::vector<std::shared_ptr<Value>> &values)
        : Value(name), values_(values) { }
    explicit ArrayValue(const std::string &name): Value(name) { }
    ArrayValue(const std::string &name, const std::vector<std::string> &values);
    virtual ~ArrayValue() { }
    virtual inline size_t size() const final { return values_.size(); }
    void appendValue(const std::shared_ptr<Value> &new_value) { values_.emplace_back(new_value); }
    void appendValue(const std::string &new_value) {
        values_.emplace_back(std::shared_ptr<Value>(new ScalarValue(getName() + "[" + std::to_string(values_.size()) + "]",
                                                                    new_value)));
    }
    const std::shared_ptr<Value> &operator[](const size_t index) const;

    // \return NULL if index is out of range, else the value at the index.
    const Value *getValueAt(const size_t index) const;

    static std::shared_ptr<Value> Factory(const std::string &name, const std::vector<std::shared_ptr<Value>> &values)
        { return std::shared_ptr<Value>(new ArrayValue(name, values)); }
    static std::shared_ptr<Value> Factory(const std::string &name, const std::vector<std::string> &values)
        { return std::shared_ptr<Value>(new ArrayValue(name, values)); }
};


class Function {
public:
    class ArgDesc {
    private:
        std::string description_;
    public:
        ArgDesc(const std::string &description): description_(description) { }
        inline const std::string &getDescription() const { return description_; }
    };
protected:
    std::string name_;
    std::vector<ArgDesc> argument_descriptors_;
public:
    Function(const std::string &name, const std::vector<ArgDesc> &argument_descriptors)
        : name_(name), argument_descriptors_(argument_descriptors) { }
    inline const std::string &getName() const { return name_; }
    virtual std::string call(const std::vector<const Value *> &arguments) const = 0;
};


class Map {
    std::unordered_map<std::string, std::shared_ptr<Value>> map_;
public:
    typedef std::unordered_map<std::string, std::shared_ptr<Value>>::const_iterator const_iterator;
public:
    template<typename T> void insertScalar(const std::string &name, T t) = delete;
    inline void insertScalar(const std::string &name, const std::string &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, value))); }
    inline void insertScalar(const std::string &name, const char * const value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, value))); }
    inline void insertScalar(const std::string &name, const char &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, std::string(1, value)))); }
    inline void insertScalar(const std::string &name, const bool &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, value ? "true" : "false"))); }
    inline void insertScalar(const std::string &name, const unsigned &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, std::to_string(value)))); }
    inline void insertScalar(const std::string &name, const int &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, std::to_string(value)))); }
    inline void insertScalar(const std::string &name, const float &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, std::to_string(value)))); }
    inline void insertScalar(const std::string &name, const double &value)
        { map_.emplace(name, std::shared_ptr<Value>(new ScalarValue(name, std::to_string(value)))); }
    inline void insertArray(const std::string &name, const std::vector<std::string> &values)
        { map_.emplace(name, std::shared_ptr<Value>(new ArrayValue(name, values))); }
    inline void insertArray(const std::string &name, const std::vector<std::shared_ptr<Value>> &values)
        { map_.emplace(name, std::shared_ptr<Value>(new ArrayValue(name, values))); }
    inline const_iterator begin() const { return map_.begin(); }
    inline const_iterator end() const { return map_.end(); }
    inline std::shared_ptr<Value> operator[](const std::string &key) { return map_[key]; }
    inline const_iterator find(const std::string &key) const { return map_.find(key); }
    inline void clear() { map_.clear(); }
    inline bool empty() const { return map_.empty(); }
};


/** A simple template expander.  All special constructs are in curly brackets.  To emit normal curly brackets
 *  you must duplicate them.  Variable names as defined by "names_to_values_map" must start with a lowercase ASCII
 *  letter, followed by lowercase ASCII letters, underscores or ASCII digits.  All keywords are all uppercase.
 *  The list of keywords is IF, ELSE, ENDIF, DEFINED, LOOP and ENDLOOP.  The conditionals for an IF are either
 *  DEFINED(var), var == "value" var1 == var2, var != "value" and var1 != var2.  The DEFINED(var) returns true if
 *  "var" is a key in "names_to_values_map", else false.  Two conditions may be combined with the keywords AND or OR
 *  String constants must start and end with a double quote.  Three backslash escapes are supported, "\\" for a
 *  literal backslash, "\n" for a newline and "\"" for an embedded double quote.  Output gets suppressed if a
 *  condition evaluates to false.  ELSE is optional.  Loops look like "LOOP var1[,var2..]" if more than one variable
 *  name has been specified, all variables must have the same cardinality.  In a loop "var1" etc. are automatically
 *  indexed based on the current iteration.
 *
 *  Predefined functions are Length, UrlEncode, RegexMatch, and Hostname which all return strings.  Length and
 *  UrlEncode take one argument each, Hostname takes no arguments and RegexMatch takes two, the first of which is
 *  a PCRE and the second of which is the string to match against.  It returns the matched part, which implies that
 *  an empty string will be returned if there was no match.
 *
 *  \throws std::runtime_error if anything goes wrong, i.e. if a syntax error has been detected.
 */
void ExpandTemplate(std::istream &input, std::ostream &output, const Map &names_to_values_map,
                    const std::vector<Function *> &functions = {});
std::string ExpandTemplate(const std::string &template_string, const Map &names_to_values_map,
                           const std::vector<Function *> &functions = {});


} // namespace MiscUtil
