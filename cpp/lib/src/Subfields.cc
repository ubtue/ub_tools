/** \file   Subfields.cc
 *  \brief  Implementation of the Subfields class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "Subfields.h"
#include <stdexcept>
#include "Compiler.h"
#include "util.h"


Subfields::Subfields(const std::string &field_data) {
    if (field_data.size() < 3) {
        indicator1_ = indicator2_ = '\0';
        return;
    }

    std::string::const_iterator ch(field_data.begin());
    indicator1_ = *ch++;
    indicator2_ = *ch++;

    while (ch != field_data.end()) {
        if (*ch != '\x1F')
            std::runtime_error(
                "in Subfields::Subfields(const std::string &): expected subfield code delimiter not found! "
                "Found " + std::string(1, *ch) + " in " + field_data + " indicators: "
                + std::string(1, indicator1_) + ", " + std::string(1, indicator2_) + "! " + field_data);

        ++ch;
        if (ch == field_data.end())
            std::runtime_error(
                "in Subfields::Subfields(const std::string &): unexpected subfield data end while expecting "
                "a subfield code! " + field_data);
        const char subfield_code(*ch++);

        std::string subfield_data;
        while (ch != field_data.end() and *ch != '\x1F')
            subfield_data += *ch++;
        if (not subfield_data.empty())
            subfields_.emplace_back(subfield_code, subfield_data);
    }

    std::sort(begin(), end(), SubfieldCodeLessThan());
}


bool Subfields::hasSubfieldWithValue(const char subfield_code, const std::string &value) const {
    auto code_and_value(std::find_if(begin(), end(), CompareSubfieldCode(subfield_code)));
    for (/* empty */; code_and_value != end() && code_and_value->code_ == subfield_code; ++code_and_value) {
        if (code_and_value->value_ == value)
            return true;
    }
    return false;
}


std::string Subfields::getFirstSubfieldValue(const char subfield_code) const {
    const auto begin_end(getIterators(subfield_code));
    if (begin_end.first == begin_end.second)
        return "";
    return begin_end.first->value_;
}


void Subfields::addSubfield(const char subfield_code, const std::string &subfield_data) {
    auto insert(begin());
    for (/* empty */; insert != end() && insert->code_ <= subfield_code; ++insert);
    subfields_.emplace(insert, subfield_code, subfield_data);
}


void Subfields::replace(const char subfield_code, const std::string &old_value, const std::string &new_value) {
    bool found(false);

    const auto begin_end(getIterators(subfield_code));
    for (auto code_and_value(begin_end.first); code_and_value != begin_end.second; ++code_and_value) {
        if (code_and_value->value_ == old_value) {
            found = true;
            code_and_value->value_ = new_value;
        }
    }

    if (not found)
        throw std::runtime_error(
                "Subfields::replace: tried to replace \"" + old_value + "\" with \"" + new_value + "\" in subfield '"
                + subfield_code + "' but did not find the original value!");
}


void Subfields::erase(const char subfield_code) {
    const auto begin_end(getIterators(subfield_code));
    subfields_.erase(begin_end.first, begin_end.second);
}


void Subfields::erase(const char subfield_code, const std::string &value) {
    auto code_and_value(std::find_if(begin(), end(), CompareSubfieldCode(subfield_code)));
    while (code_and_value != end() and code_and_value->code_ == subfield_code) {
        if (code_and_value->value_ == value)
            code_and_value = subfields_.erase(code_and_value);
        else
            ++code_and_value;
    }
}


void Subfields::moveSubfield(const char from_subfield_code, const char to_subfield_code) {
    erase(to_subfield_code);
    std::vector <std::string> values;
    extractSubfields(from_subfield_code, &values);
    for (auto value : values)
        subfields_.emplace_back(to_subfield_code, value);
    erase(from_subfield_code);
    std::sort(begin(), end(), SubfieldCodeLessThan());
}


size_t Subfields::extractSubfields(const std::string &subfield_codes,
                                   std::vector <std::string> *const subfield_values) const {
    for (char subfield_code : subfield_codes) {
        extractSubfields(subfield_code, subfield_values);
    }
    return subfield_values->size();
}

void Subfields::extractSubfields(const char subfield_code, std::vector <std::string> *const subfield_values) const {
    const auto begin_end(getIterators(subfield_code));
    for (auto code_and_value(begin_end.first); code_and_value != begin_end.second; ++code_and_value)
        subfield_values->emplace_back(code_and_value->value_);
}


bool Subfields::replaceSubfieldCode(const char old_code, const char new_code) {
    bool replaced_at_least_one(false);

    for (auto &subfield : subfields_) {
        if (subfield.code_ == old_code) {
            replaced_at_least_one = true;
            subfield.code_ = new_code;
        }
    }

    return replaced_at_least_one;
}


std::string Subfields::toString() const {
    std::string as_string;

    as_string += indicator1_;
    as_string += indicator2_;

    for (const_iterator code_and_value(cbegin()); code_and_value != cend(); ++code_and_value) {
        as_string += '\x1F';
        as_string += code_and_value->code_;
        as_string += code_and_value->value_;
    }

    return as_string;
}
