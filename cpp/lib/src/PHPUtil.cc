/** \file   PHPUtil.cc
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
#include "PHPUtil.h"
#include <cctype>
#include <cstdio>
#include "Compiler.h"
#include "StringUtil.h"


namespace PHPUtil {


static std::string ExtractString(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(*ch != '"'))
        throw ParseException("in PHPUtil::ExtractString: leading double quote not found!");
    ++ch;

    std::string retval;
    while (ch != end and *ch != '"') {
        if (*ch == '\\') {
            retval += *ch++;
            if (unlikely(ch == end))
                throw ParseException("in PHPUtil::ExtractString: unexpected end of input after backslash escape!");
        }
        retval += *ch++;
    }
    if (unlikely(ch == end))
        ParseException("in PHPUtil::ExtractString: unexpected end of input while looking "
                       "for the trailing double quote!");
    ++ch;

    return retval;
}


static void ScanOverColon(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ScanOverColon: unexpected end of input while looking for a colon!");
    if (unlikely(*ch != ':'))
        throw ParseException("in PHPUtil::ScanOverColon: expected a colon but found a different character!");
    ++ch;
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ScanOverColon: unexpected end of input after a colon!");
}


static unsigned ExtractUnsigned(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ExtractUnsigned: unexpected end of input while looking for "
                             "an unsigned number!");

    // We need at least one digit!
    if (unlikely(not isdigit(*ch)))
        throw ParseException("in PHPUtil::ExtractUnsigned: found '" + std::string(1, *ch)
                             + "' while looking for a digit!");

    unsigned retval(0);
    while (ch != end and isdigit(*ch)) {
        retval = retval * 10 + (*ch - '0');
        ++ch;
    }

    return retval;
}


static long ExtractLong(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ExtractLong: unexpected end of input while looking for "
                             "a number!");

    // Deal w/ an optional sign:
    bool negative(false);
    if (*ch == '-') {
        negative = true;
        ++ch;
    }

    const long magnitude(static_cast<long>(ExtractUnsigned(ch, end)));
    return negative ? -magnitude : magnitude;
}


static double ExtractFloat(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ExtractLong: unexpected end of input while looking for "
                             "a floating point number!");

    std::string buffer;
    if (*ch == '-')
        buffer += '-';
    while (ch != end and (isdigit(*ch) or *ch == '.'))
        buffer += *ch++;

    double retval;
    if (std::sscanf(buffer.c_str(), "%lf", &retval) != 1)
        throw ParseException("in PHPUtil::ExtractLong: can't convert \"" + buffer
                             + "\" to a floating point number!");

    return retval;
}


static std::string Unescape(const std::string &s) {
    std::string unescaped_s;

    bool escaped(false);
    for (const char ch : s) {
        if (escaped) {
            escaped = false;
            unescaped_s += ch;
        } else if (ch == '\\')
            escaped = true;
        else
            unescaped_s += ch;
    }

    return unescaped_s;
}


std::string ExtractFieldName(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ExtractFieldName: unexpected end of input while looking for "
                             "the leading 's'!");
    if (unlikely(*ch != 's'))
        throw ParseException("in PHPUtil::ExtractFieldName: did not find a leading 's'!");
    ++ch;

    ScanOverColon(ch, end);
    const unsigned string_length(ExtractUnsigned(ch, end));

    ScanOverColon(ch, end);
    const std::string string_value(ExtractString(ch, end));
    if (unlikely(string_value.length() != string_length))
        throw ParseException("in PHPUtil::ExtractFieldName: length of extracted string does not "
                             "match the preceeding string length!");

    if (unlikely(ch == end or *ch != ';'))
        throw ParseException("in PHPUtil::ExtractFieldName: did not find expected semicolon!");

    return Unescape(string_value);
}


String *ExtractStringField(std::string::const_iterator &ch, const std::string::const_iterator &end,
                           const std::string &field_name)
{
    if (unlikely(ch == end or *ch != 's'))
        throw ParseException("in PHPUtil::ExtractStringField: did not find an 's'!");
    ++ch;
    ScanOverColon(ch, end);
    const unsigned string_length(ExtractUnsigned(ch, end));

    ScanOverColon(ch, end);
    const std::string string_value(ExtractString(ch, end));
    if (unlikely(string_value.length() != string_length))
        throw ParseException("in PHPUtil::ExtractStringField: length of extracted string does not "
                             "match the preceeding string length!");

    if (unlikely(ch == end or *ch != ';'))
        throw ParseException("in PHPUtil::ExtractStringField: did not find expected semicolon!");
    ++ch;

    return new String(field_name, Unescape(string_value));
}


Integer *ExtractIntegerField(std::string::const_iterator &ch, const std::string::const_iterator &end,
                             const std::string &field_name)
{
    if (unlikely(ch == end or *ch != 'i'))
        throw ParseException("in PHPUtil::ExtractIntegerField: did not find an 'i'!");
    ++ch;
    ScanOverColon(ch, end);
    const long integer_value(ExtractLong(ch, end));

    if (unlikely(ch == end or *ch != ';'))
        throw ParseException("in PHPUtil::ExtractIntegerField: did not find expected semicolon!");
    ++ch;

    return new Integer(field_name, integer_value);
}


Float *ExtractFloatField(std::string::const_iterator &ch, const std::string::const_iterator &end,
                         const std::string &field_name)
{
    if (unlikely(ch == end or *ch != 'd'))
        throw ParseException("in PHPUtil::ExtractFloatField: did not find a 'd'!");
    ++ch;
    ScanOverColon(ch, end);
    const double float_value(ExtractFloat(ch, end));

    if (unlikely(ch == end or *ch != ';'))
        throw ParseException("in PHPUtil::ExtractFloatField: did not find expected semicolon!");
    ++ch;

    return new Float(field_name, float_value);
}


Array *ExtractArrayField(std::string::const_iterator &ch, const std::string::const_iterator &end,
                         const std::string &field_name, unsigned level = 0)
{
    if (unlikely(ch == end or *ch != 'a'))
        throw ParseException("in PHPUtil::ExtractArrayField: did not find an 'a'!");
    ++ch;
    ScanOverColon(ch, end);
    const unsigned array_size(ExtractUnsigned(ch, end));
    ScanOverColon(ch, end);
    if (unlikely(ch == end or *ch != '{'))
        throw ParseException("in PHPUtil::ExtractArrayField: did not find expected opening brace!");
    ++ch;

    Array * const new_array(new Array(field_name));
    for (unsigned i(0); i < array_size; ++i) {
        if (unlikely(ch == end))
            throw ParseException("in PHPUtil::ExtractArrayField: unexpected end of input while looking for "
                                 "the type of an entry's key!");

        // Keys of array entries can be either numbers or strings:
        std::string key_name;
        if (*ch == 'i') {
            ++ch;
            ScanOverColon(ch, end);
            const long integer_index(ExtractLong(ch, end));
            key_name = StringUtil::ToString(integer_index);
        } else
            key_name = ExtractFieldName(ch, end);

        if (unlikely(ch == end or *ch != ';'))
            throw ParseException("in PHPUtil::ExtractArrayField: did not find expected semicolon after "
                                 "an array index value!");
        ++ch;

        if (unlikely(ch == end))
            throw ParseException("in PHPUtil::ExtractArrayField: unexpected end of input while looking for "
                                 "the data type of an entry!");
        const char field_type(*ch);
        switch (field_type) {
        case 's': {
            String * const new_string(ExtractStringField(ch, end, "*array_entry*"));
            new_array->addEntry(key_name, new_string);
            break;
        }
        case 'i': {
            Integer * const new_integer(ExtractIntegerField(ch, end, "*array_entry*"));
            new_array->addEntry(key_name, new_integer);
            break;
        }
        case 'd': {
            Float * const new_float(ExtractFloatField(ch, end, "*array_entry*"));
            new_array->addEntry(key_name, new_float);
            break;
        }
        case 'a': {
            Array * const new_array2(ExtractArrayField(ch, end, "*array_entry*", level + 1));
            new_array->addEntry(key_name, new_array2);
            break;
        }
        case 'O': {
            throw ParseException("in PHPUtil::ExtractArrayField: now is the time to implement support for the "
                                 "expect_field_name argument of this function!!");
            //Object * const new_object2(ExtractObjectField(ch, end, "*array_entry*", /* expect_field_name = */true));
            //new_array->addEntry(key_name, new_object2);
            break;
        }
        default:
            throw ParseException("in PHPUtil::ExtractArrayField: unknown or unsupported field type '"
                                 + std::string(1, field_type) + "'!");
        }
    }

    if (unlikely(ch == end or *ch != '}'))
        throw ParseException("in PHPUtil::ExtractArrayField: did not find expected closing brace!");
    ++ch;

    if (unlikely(array_size != new_array->size()))
        throw ParseException("in PHPUtil::ExtractArrayField: array does not have the expected size!"
                             " (expected: " + std::to_string(array_size) + ", actual: "
                             + std::to_string(new_array->size()) + ")");

    return new_array;
}


Object *ParseObject(std::string::const_iterator &ch, const std::string::const_iterator &end,
                    const bool /*expect_field_name*/)
{
    if (unlikely(ch == end))
        throw ParseException("in PHPUtil::ParseObject: unexpected end of input!");
    if (unlikely(*ch != 'O'))
        throw ParseException("in PHPUtil::ParseObject: expected 'O' but did not find it!");
    ++ch;

    ScanOverColon(ch, end);
    const unsigned class_name_length(ExtractUnsigned(ch, end));

    ScanOverColon(ch, end);
    const std::string class_name(ExtractString(ch, end));
    if (unlikely(class_name.empty()))
        throw ParseException("in PHPUtil::ParseObject: found an empty class name!");
    if (unlikely(class_name.length() != class_name_length))
        throw ParseException("in PHPUtil::ParseObject: length of class name does not equal the "
                             "preceeding length!");

    ScanOverColon(ch, end);
    const unsigned field_count(ExtractUnsigned(ch, end));
    ScanOverColon(ch, end);
    if (ch == end or *ch != '{')
        throw ParseException("in PHPUtil::ParseObject: unexpected end of input "
                             "while looking for an opening brace!");
    ++ch;

    Object *new_object(new Object("", Unescape(class_name)));

    for (unsigned field_no(0); field_no < field_count; ++field_no) {
        const std::string field_name(ExtractFieldName(ch, end));
        if (unlikely(ch == end))
            throw ParseException("in PHPUtil::ParseObject: unexpected end of input while looking for "
                                 "the data type of a field!");

        if (unlikely(ch == end or *ch != ';'))
            throw ParseException("in PHPUtil::ParseObject: missing semicolon at end of field name section!");
        ++ch;

        const char field_type(*ch);
        switch (field_type) {
        case 's': {
            String * const new_string(ExtractStringField(ch, end, field_name));
            new_object->addEntry(field_name, new_string);
            break;
        }
        case 'i': {
            Integer * const new_integer(ExtractIntegerField(ch, end, field_name));
            new_object->addEntry(field_name, new_integer);
            break;
        }
        case 'd': {
            Float * const new_float(ExtractFloatField(ch, end, field_name));
            new_object->addEntry(field_name, new_float);
            break;
        }
        case 'a': {
            Array * const new_array(ExtractArrayField(ch, end, field_name));
            new_object->addEntry(field_name, new_array);
            break;
        }
        case 'O': {
            throw ParseException("in PHPUtil::ParseObject: now is the time to implement support for the "
                                 "expect_field_name argument of this function!!");
            //Object * const new_object2(ExtractObjectField(ch, end, field_name, /* expect_field_name = */true));
            //new_object->addEntry(field_name, new_object2);
            break;
        }
        default:
            throw ParseException("in PHPUtil::ParseObject: unknown or unsupported field type '"
                                 + std::string(1, field_type) + "'!");
        }
    }

    return new_object;
}


std::shared_ptr<DataType> DeserialisePHPObject(const std::string &serialised_object) {
    std::string::const_iterator ch(serialised_object.begin());
    const std::string::const_iterator end(serialised_object.end());
    return std::shared_ptr<DataType>(ParseObject(ch, end, /* expect_field_name = */ false));
}


} // namespace PHPUtil
