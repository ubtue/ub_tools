/** \file   Subfields.h
 *  \brief  Interface for the Subfields class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef SUBFIELDS_H
#define SUBFIELDS_H


#include <unordered_map>
#include <string>


/** \class Subfields
 *  \brief Encapsulates the subfields of a MARC-21 data field.
 */
class Subfields {
    char indicator1_, indicator2_;
    std::unordered_multimap<char, std::string> subfield_code_to_data_map_;
public:
    typedef std::unordered_multimap<char, std::string>::const_iterator ConstIterator;
    typedef std::unordered_multimap<char, std::string>::iterator Iterator;
public:
    Subfields() {}
    Subfields(const char indicator1, const char indicator2): indicator1_(indicator1), indicator2_(indicator2) {}

    /** \brief Parses a binary MARC-21 field. */
    explicit Subfields(const std::string &field_data);

    bool empty() const { return subfield_code_to_data_map_.empty(); }
    size_t size() const { return subfield_code_to_data_map_.size(); }
    char getIndicator1() const { return indicator1_; }
    void setIndicator1(const char indicator1) { indicator1_ = indicator1; }
    char getIndicator2() const { return indicator2_; }
    void setIndicator2(const char indicator2) { indicator2_ = indicator2; }
    bool hasSubfield(const char subfield_code) const
        { return subfield_code_to_data_map_.find(subfield_code) != subfield_code_to_data_map_.end(); }

    /** \return True, if a subfield with subfield code "subfield_code" and contents "value" exists, else false. */
    bool hasSubfieldWithValue(const char subfield_code, const std::string &value) const;

    /** \return The bounds of the range of entries that have a subfield code of "subfield_code". */
    std::pair<ConstIterator, ConstIterator> getIterators(const char subfield_code) const
        { return subfield_code_to_data_map_.equal_range(subfield_code); }

    /** \return The bounds of the range of entries that have a subfield code of "subfield_code". */
    std::pair<Iterator, Iterator> getIterators(const char subfield_code)
        { return subfield_code_to_data_map_.equal_range(subfield_code); }

    /** \return The content of the first subfield with code "subfield_code" or the empty string if it doesn't exist.
     */
    std::string getFirstSubfieldValue(const char subfield_code) const;

    /** Swaps out all subfields' data whose subfield code is "subfield_code" and whose data value is "old_value". */
    void replace(const char subfield_code, const std::string &old_value, const std::string &new_value);

    void erase(const char subfield_code) { subfield_code_to_data_map_.erase(subfield_code); }
    void erase(const char subfield_code, const std::string &value);
    void addSubfield(const char subfield_code, const std::string &subfield_data)
        { subfield_code_to_data_map_.insert(std::make_pair(subfield_code, subfield_data)); }

    /** Returns true if the two indicators have valid, i.e. non-NUL, data and at least one subfield exists. */
    bool isValid() const
        { return indicator1_ != '\0' and indicator2_ != '\0' and not subfield_code_to_data_map_.empty(); }

    /** Returns a MARC-21 binary blob for all subfields. (No field terminator will be appended!) */
    std::string toString() const;
};


#endif // ifndef SUBFIELDS_H
