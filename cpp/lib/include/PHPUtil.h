/** \file   PHPUtil.h
 *  \brief  Utility functions and classes relating to PHP.
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
#ifndef DB_PHP_UTIL_H
#define DB_PHP_UTIL_H


#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>


namespace PHPUtil {


enum class Type { OBJECT, ARRAY, STRING, INTEGER, FLOAT };

        
class DataType {
protected:
    Type type_;
    std::string name_;
protected:
    DataType(const Type type, const std::string &name): type_(type), name_(name) { }
public:
    DataType(DataType &&other): type_(other.type_), name_(std::move(other.name_)) { }
    virtual ~DataType() { }

    Type getType() const { return type_; }
    const std::string &getName() const { return name_; }
};


class Array: public DataType {
    using StringToDataTypePtrMap = std::unordered_map<std::string, std::shared_ptr<DataType>>;
    mutable StringToDataTypePtrMap map_;
public:
    explicit Array(const std::string &name): DataType(Type::ARRAY, name) { }
    Array(Array &&other): DataType(std::move(other)), map_(std::move(other.map_)) { }

    size_t size() const { return map_.size(); }
    const DataType &operator[](const size_t index) const { return *map_[std::to_string(index)]; }
    const DataType &operator[](const std::string &index) const { return *map_[index]; }
    void addEntry(const std::string &key, DataType * const value) {
        map_[key] = std::shared_ptr<DataType>(value);
    }

    // Iterate over entries:
    StringToDataTypePtrMap::const_iterator cbegin() const { return map_.cbegin(); }
    StringToDataTypePtrMap::const_iterator cend() const { return map_.cend(); }
};


class Object: public Array {
    std::string class_;
public:
    Object(const std::string &name, const std::string &cls): Array(name), class_(cls) { type_ = Type::OBJECT; }
    Object(Object &&other): Array(std::move(other)), class_(std::move(other.class_)) { }

    const std::string &getClass() const { return class_; }
};


class String: public DataType {
    std::string value_;
public:
    String(const std::string &name, const std::string &value): DataType(Type::STRING, name), value_(value) { }
    String(String &&other): DataType(std::move(other)), value_(std::move(other.value_)) { }

    const std::string &getValue() const { return value_; }
};


class Integer: public DataType {
    long value_;
public:
    Integer(const std::string &name, const long value): DataType(Type::INTEGER, name), value_(value) { }
    Integer(Integer &&other): DataType(std::move(other)), value_(other.value_) { }

    long getValue() const { return value_; }
};


class Float: public DataType {
    double value_;
public:
    explicit Float(const std::string &name, const double value): DataType(Type::FLOAT, name), value_(value) { }
    Float(Float &&other): DataType(std::move(other)), value_(other.value_) { }

    double getValue() const { return value_; }
};
    

class ParseException: public std::runtime_error {
public:
    explicit ParseException(const std::string &err_msg): std::runtime_error(err_msg) { }
};
    

/** \brief Parses a serialised PHP object.
 *  \param serialised_object  Hopefully the serialised version of a PHP object.
 *  \return A pointer to the deserialised object.
 *  \throws ParseException when a parse error occurred.
 */
std::shared_ptr<DataType> DeserialisePHPObject(const std::string &serialised_object);


} // namespace PHPUtil


#endif // ifndef DB_PHP_UTIL_H
