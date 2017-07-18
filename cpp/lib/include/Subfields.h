/** \file   Subfields.h
 *  \brief  Interface for the Subfields class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>


// Forward declaration:
class RegexMatcher;


struct Subfield {
    char code_;
    std::string value_;
public:
    Subfield(const char code, const std::string &value) : code_(code), value_(value) {}
};

struct SubfieldCodeLessThan {
    bool operator() (const Subfield &lhs, const Subfield &rhs) const { return lhs.code_ < rhs.code_; }
    bool operator() (const Subfield &lhs, const char &rhs) const { return lhs.code_ < rhs; }
    bool operator() (const char &lhs, const Subfield &rhs) const { return lhs < rhs.code_; }
};

struct CompareSubfieldCode {
    char code_;
public:
    explicit CompareSubfieldCode(const char code) : code_(code) {}
    bool operator() (const Subfield &subfield) const { return subfield.code_ == code_; }
};

/** \class Subfields
 *  \brief Encapsulates the subfields of a MARC-21 data field.
 */
class Subfields {
    char indicator1_, indicator2_;

    std::vector<Subfield> subfields_;
public:
    typedef std::vector<Subfield>::const_iterator const_iterator;
    typedef std::vector<Subfield>::iterator iterator;
public:
    Subfields() : indicator1_('\0'), indicator2_('\0') { }
    Subfields(const char indicator1, const char indicator2)
        : indicator1_(indicator1), indicator2_(indicator2) { }

    /** \brief Parses a binary MARC-21 field. */
    explicit Subfields(const std::string &field_data);

    bool empty() const { return subfields_.empty(); }
    size_t size() const { return subfields_.size(); }
    char getIndicator1() const { return indicator1_; }
    void setIndicator1(const char indicator1) { indicator1_ = indicator1; }
    char getIndicator2() const { return indicator2_; }
    void setIndicator2(const char indicator2) { indicator2_ = indicator2; }
    bool hasSubfield(const char subfield_code) const
        { return std::find_if(begin(), end(), CompareSubfieldCode(subfield_code)) != end(); }

    /** \return True, if a subfield with subfield code "subfield_code" and contents "value" exists, else false. */
    bool hasSubfieldWithValue(const char subfield_code, const std::string &value) const;

    /** \return True, if a subfield with subfield code "subfield_code" matching "regex" exists, else false. */
    bool hasSubfieldWithPattern(const char subfield_code, const RegexMatcher &regex) const;

    /** \return The bounds of the range of entries that have a subfield code of "subfield_code". */
    std::pair<const_iterator, const_iterator> getIterators(const char subfield_code) const
        { return std::equal_range(begin(), end(), subfield_code, SubfieldCodeLessThan()); }

    /** \return The bounds of the range of entries that have a subfield code of "subfield_code". */
    std::pair<iterator, iterator> getIterators(const char subfield_code)
        { return std::equal_range(begin(), end(), subfield_code, SubfieldCodeLessThan()); }

    /** \return The content of the first subfield with code "subfield_code" or the empty string if it doesn't exist.
     */
    std::string getFirstSubfieldValue(const char subfield_code) const;

    /** Swaps out all subfields' data whose subfield code is "subfield_code" and whose data value is "old_value". */
    void replace(const char subfield_code, const std::string &old_value, const std::string &new_value);

    /** erases all subfields with subfield code "subfield_code". **/
    void erase(const char subfield_code);

    /** erases all subfields with subfield code "subfield_code" iff the subfield value is "value". */
    void erase(const char subfield_code, const std::string &value);

    void addSubfield(const char subfield_code, const std::string &subfield_data);

    /** Replaces all subfields with subfield code "subfield_code" with the given subfield data. **/
    void setSubfield(const char subfield_code, const std::string &subfield_data)
        { erase(subfield_code); addSubfield(subfield_code, subfield_data); }

    /** \brief Moves the contents from the subfield with subfield code "from_subfield_code" to the
     *         subfield with the subfield code "to_subfield_code".
     */
    void moveSubfield(const char from_subfield_code, const char to_subfield_code);

    iterator begin() { return subfields_.begin(); }
    iterator end() { return subfields_.end(); }
    const_iterator begin() const { return subfields_.begin(); }
    const_iterator end() const { return subfields_.end(); }
    const_iterator cbegin() const { return subfields_.cbegin(); }
    const_iterator cend() const { return subfields_.cend(); }

    /** \brief Extracts all values from subfields with codes in the "list" of codes in "subfield_codes".
     *  \return The number of fields that were extracted or, equivalently, the size of "subfield_values" after the call
     *          to this function.
     */
    size_t extractSubfields(const std::string &subfield_codes, std::vector<std::string> * const subfield_values) const;
    void extractSubfields(const char subfield_code, std::vector<std::string> * const subfield_values) const;

    /** \brief Replaces all occurrences of "old_code" with "new_code".
     *  \return True if at least one code was replaced and false o/w.
     */
    bool replaceSubfieldCode(const char old_code, const char new_code);

    /** Returns true if the two indicators have valid, i.e. non-NUL, data and at least one subfield exists. */
    bool isValid() const
        { return indicator1_ != '\0' and indicator2_ != '\0' and not subfields_.empty(); }

    /** Returns a MARC-21 binary blob for all subfields. (No field terminator will be appended!) */
    std::string toString() const;
};


#endif // ifndef SUBFIELDS_H
