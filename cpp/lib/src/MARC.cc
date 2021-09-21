/** \brief Various classes, functions etc. having to do with the Library of Congress MARC bibliographic format.
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
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "MARC.h"
#include <set>
#include <unordered_map>
#include <cerrno>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "BSZUtil.h"
#include "FileLocker.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"
#include <iostream>


namespace {


inline unsigned ToUnsigned(const char *cp, const unsigned count) {
    unsigned retval(0);
    for (unsigned i(0); i < count; ++i)
        retval = retval * 10 + (*cp++ - '0');

    return retval;
}


inline unsigned NoOfDigits(const unsigned n) {
    if (n < 10)
        return 1;
    if (n < 100)
        return 2;
    if (n < 1000)
        return 3;
    if (n < 10000)
        return 4;
    if (n < 100000)
        return 5;
    throw std::out_of_range("in NoOfDigits: n == " + std::to_string(n));
}


inline std::string &AppendToStringWithLeadingZeros(std::string &target, unsigned n, const unsigned width) {
    const unsigned no_of_digits(NoOfDigits(n));

    for (unsigned i(no_of_digits); i < width; ++i)
        target += '0';

    static const unsigned powers_of_ten[] = { 1, 10, 100, 1000, 10000, 100000 };

    unsigned divisor(powers_of_ten[no_of_digits - 1]);
    for (unsigned i(0); i < no_of_digits; ++i) {
        target += '0' + (n / divisor);
        n %= divisor;
        divisor /= 10u;
    }

    return target;
}


} // unnamed namespace


namespace MARC {


static inline bool IsNineOrNonDigit(const char ch) {
    return not StringUtil::IsDigit(ch) or ch == '9';
}


bool Tag::isLocal() const {
    return IsNineOrNonDigit(tag_.as_cstring_[0]) or IsNineOrNonDigit(tag_.as_cstring_[1]) or IsNineOrNonDigit(tag_.as_cstring_[2]);
}


Tag &Tag::swap(Tag &other) {
    std::swap(tag_.as_int_, other.tag_.as_int_);
    return *this;
}


Subfields::Subfields(const std::string &field_contents) {
    if (unlikely(field_contents.length() < 5)) // We need more than: 2 indicators + delimiter + subfield code
        return;

    std::string value;
    char subfield_code(field_contents[3]);
    for (auto ch(field_contents.cbegin() + 4 /* 2 indicators + delimiter + subfield code */);
         ch != field_contents.cend(); ++ch)
    {
        if (unlikely(*ch == '\x1F')) {
            subfields_.emplace_back(subfield_code, value);
            value.clear();
            ++ch; // Skip over the delimiter.
            subfield_code = *ch;
        } else
            value += *ch;
    }

    subfields_.emplace_back(subfield_code, value);
}


void Subfields::addSubfield(const char subfield_code, const std::string &subfield_value) {
    auto insertion_location(subfields_.begin());
    while (insertion_location != subfields_.end() and insertion_location->code_ < subfield_code)
        ++insertion_location;
    subfields_.emplace(insertion_location, subfield_code, subfield_value);
}


bool Subfields::replaceFirstSubfield(const char subfield_code, const std::string &new_subfield_value) {
    auto replacement_location(subfields_.begin());
    while (replacement_location != subfields_.end() and replacement_location->code_ != subfield_code)
        ++replacement_location;
    if (replacement_location == subfields_.end())
        return false;
    replacement_location->value_ = new_subfield_value;
    return true;
}


bool Subfields::replaceAllSubfields(const char subfield_code, const std::string &old_subfield_value,
                                    const std::string &new_subfield_value)
{
    auto replacement_location(subfields_.begin());
    while (replacement_location != subfields_.end() and replacement_location->code_ != subfield_code)
        ++replacement_location;
    if (replacement_location == subfields_.end())
        return false;

    bool replaced_at_least_one_subfield(false);
    while (replacement_location->code_ == subfield_code) {
        if (replacement_location->value_ == old_subfield_value) {
            replacement_location->value_ = new_subfield_value;
            replaced_at_least_one_subfield = true;
        }
        ++replacement_location;
    }

    return replaced_at_least_one_subfield;
}


std::vector<std::string> Subfields::extractSubfieldsAndNumericSubfields(const std::string &subfield_spec) const {
    std::set<std::string> numeric_subfield_specs;
    std::string subfield_codes;

    for (auto code(subfield_spec.cbegin()); code != subfield_spec.cend(); ++code) {
        if (StringUtil::IsDigit(*code)) {
            if (unlikely((code + 1) == subfield_spec.cend()))
                LOG_ERROR("numeric subfield code is missing a following character!");
            numeric_subfield_specs.insert(std::string(1, *code) + std::string(1, *(code + 1)));
            subfield_codes += *code;
            ++code;
        } else
            subfield_codes += *code;
    }

    std::vector<std::string> subfield_values;
    if (subfield_codes.empty())
        return subfield_values;

    for (const auto &subfield : subfields_) {
        if (subfield_codes.find(subfield.code_) != std::string::npos) {
            if (not StringUtil::IsDigit(subfield.code_))
                subfield_values.emplace_back(subfield.value_);
            else {
               if (numeric_subfield_specs.find(std::string(1, subfield.code_) + subfield.value_[0]) != numeric_subfield_specs.cend())
                   if (subfield.value_[1] == ':')
                       subfield_values.emplace_back(subfield.value_.substr(2));
            }
        }
    }
    return subfield_values;
}


bool Record::Field::operator<(const Record::Field &rhs) const {
    if (tag_ < rhs.tag_)
        return true;
    if (tag_ > rhs.tag_)
        return false;
    return contents_ < rhs.contents_;
}



Tag Record::Field::getLocalTag() const {
    if (unlikely(tag_ != "LOK"))
        LOG_ERROR("you must not call getLocalTag() on a \"" + tag_.toString() + "\" tag!");
    if (contents_.length() < 2 /*indicators*/ + 2/*delimiter and subfield code*/ + 3 /*pseudo tag*/
        or contents_[2] != '\x1F' or contents_[3] != '0')
        return "";
    return contents_.substr(2 /*indicators*/ + 2/*delimiter and subfield code*/, 3 /*tag length*/);
}


bool Record::Field::filterSubfields(const std::string &codes_to_keep) {
    std::string new_contents;
    new_contents.reserve(contents_.size());

    if (unlikely(contents_.length() < 2))
        return false;

    auto ch(contents_.begin());
    new_contents += *ch++; // 1st indicator
    new_contents += *ch++; // 2nd indicator

    while (ch != contents_.end()) {
        // The subfield code follows the subfield separtor.
        if (unlikely(*ch != '\x1F'))
            LOG_ERROR("missing subfield separator!");
        ++ch;

        if (unlikely(ch == contents_.end()))
            LOG_ERROR("premature end of field!");

        if (codes_to_keep.find(*ch) == std::string::npos) {
            // Skip subfield:
            while (ch != contents_.end() and *ch != '\x1F')
                ++ch;
        } else {
            new_contents += '\x1F';
            while (ch != contents_.end() and *ch != '\x1F')
                new_contents += *ch++;
        }
    }

    if (new_contents.size() == contents_.size())
        return false;

    contents_.swap(new_contents);
    return true;
}


std::string Record::Field::getFirstSubfieldWithCode(const char subfield_code) const {
    if (unlikely(contents_.length() < 5)) // We need more than: 2 indicators + delimiter + subfield code
        return "";

    const char delimiter_and_code[]{ '\x1F', subfield_code, '\0' };
    const size_t subfield_start_pos(contents_.find(delimiter_and_code, 2 /*skip indicators*/, 2));
    if (subfield_start_pos == std::string::npos)
        return "";

    std::string subfield_value;
    for (auto ch(contents_.cbegin() + subfield_start_pos + 2); ch != contents_.cend(); ++ch) {
        if (*ch == '\x1F')
            break;
        subfield_value += *ch;
    }

    return subfield_value;
}


std::string Record::Field::getFirstSubfieldWithCodeAndPrefix(const char subfield_code, const std::string &prefix) const {
    const auto subfields(getSubfields());
    for (const auto &subfield : subfields) {
        if (subfield.code_ == subfield_code) {
            if (StringUtil::StartsWith(subfield.value_, prefix))
                return subfield.value_;
        }
    }
    return "";
}


bool Record::Field::hasSubfield(const char subfield_code) const {
    bool subfield_delimiter_seen(false);
    for (const char ch : contents_) {
        if (subfield_delimiter_seen) {
            if (ch == subfield_code)
                return true;
            subfield_delimiter_seen = false;
        } else if (ch == '\x1F')
            subfield_delimiter_seen = true;
    }

    return false;
}


bool Record::Field::hasSubfieldWithValue(const char subfield_code, const std::string &value, const bool case_insensitive) const {
    bool subfield_delimiter_seen(false);
    for (auto ch(contents_.cbegin()); ch != contents_.cend(); ++ch) {
        if (subfield_delimiter_seen) {
            if (*ch == subfield_code and (case_insensitive ?
                                          StringUtil::ASCIIToUpper(contents_.substr(ch - contents_.cbegin() + 1, value.length())) == StringUtil::ASCIIToUpper(value) :
                                          contents_.substr(ch - contents_.cbegin() + 1, value.length()) == value)
               )
                return true;
            subfield_delimiter_seen = false;
        } else if (*ch == '\x1F')
            subfield_delimiter_seen = true;
    }

    return false;
}


bool Record::Field::extractSubfieldWithPattern(const char subfield_code, RegexMatcher &regex, std::string * const value) const {
    value->clear();

    bool subfield_delimiter_seen(false), collecting(false);
    for (auto ch : contents_) {
        if (subfield_delimiter_seen) {
            if (not value->empty() and regex.matched(*value))
                return true;
            subfield_delimiter_seen = false;
            value->clear();
            collecting = ch == subfield_code;
        } else if (ch == '\x1F') {
            subfield_delimiter_seen = true;
        } else if (collecting)
            *value += ch;
    }

    if (value->empty())
        return false;
    return regex.matched(*value);
}


bool Record::Field::removeSubfieldWithPattern(const char subfield_code, const ThreadSafeRegexMatcher &regex) {
    auto subfields((Subfields(contents_)));
    subfields.deleteAllSubfieldsWithCodeMatching(subfield_code, regex);
    setContents(subfields, getIndicator1(), getIndicator2());
    return true;
}


void Record::Field::insertOrReplaceSubfield(const char subfield_code, const std::string &subfield_contents) {
    Subfields subfields(contents_);
    if (not subfields.replaceFirstSubfield(subfield_code, subfield_contents))
        subfields.addSubfield(subfield_code, subfield_contents);
    contents_ = contents_.substr(0, 2) /* keep our original indicators */ + subfields.toString();
}


bool Record::Field::replaceSubfieldCode(const char old_code, const char new_code) {
    if (contents_.length() < 5)
        return false;

    bool replaced_at_least_one_code(false), subfield_delimiter_seen(false);
    for (auto &ch : contents_) {
        if (subfield_delimiter_seen) {
            subfield_delimiter_seen = false;
            if (ch == old_code) {
                ch = new_code;
                replaced_at_least_one_code = true;
            }
        } else if (ch == '\x1F')
            subfield_delimiter_seen = true;
    }

    return replaced_at_least_one_code;
}


void Record::Field::deleteAllSubfieldsWithCode(const char subfield_code) {
    if (contents_.size() < 5)
        return;

    std::string new_contents;
    new_contents.reserve(contents_.size());

    new_contents += contents_[0]; // indicator 1
    new_contents += contents_[1]; // indicator 2

    auto ch(contents_.begin() + 2 /* skip over the indicators */ + 1 /* \x1F */);
    while (ch != contents_.end()) {
        if (*ch == subfield_code) {
            while (ch != contents_.end() and *ch != '\x1F')
                ++ch;
        } else {
            new_contents += '\x1F';
            while (ch != contents_.end() and *ch != '\x1F')
                new_contents += *ch++;
        }
        if (ch != contents_.end())
            ++ch;
    }

    contents_.swap(new_contents);
}


Record::KeywordAndSynonyms &Record::KeywordAndSynonyms::swap(KeywordAndSynonyms &other) {
    if (this != &other) {
        tag_.swap(other.tag_);
        keyword_.swap(other.keyword_);
        synonyms_.swap(other.synonyms_);
    }

    return *this;
}


Record::Record(const std::string &leader): record_size_(LEADER_LENGTH + 1 /* end-of-directory */ + 1 /* end-of-record */), leader_(leader) {
    if (unlikely(leader_.length() != LEADER_LENGTH))
        LOG_ERROR("supposed leader has invalid length!");
}


Record::Record(const size_t record_size, const char * const record_start)
    : record_size_(record_size), leader_(record_start, LEADER_LENGTH)
{
    const char * const base_address_of_data(record_start + ToUnsigned(record_start + 12, 5));

    // Process directory:
    const char *directory_entry(record_start + LEADER_LENGTH);
    while (directory_entry != base_address_of_data - 1) {
        if (unlikely(directory_entry > base_address_of_data))
            LOG_ERROR("directory_entry > base_address_of_data!");
        std::string tag;
        tag += directory_entry[0];
        tag += directory_entry[1];
        tag += directory_entry[2];
        const unsigned field_length(ToUnsigned(directory_entry + 3, 4));
        const unsigned field_offset(ToUnsigned(directory_entry + 7, 5));
        const std::string field_contents(base_address_of_data + field_offset, field_length - 1);
        fields_.emplace_back(tag, field_contents);
        directory_entry += 3 /* tag */ + 4 /* field length */ + 5 /* field offset */;
    }
}


static std::string TypeOfRecordToString(const Record::TypeOfRecord type_of_record) {
    switch (type_of_record) {
    case Record::TypeOfRecord::LANGUAGE_MATERIAL:
        return std::string(1, 'a');
    case Record::TypeOfRecord::NOTATED_MUSIC:
        return std::string(1, 'c');
    case Record::TypeOfRecord::MANUSCRIPT_NOTATED_MUSIC:
        return std::string(1, 'd');
    case Record::TypeOfRecord::CARTOGRAPHIC_MATERIAL:
        return std::string(1, 'e');
    case Record::TypeOfRecord::MANUSCRIPT_CARTOGRAPHIC_MATERIAL:
        return std::string(1, 'f');
    case Record::TypeOfRecord::PROJECTED_MEDIUM:
        return std::string(1, 'g');
    case Record::TypeOfRecord::NONMUSICAL_SOUND_RECORDING:
        return std::string(1, 'i');
    case Record::TypeOfRecord::MUSICAL_SOUND_RECORDING:
        return std::string(1, 'j');
    case Record::TypeOfRecord::TWO_DIMENSIONAL_NONPROJECTABLE_GRAPHIC:
        return std::string(1, 'k');
    case Record::TypeOfRecord::COMPUTER_FILE:
        return std::string(1, 'm');
    case Record::TypeOfRecord::KIT:
        return std::string(1, 'o');
    case Record::TypeOfRecord::MIXED_MATERIALS:
        return std::string(1, 'p');
    case Record::TypeOfRecord::THREE_DIMENSIONAL_ARTIFACT_OR_NATURALLY_OCCURRING_OBJECT:
        return std::string(1, 'r');
    case Record::TypeOfRecord::MANUSCRIPT_LANGUAGE_MATERIAL:
        return std::string(1, 't');
    default:
        LOG_ERROR("unknown type-of-record: " + std::to_string(static_cast<int>(type_of_record)) + "!");
    }
}


char Record::BibliographicLevelToChar(const Record::BibliographicLevel bibliographic_level) {
    switch (bibliographic_level) {
    case Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART:
        return 'a';
    case Record::BibliographicLevel::SERIAL_COMPONENT_PART:
        return 'b';
    case Record::BibliographicLevel::COLLECTION:
        return 'c';
    case Record::BibliographicLevel::SUBUNIT:
        return 'd';
    case Record::BibliographicLevel::INTEGRATING_RESOURCE:
        return 'i';
    case Record::BibliographicLevel::MONOGRAPH_OR_ITEM:
        return 'm';
    case Record::BibliographicLevel::SERIAL:
        return 's';
    case Record::BibliographicLevel::UNDEFINED:
        return ' ';
    default:
        LOG_ERROR("unknown bibliographic level: " + std::to_string(static_cast<int>(bibliographic_level)) + "!");
    }
}


std::string Record::RecordTypeToString(const RecordType record_type) {
    switch (record_type) {
    case RecordType::AUTHORITY:
        return "AUTHORITY";
    case RecordType::UNKNOWN:
        return "UNKNOWN";
    case RecordType::BIBLIOGRAPHIC:
        return "BIBLIOGRAPHIC";
    case RecordType::CLASSIFICATION:
        return "CLASSIFICATION";
    }

    LOG_ERROR("Unknown record type " + std::to_string(static_cast<int>(record_type)) + "!");
}


Record::Record(const TypeOfRecord type_of_record, const BibliographicLevel bibliographic_level,
               const std::string &control_number)
{
    leader_ = "00000" "n" + TypeOfRecordToString(type_of_record) + std::string(1, BibliographicLevelToChar(bibliographic_level))
              + " a22004452  4500";

    if (not control_number.empty())
        insertField("001", control_number);
}


std::string Record::toBinaryString() const {
    std::string as_string;

    Record::const_iterator start(begin());
    do {
        const bool record_is_oversized(start > begin());
        Record::const_iterator end(start);
        unsigned record_size(Record::LEADER_LENGTH + 2 /* end-of-directory and end-of-record */);
        if (record_is_oversized) // Include size of the 001 field.
            record_size += fields_.front().getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH;
        while (end != this->end()
               and (record_size + end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH < Record::MAX_RECORD_LENGTH))
        {
            record_size += end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH;
            ++end;
        }

        std::string raw_record;
        raw_record.reserve(record_size);
        const unsigned no_of_fields(end - start + (record_is_oversized ? 1 /* for the added 001 field */ : 0));
        AppendToStringWithLeadingZeros(raw_record, record_size, /* width = */ 5);
        StringUtil::AppendSubstring(raw_record, leader_, 5, 12 - 5);
        const unsigned base_address_of_data(Record::LEADER_LENGTH + no_of_fields * Record::DIRECTORY_ENTRY_LENGTH
                                            + 1 /* end-of-directory */);
        AppendToStringWithLeadingZeros(raw_record, base_address_of_data, /* width = */ 5);
        StringUtil::AppendSubstring(raw_record, leader_, 17, Record::LEADER_LENGTH - 17);

        // Append the directory:
        unsigned field_start_offset(0);
        if (record_is_oversized) {
            raw_record += "001";
            AppendToStringWithLeadingZeros(raw_record, fields_.front().getContents().length() + 1 /* field terminator */, 4);
            AppendToStringWithLeadingZeros(raw_record, field_start_offset, /* width = */ 5);
            field_start_offset += fields_.front().getContents().length() + 1 /* field terminator */;
        }
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            const size_t contents_length(entry->getContents().length());
            if (unlikely(contents_length > Record::MAX_VARIABLE_FIELD_DATA_LENGTH))
                LOG_ERROR("can't generate a directory entry w/ a field w/ data length " + std::to_string(contents_length) +
                          " for PPN \"" + getControlNumber() + "\"!");
            raw_record += entry->getTag().toString();
            AppendToStringWithLeadingZeros(raw_record, entry->getContents().length() + 1 /* field terminator */, 4);
            AppendToStringWithLeadingZeros(raw_record, field_start_offset, /* width = */ 5);
            field_start_offset += contents_length + 1 /* field terminator */;
        }
        raw_record += '\x1E'; // end-of-directory

        // Now append the field data:
        if (record_is_oversized) {
            raw_record += fields_.front().getContents();
            raw_record += '\x1E'; // end-of-field
        }
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getContents();
            raw_record += '\x1E'; // end-of-field
        }
        raw_record += '\x1D'; // end-of-record

        if (as_string.empty())
            as_string.swap(raw_record);
        else
            as_string += raw_record;

        start = end;
    } while (start != end());

    return as_string;
}


void Record::toXmlStringHelper(MarcXmlWriter * const xml_writer) const {
    xml_writer->openTag("record");
    xml_writer->writeTagsWithData("leader", leader_, /* suppress_newline = */ true);
    for (const auto &field : *this) {
        if (field.isControlField())
            xml_writer->writeTagsWithData("controlfield", { std::make_pair("tag", field.getTag().toString()) }, field.getContents(),
                                         /* suppress_newline = */ true);
        else { // We have a data field.
            xml_writer->openTag("datafield",
                                { std::make_pair("tag", field.getTag().toString()),
                                  std::make_pair("ind1", std::string(1, field.getIndicator1())),
                                  std::make_pair("ind2", std::string(1, field.getIndicator2()))
                                });

            const Subfields subfields(field.getSubfields());
            for (const auto &subfield : subfields)
                xml_writer->writeTagsWithData("subfield", { std::make_pair("code", std::string(1, subfield.code_)) },
                                              subfield.value_, /* suppress_newline = */ true);

            xml_writer->closeTag(); // Close "datafield".
        }
    }
    xml_writer->closeTag(); // Close "record".
}


size_t Record::truncate(const const_iterator field_iter) {
    const auto old_field_count(fields_.size());
    fields_.erase(field_iter, fields_.cend());
    return old_field_count - fields_.size();
}


std::string Record::toString(const RecordFormat record_format, const unsigned indent_amount,
                             const MarcXmlWriter::TextConversionType text_conversion_type) const
{
    if (record_format == RecordFormat::MARC21_BINARY)
        return toBinaryString();
    else {
        std::string as_string;
        MarcXmlWriter xml_writer(&as_string, /* suppress_header_and_tailer = */true, indent_amount, text_conversion_type);
        toXmlStringHelper(&xml_writer);
        return as_string;
    }
}


bool Record::isProbablyNewerThan(const Record &other) const {
    const auto this_publication_year(getMostRecentPublicationYear());
    const auto other_publication_year(other.getMostRecentPublicationYear());
    if (this_publication_year.empty() or other_publication_year.empty())
        return getControlNumber() > other.getControlNumber();
    return this_publication_year > other_publication_year;
}


void Record::merge(const Record &other) {
    for (const auto &other_field : other)
        insertField(other_field);
}


bool Record::isMonograph() const {
    for (const auto &_935_field : getTagRange("935")) {
        for (const auto &subfield : _935_field.getSubfields()) {
            if (subfield.code_ == 'c' and subfield.value_ == "so")
                return false;
        }
    }

    return leader_[7] == 'm';
}


bool Record::isArticle() const {
    if (leader_[7] == 'm') {
        for (const auto &_935_field : getTagRange("935")) {
            for (const auto &subfield : _935_field.getSubfields()) {
                if (subfield.code_ == 'c' and subfield.value_ == "so")
                    return true;
            }
        }
        return false;
    }

    return leader_[7] == 'a' or leader_[7] == 'b';
}


static const std::set<std::string> ELECTRONIC_CARRIER_TYPES{ "cb", "cd", "ce", "ca", "cf", "ch", "cr", "ck", "cz" };


bool Record::isWebsite() const {
    if (leader_.length() < 7 or leader_[6] != 'i')
        return false;

    const auto _008_field(findTag("008"));
    if (unlikely(_008_field == end()))
        return false;

    const auto &_008_contents(_008_field->getContents());
    return _008_contents.length() > 21 and _008_contents[21] == 'W';
}


bool Record::isElectronicResource() const {
    if (leader_.length() > 6 and (leader_[6] == 'a' or leader_[6] == 'm')) {
        for (const auto &_007_field : getTagRange("007")) {
            if (*_007_field.getContents().c_str() == 'c')
                return true;
        }
    }

    for (const auto &_935_field : getTagRange("935")) {
        const Subfields subfields(_935_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ != 'c')
                continue;
            if (subfield.value_ == "sodr")
                return true;
        }
    }

    for (const auto &_245_field : getTagRange("245")) {
        const Subfields subfields(_245_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ != 'h')
                continue;
            if (::strcasestr(subfield.value_.c_str(), "[Elektronische Ressource]") != nullptr
                or ::strcasestr(subfield.value_.c_str(), "[electronic resource]") != nullptr)
                return true;
        }
    }

    for (const auto &_300_field : getTagRange("300")) {
        const Subfields subfields(_300_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ != 'a')
                continue;
            if (::strcasestr(subfield.value_.c_str(), "Online-Ressource") != nullptr)
                return true;
        }
    }

    for (const auto &_338_field : getTagRange("338")) {
        const Subfields subfields(_338_field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ != 'b')
                continue;
            if (ELECTRONIC_CARRIER_TYPES.find(subfield.value_) != ELECTRONIC_CARRIER_TYPES.end())
                return true;
        }
    }

    return not getDOIs().empty();

    return false;
}


bool Record::isPrintResource() const {
    if (leader_.length() > 6 and leader_[6] == 'a') {
        for (const auto &_007_field : getTagRange("007")) {
            if (*_007_field.getContents().c_str() == 't')
                return true;
        }
    }

    for (const auto &_935_field : getTagRange("935")) {
        for (const auto &subfield : _935_field.getSubfields()) {
            if (subfield.code_ == 'b' and subfield.value_ == "druck")
                return true;
        }
    }

    return false;
}


enum Record::BibliographicLevel Record::getBibliographicLevel() {
    switch (leader_[7]) {
    case 'a':
        return Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART;
    case 'b':
        return Record::BibliographicLevel::SERIAL_COMPONENT_PART;
    case 'c':
        return Record::BibliographicLevel::COLLECTION;
    case 'd':
        return Record::BibliographicLevel::SUBUNIT;
    case 'i':
        return Record::BibliographicLevel::INTEGRATING_RESOURCE;
    case 'm':
        return Record::BibliographicLevel::MONOGRAPH_OR_ITEM;
    case 's':
        return Record::BibliographicLevel::SERIAL;
    case ' ':
        return Record::BibliographicLevel::UNDEFINED;
    default:
        LOG_ERROR("unknown bibliographic level: " + std::string(1, leader_[0]) + "!");
    }
}


void Record::setBibliographicLevel(const Record::BibliographicLevel new_bibliographic_level) {
    leader_[7] = BibliographicLevelToChar(new_bibliographic_level);
}


inline static bool MatchAny(const Tag &tag, const std::vector<Tag> &tags) {
    return std::find_if(tags.cbegin(), tags.cend(), [&tag](const Tag &tag1){ return tag1 == tag; }) != tags.cend();
}


Record::ConstantRange Record::getTagRange(const std::vector<Tag> &tags) const {
    const auto begin(std::find_if(fields_.begin(), fields_.end(),
                                  [&tags](const Field &field) -> bool { return MatchAny(field.getTag(), tags); }));
    if (begin == fields_.end())
        return ConstantRange(fields_.end(), fields_.end());

    auto end(begin);
    while (end != fields_.end() and MatchAny(end->getTag(), tags))
        ++end;

    return ConstantRange(begin, end);
}


Record::Range Record::getTagRange(const Tag &tag) {
    auto begin(std::find_if(fields_.begin(), fields_.end(),
                            [&tag](const Field &field) -> bool { return field.getTag() == tag; }));
    if (begin == fields_.end())
        return Range(fields_.end(), fields_.end());

    auto end(begin);
    while (end != fields_.end() and end->getTag() == tag)
        ++end;

    return Range(begin, end);
}


size_t Record::reTag(const Tag &from_tag, const Tag &to_tag) {
    size_t changed_count(0);
    for (auto &field : getTagRange(from_tag)) {
        field.setTag(to_tag);
        ++changed_count;
    }

    if (changed_count > 0)
        std::stable_sort(fields_.begin(), fields_.end(), [](const Field &lhs, const Field &rhs){ return lhs.tag_ < rhs.tag_; });

    return changed_count;
}


Record::iterator Record::erase(const Tag &tag, const bool first_occurrence_only) {
    auto iter(findTag(tag));
    while (iter != end() and iter->getTag() == tag) {
        iter = erase(iter);
        if (first_occurrence_only)
            return iter;
    }

    return iter;
}


bool Record::deleteFieldWithSubfieldCodeMatching(const Tag &tag, const char subfield_code, const ThreadSafeRegexMatcher &matcher) {
    bool matched(false);
    fields_.erase(std::remove_if(fields_.begin(), fields_.end(),
                  [&](const Field &field) -> bool
                  {
                      if ((field.getTag() != tag) or not field.hasSubfield(subfield_code))
                          return false;
                      const Subfields subfields(field.getContents());
                      const auto subfield_values(subfields.extractSubfields(std::string(1, subfield_code)));
                      for (const auto &subfield_value : subfield_values) {
                           if (matcher.match(subfield_value)) {
                               matched = true;
                               return true;
                           }
                      }
                      return false;
                  }),
                  fields_.end());
    return matched;
}


bool Record::hasTagWithIndicators(const Tag &tag, const char indicator1, const char indicator2) const {
    for (const auto &field : getTagRange(tag)) {
        if (field.getIndicator1() == indicator1 and field.getIndicator2() == indicator2)
            return true;
    }
    return false;
}

std::vector<std::string> Record::getSubfieldValues(const Tag &tag, const char subfield_code) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        for (const auto &subfield_value : subfields.extractSubfields(subfield_code))
            subfield_values.emplace_back(subfield_value);
    }

    return subfield_values;
}


std::vector<std::string> Record::getSubfieldValues(const Tag &tag, const std::string &subfield_codes) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        for (const auto &subfield_code : subfield_codes)
            for (const auto &subfield_value : subfields.extractSubfields(subfield_code))
                subfield_values.emplace_back(subfield_value);
    }
    return subfield_values;
}


std::vector<std::string> Record::getSubfieldAndNumericSubfieldValues(const Tag &tag, const std::string &subfield_spec) const {
    std::vector<std::string> subfield_values;
    for (const auto &field : getTagRange(tag)) {
        const Subfields subfields(field.getContents());
        const std::vector<std::string> one_tag_subfield_values(subfields.extractSubfieldsAndNumericSubfields(subfield_spec));
        subfield_values.insert(std::end(subfield_values), one_tag_subfield_values.cbegin(), one_tag_subfield_values.cend());
    }
    return subfield_values;
}


std::string Record::getMainTitle() const {
    std::string title;
    const auto title_field(getFirstField("245"));
    if (unlikely(title_field == end()))
        return "";

    const Subfields subfields(title_field->getSubfields());
    std::string main_title(StringUtil::RightTrim(" \t/", subfields.getFirstSubfieldWithCode('a')));
    if (main_title.empty())
        return StringUtil::RightTrim(" \t/", subfields.getFirstSubfieldWithCode('b'));
    return main_title;
}


std::string Record::getCompleteTitle() const {
    const auto title_field(getFirstField("245"));
    if (title_field == end())
        return "";

    const Subfields title_subfields(title_field->getSubfields());
    const std::string subfield_a(StringUtil::RightTrim(" \t/", title_subfields.getFirstSubfieldWithCode('a')));
    const std::string subfield_b(StringUtil::RightTrim(" \t/", title_subfields.getFirstSubfieldWithCode('b')));
    if (subfield_a.empty() and subfield_b.empty())
        return "";

    std::string complete_title;
    if (subfield_a.empty())
        complete_title = subfield_b;
    else if (subfield_b.empty())
        complete_title = subfield_a;
    else { // Neither titleA nor titleB are null.
        complete_title = subfield_a;
        if (not StringUtil::StartsWith(subfield_b, " = "))
            complete_title += " : ";
        complete_title += subfield_b;
    }

    const std::string subfield_p(StringUtil::RightTrim(" \t/", title_subfields.getFirstSubfieldWithCode('p')));
    if (not subfield_p.empty()) {
        complete_title += ' ';
        complete_title += subfield_p;
    }

    const std::string subfield_n(StringUtil::RightTrim(" \t/", title_subfields.getFirstSubfieldWithCode('n')));
    if (not subfield_n.empty()) {
        complete_title += ' ';
        complete_title += subfield_n;
    }

    return complete_title;
}


std::string  Record::getSuperiorTitle() const {
    for (const auto &field : getTagRange("773")) {
        const auto superior_title_candidate(field.getFirstSubfieldWithCode('t'));
        if (likely(not superior_title_candidate.empty()))
            return superior_title_candidate;
    }
    for (const auto &field : getTagRange("773")) {
        const auto superior_title_candidate(field.getFirstSubfieldWithCode('a'));
        if (likely(not superior_title_candidate.empty()))
            return superior_title_candidate;
    }

    return "";
}


std::string Record::getSuperiorControlNumber() const {
    for (const auto &field : getTagRange("773")) {
        const auto w_subfield(field.getFirstSubfieldWithCode('w'));
        if (likely(StringUtil::StartsWith(w_subfield, "(DE-627)")))
            return w_subfield.substr(__builtin_strlen("(DE-627)"));
    }

    return "";
}


std::string Record::getSummary() const {
    std::string summary;
    for (const auto &field : getTagRange("520")) {
        const auto a_subfield(field.getFirstSubfieldWithCode('a'));
        if (unlikely(not a_subfield.empty())) {
            if (not summary.empty())
                summary += ' ';
            summary += a_subfield;
        }
    }

    return summary;
}


static inline bool ConsistsOfDigitsOnly(const std::string &s) {
    for (const char ch : s) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


std::string Record::getMostRecentPublicationYear(const std::string &fallback) const {
    const auto zwi_field(findTag("ZWI"));
    if (zwi_field != end()) {
        const auto publication_year(zwi_field->getFirstSubfieldWithCode('y'));
        if (not publication_year.empty())
            return publication_year;
    }

    if (isSerial()) {
        // 363$i w/ indicators 01 is the publication start year and 363$i with indicators 10 the publication end year.
        std::string start_year, end_year;
        for (const auto &_363_field : getTagRange("363")) {
            const auto subfield_i(_363_field.getFirstSubfieldWithCode('i'));
            if (subfield_i.empty())
                continue;
            const char indicator1(_363_field.getIndicator1()), indicator2(_363_field.getIndicator2());
            if (indicator1 == '0' and indicator2 == '1') // start year
                start_year = subfield_i;
            else if (indicator1 == '1' and indicator2 == '0') // end year
                end_year = subfield_i;
        }
        if (not end_year.empty())
            return end_year;
        if (not start_year.empty()) {
            const auto current_year(TimeUtil::GetCurrentYear());
            return current_year > start_year ? current_year : start_year;
        }
    }

    if (isReproduction()) {
        const auto _534_field(findTag("534"));
        if (unlikely(_534_field == end()))
            LOG_ERROR("No 534 Field for reproduction w/ control number " + getControlNumber() + "!");

        const auto c_contents(_534_field->getFirstSubfieldWithCode('c'));
        if (not c_contents.empty()) {
            static const auto digit_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(\\d+)"));
            if (digit_matcher->matched(c_contents))
                return (*digit_matcher)[1];
        }
    }

    if ((isArticle() or isReviewArticle()) and not isMonograph()) {
        for (const auto &_936_field : getTagRange("936")) {
            const auto j_contents(_936_field.getFirstSubfieldWithCode('j'));
            if (not j_contents.empty()) {
                static const auto year_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(\\d{4})"));
                if (year_matcher->matched(j_contents))
                    return (*year_matcher)[1];
            }
        }
    }

    for (const auto &_190_field : getTagRange("190")) {
        const auto j_contents(_190_field.getFirstSubfieldWithCode('j'));
        if (likely(not j_contents.empty()))
            return j_contents;
    }

    const auto _008_field(findTag("008"));
    if (likely(_008_field != end())) {
        const auto &field_contents(_008_field->getContents());
        if (likely(field_contents.length() >= 16)) {
            const std::string year_candidate(field_contents.substr(11, 4));
            if (ConsistsOfDigitsOnly(year_candidate)) {
                if (year_candidate == "9999")
                    return TimeUtil::GetCurrentYear();
                else
                    return year_candidate;
            }
        }
        if (likely(field_contents.length() >= 12)) {
            const std::string year_candidate(field_contents.substr(7, 4));
            if (ConsistsOfDigitsOnly(year_candidate))
                return year_candidate;
        }
    }

    return fallback;
}


std::vector<std::string> Record::getDatesOfProductionEtc() const {
    std::vector<std::string> dates;
    for (const auto &field : getTagRange("264")) {
        const auto date_candidate(field.getFirstSubfieldWithCode('c'));
        if (not date_candidate.empty())
            dates.emplace_back(date_candidate);
    }

    std::sort(dates.begin(), dates.end());
    std::vector<std::string> filtered_dates;
    std::string last_start_year;
    for (const auto &date : dates) {
        const auto start_year(date.substr(0, 4));
        if (start_year > last_start_year) {
            filtered_dates.emplace_back(date);
            last_start_year = start_year;
        } else
            filtered_dates.back() = date;
    }

    return filtered_dates;
}


std::string Record::getMainAuthor() const {
    const auto field_100(findTag("100"));
    if (unlikely(field_100 == end()))
        return "";

    return field_100->getFirstSubfieldWithCode('a');
}


static const std::vector<std::string> AUTHOR_TAGS { "100", "109", "700" };


std::set<std::string> Record::getAllAuthors() const {
    std::set<std::string> author_names;
    for (const auto &tag : AUTHOR_TAGS) {
        for (const auto &field : getTagRange(tag)) {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == 'a')
                    author_names.emplace(subfield.value_);
            }
        }
    }

    return author_names;
}


std::map<std::string, std::string> Record::getAllAuthorsAndPPNs() const {
    std::map<std::string, std::string> author_names_to_authority_ppns_map;
    std::set<std::string> already_seen_author_names;
    for (const auto &tag : AUTHOR_TAGS) {
        for (const auto &field : getTagRange(tag)) {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == 'a' and already_seen_author_names.find(subfield.value_) == already_seen_author_names.end()) {
                    already_seen_author_names.emplace(subfield.value_);
                    author_names_to_authority_ppns_map[subfield.value_] = BSZUtil::GetK10PlusPPNFromSubfield(field, '0');
                }
            }
        }
    }

    return author_names_to_authority_ppns_map;
}


std::set<std::string> Record::getAllISSNs() const {
    static const std::vector<std::string> ISSN_TAGS_AND_SUBFIELDS { "022a", "029a", "440x", "490x", "730x", "773x", "776x", "780x", "785x" };
    std::set<std::string> all_issns;
    for (const auto &tag_and_subfield : ISSN_TAGS_AND_SUBFIELDS) {
        for (const auto &field: getTagRange(tag_and_subfield.substr(0, 3))) {
            const char subfield_code(tag_and_subfield[3]);
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == subfield_code)
                   all_issns.emplace(subfield.value_);
            }
        }
    }

    return all_issns;
}


std::set<std::string> Record::getDOIs() const {
    std::set<std::string> dois;
    for (const auto &field : getTagRange("024")) {
        const Subfields subfields(field.getSubfields());
        if (field.getIndicator1() == '7' and subfields.getFirstSubfieldWithCode('2') == "doi")
            dois.emplace(StringUtil::Trim(subfields.getFirstSubfieldWithCode('a')));
    }

    return dois;
}


std::set<std::string> Record::getISSNs() const {
    std::set<std::string> issns;
    for (const auto &field : getTagRange("022")) {
        const std::string first_subfield_a(field.getFirstSubfieldWithCode('a'));
        std::string normalised_issn;
        if (MiscUtil::NormaliseISSN(first_subfield_a, &normalised_issn))
            issns.insert(normalised_issn);;
    }

    return issns;
}


std::set<std::string> Record::getSuperiorISSNs() const {
    std::set<std::string> superior_issns;
    for (const auto &field : getTagRange("773")) {
        const std::string first_subfield_x(field.getFirstSubfieldWithCode('x'));
        std::string normalised_issn;
        if (MiscUtil::NormaliseISSN(first_subfield_x, &normalised_issn))
            superior_issns.insert(normalised_issn);
    }

    return superior_issns;
}

std::set<std::string> Record::getISBNs() const {
    std::set<std::string> isbns;
    for (const auto &field : getTagRange("020")) {
        const std::string first_subfield_a(field.getFirstSubfieldWithCode('a'));
        std::string normalised_isbn;
        if (MiscUtil::NormaliseISSN(first_subfield_a, &normalised_isbn))
            isbns.insert(normalised_isbn);
    }

    return isbns;
}


std::set<std::string> Record::getDDCs() const {
    std::set<std::string> ddcs;
    for (const auto &field : getTagRange("082"))
        // Many DDC's have superfluous backslashes which are non-standard and should be removed.
        ddcs.emplace(StringUtil::RemoveChars("/", field.getFirstSubfieldWithCode('a')));

    return ddcs;
}


std::set<std::string> Record::getRVKs() const {
    std::set<std::string> rvks;
    for (const auto &field : getTagRange("084")) {
        if (field.getFirstSubfieldWithCode('2') == "rvk")
            rvks.emplace(field.getFirstSubfieldWithCode('a'));
    }

    return rvks;
}


std::set<std::string> Record::getSSGNs() const {
    std::set<std::string> ssgns;
    for (const auto &field : getTagRange("084")) {
        if (field.getFirstSubfieldWithCode('2') == "ssgn") {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == 'a')
                    ssgns.insert(StringUtil::TrimWhite(subfield.value_));
            }
        }
    }

    return ssgns;
}


std::set<std::string> Record::getReferencedGNDNumbers(const std::set<std::string> &tags) const {
    std::set<std::string> referenced_gnd_numbers;
    for (const auto &field : fields_) {
        if (not field.isDataField())
            continue;

        if (not tags.empty() and tags.find(field.getTag().toString()) == tags.cend())
            continue;

        const char FIRST_TAG_CHAR(field.getTag().c_str()[0]);
        if (FIRST_TAG_CHAR == '6' or FIRST_TAG_CHAR == 'L') {
            for (const auto &subfield : field.getSubfields()) {
                if (subfield.code_ == '0' and StringUtil::StartsWith(subfield.value_, "(DE-588)"))
                    referenced_gnd_numbers.emplace(subfield.value_.substr(__builtin_strlen("(DE-588)")));
            }
        }
    }

    return referenced_gnd_numbers;
}


bool Record::getKeywordAndSynonyms(KeywordAndSynonyms * const keyword_and_synonyms) const {
    const auto record_type(getRecordType());
    if (unlikely(record_type != RecordType::AUTHORITY))
        LOG_ERROR("this function can only be applied to an authority record but the type of this record is "
                  + RecordTypeToString(record_type) + "!");

    for (const auto &canonical_keyword_field : getTagRange({ "150", "151" })) {
        if (unlikely(not canonical_keyword_field.hasSubfield('a')))
            continue;

        std::vector<std::string> synonyms;
        for (const auto &synonym_field : getTagRange(canonical_keyword_field.getTag() == "150" ? "450" : "451")) {
            const std::string synonym(synonym_field.getFirstSubfieldWithCode('a'));
            if (likely(not synonym.empty()))
                synonyms.emplace_back(synonym);
        }

        KeywordAndSynonyms temp(canonical_keyword_field.getTag(), canonical_keyword_field.getFirstSubfieldWithCode('a'), synonyms);
        keyword_and_synonyms->swap(temp);
        return true;
    }

    return false;
}


namespace {


const std::string LOCAL_FIELD_PREFIX("  ""\x1F""0"); // Every local field starts w/ this.


inline std::string GetLocalTag(const Record::Field &local_field) {
    return local_field.getContents().substr(LOCAL_FIELD_PREFIX.size(), Record::TAG_LENGTH);
}


inline bool LocalIndicatorsMatch(const char indicator1, const char indicator2, const Record::Field &local_field) {
    if (indicator1 != '?' and (indicator1 != local_field.getContents()[LOCAL_FIELD_PREFIX.size() + Record::TAG_LENGTH + 0]))
        return false;
    if (indicator2 != '?' and (indicator2 != local_field.getContents()[LOCAL_FIELD_PREFIX.size() + Record::TAG_LENGTH + 1]))
        return false;
    return true;
}


inline bool LocalTagMatches(const Tag &tag, const Record::Field &local_field) {
    return local_field.getContents().substr(LOCAL_FIELD_PREFIX.size(), Record::TAG_LENGTH) == tag.toString();
}


} // unnamed namespace


std::vector<Record::const_iterator> Record::findStartOfAllLocalDataBlocks() const {
    std::vector<const_iterator> block_start_iterators;

    std::string last_local_tag;
    const_iterator local_field(getFirstField("LOK"));
    if (local_field == end())
        return block_start_iterators;

    block_start_iterators.emplace_back(local_field);
    while (local_field != end() and local_field->getTag() == "LOK") {
        if (GetLocalTag(*local_field) < last_local_tag)
            block_start_iterators.emplace_back(local_field);
        last_local_tag = GetLocalTag(*local_field);
        ++local_field;
    }

    return block_start_iterators;
}


std::vector<Record::iterator> Record::findStartOfAllLocalDataBlocks() {
    std::vector<iterator> block_start_iterators;

    std::string last_local_tag;
    iterator local_field(getFirstField("LOK"));
    if (local_field == end())
        return block_start_iterators;

    block_start_iterators.emplace_back(local_field);
    while (local_field != end() and local_field->getTag() == "LOK") {
        if (GetLocalTag(*local_field) < last_local_tag)
            block_start_iterators.emplace_back(local_field);
        last_local_tag = GetLocalTag(*local_field);
        ++local_field;
    }

    return block_start_iterators;
}


void Record::deleteLocalBlocks(std::vector<iterator> &local_block_starts) {
    std::sort(local_block_starts.begin(), local_block_starts.end());

    std::vector<std::pair<iterator, iterator>> deletion_ranges;

    // Coalesce as many blocks as possible:
    auto block_start(local_block_starts.begin());
    while (block_start != local_block_starts.end()) {
        iterator range_start(*block_start);
        Tag last_local_tag(range_start->getLocalTag());
        iterator range_end(range_start + 1);
        for (;;) {
            if (range_end == fields_.end() or range_end->getTag() != "LOK") {
                deletion_ranges.emplace_back(range_start, range_end);
                goto coalescing_done;
            }

            // Start of a new block?
            if (range_end->getLocalTag() < last_local_tag) {
                ++block_start;
                if (block_start == local_block_starts.end() or range_end != *block_start) {
                    deletion_ranges.emplace_back(range_start, range_end);
                    break;
                }
            }

            last_local_tag = range_end->getLocalTag();
            ++range_end;
        }
    }

coalescing_done:
    for (auto deletion_range(deletion_ranges.rbegin()); deletion_range != deletion_ranges.rend(); ++deletion_range)
        fields_.erase(deletion_range->first, deletion_range->second);
}


static inline bool LocalIndicator1Matches(const Record::Field &field, const char indicator) {
    return indicator == '?' or indicator == field.getLocalIndicator1();
}


static inline bool LocalIndicator2Matches(const Record::Field &field, const char indicator) {
    return indicator == '?' or indicator == field.getLocalIndicator2();
}


static inline bool LocalIndicatorsMatch(const Record::Field &field, const char indicator1, const char indicator2) {
    return field.getLocalTag().isTagOfControlField()
           or (LocalIndicator1Matches(field, indicator1) and LocalIndicator2Matches(field, indicator2));
}


Record::ConstantRange Record::getLocalTagRange(const Tag &local_field_tag, const const_iterator &block_start, const char indicator1,
                                               const char indicator2) const
{
    if (unlikely(not block_start->isLocal()))
        LOG_ERROR("you must call this function w/ a local \"block_start\"!");

    const_iterator tag_range_start(block_start);
    Tag last_local_tag(tag_range_start->getLocalTag());
    for (;;) {
        const Tag tag(tag_range_start->getLocalTag());
        if (tag == local_field_tag and LocalIndicatorsMatch(*tag_range_start, indicator1, indicator2))
            break;
        ++tag_range_start;
        if (tag_range_start == fields_.cend() or tag_range_start->getTag() != "LOK" or tag_range_start->getLocalTag() < last_local_tag)
            return ConstantRange(fields_.cend(), fields_.cend());
        last_local_tag = tag_range_start->getLocalTag();
    }

    const_iterator tag_range_end(tag_range_start + 1);
    while (tag_range_end != fields_.cend() and tag_range_end->getTag() == "LOK" and tag_range_end->getLocalTag() == local_field_tag
           and LocalIndicatorsMatch(*tag_range_end, indicator1, indicator2))
        ++tag_range_end;

    return ConstantRange(tag_range_start, tag_range_end);
}


bool Record::insertField(const Tag &new_field_tag, const std::string &new_field_value) {
    auto insertion_location(fields_.begin());
    while (insertion_location != fields_.end() and new_field_tag > insertion_location->getTag())
        ++insertion_location;
    if (insertion_location != fields_.begin() and (insertion_location - 1)->getTag() == new_field_tag
        and not IsRepeatableField(new_field_tag))
        return false;
    fields_.emplace(insertion_location, new_field_tag, new_field_value);
    record_size_ += DIRECTORY_ENTRY_LENGTH + new_field_value.length() + 1 /* field separator */;
    return true;
}


bool Record::insertFieldAtEnd(const Tag &new_field_tag, const std::string &new_field_value) {
    auto insertion_location(fields_.begin());
    while (insertion_location != fields_.end() and new_field_tag >= insertion_location->getTag())
        ++insertion_location;
    if (insertion_location != fields_.begin() and (insertion_location - 1)->getTag() == new_field_tag
        and not IsRepeatableField(new_field_tag))
        return false;
    fields_.emplace(insertion_location, new_field_tag, new_field_value);
    record_size_ += DIRECTORY_ENTRY_LENGTH + new_field_value.length() + 1 /* field separator */;
    return true;
}


void Record::appendField(const Tag &new_field_tag, const std::string &field_contents, const char indicator1, const char indicator2) {
    if (unlikely(not fields_.empty() and fields_.back().getTag() > new_field_tag))
        LOG_ERROR("attempt to append a \"" + new_field_tag.toString() + "\" field after a \"" + fields_.back().getTag().toString()
                  + "\" field!");
    if (unlikely(not fields_.empty() and fields_.back().getTag() == new_field_tag and not IsRepeatableField(new_field_tag)))
        LOG_ERROR("attempt to append a second non-repeatable\"" + new_field_tag.toString() + "\" field! (1)");
    fields_.emplace_back(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + field_contents);
}


void Record::appendField(const Tag &new_field_tag, const Subfields &subfields, const char indicator1, const char indicator2) {
    if (unlikely(not fields_.empty() and fields_.back().getTag() > new_field_tag))
        LOG_ERROR("attempt to append a \"" + new_field_tag.toString() + "\" field after a \"" + fields_.back().getTag().toString()
                  + "\" field! (2)");
    if (unlikely(not fields_.empty() and fields_.back().getTag() == new_field_tag and not IsRepeatableField(new_field_tag)))
        LOG_ERROR("attempt to append a second non-repeatable\"" + new_field_tag.toString() + "\" field! (2)");
    fields_.emplace_back(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + subfields.toString());
}


void Record::appendField(const Field &field) {
    if (unlikely(not fields_.empty() and fields_.back().getTag() > field.getTag()))
        LOG_ERROR("attempt to append a \"" + field.getTag().toString() + "\" field after a \"" + fields_.back().getTag().toString()
                  + "\" field! (3)");
    if (unlikely(not fields_.empty() and fields_.back().getTag() == field.getTag() and not IsRepeatableField(field.getTag())))
        LOG_ERROR("attempt to append a second non-repeatable\"" + field.getTag().toString() + "\" field! (3)");
    fields_.emplace_back(field);
}


void Record::replaceField(const Tag &field_tag, const std::string &field_contents, const char indicator1, const char indicator2) {
    std::string new_field_value;
    new_field_value += indicator1;
    new_field_value += indicator2;
    new_field_value += field_contents;

    auto insertion_location(fields_.begin());
    while (insertion_location != fields_.end() and field_tag > insertion_location->getTag())
        ++insertion_location;

    if (insertion_location != fields_.end() and field_tag == insertion_location->getTag()) {
        record_size_ += new_field_value.size();
        record_size_ -= insertion_location->getContents().size();
        insertion_location->setContents(new_field_value);
        return;
    }

    fields_.emplace(insertion_location, field_tag, new_field_value);
    record_size_ += DIRECTORY_ENTRY_LENGTH + new_field_value.length() + 1 /* field separator */;
}


bool Record::addSubfield(const Tag &field_tag, const char subfield_code, const std::string &subfield_value) {
    const auto &field(std::find_if(fields_.begin(), fields_.end(),
                                   [&field_tag](const Field &field1) -> bool { return field1.getTag() == field_tag; }));
    if (field == fields_.end())
        return false;

    Subfields subfields(field->getContents());
    subfields.addSubfield(subfield_code, subfield_value);

    std::string new_field_value;
    new_field_value += field->getIndicator1();
    new_field_value += field->getIndicator2();
    new_field_value += subfields.toString();
    field->contents_ = new_field_value;

    return true;
}

bool Record::addSubfieldCreateFieldUnique(const Tag &field_tag, const char subfield_code, const std::string &subfield_value) {
    const auto &field(findTag(field_tag));
    bool success = true;

    if (field == fields_.end()) {
        success = insertField(field_tag, std::string(1, subfield_code) + subfield_value);
    }

    if (success) {
        const auto field_lookup = findTag(field_tag);
        if (not field_lookup->hasSubfieldWithValue(subfield_code, subfield_value)) {
            success = addSubfield(field_tag, subfield_code, subfield_value);
        }
    }
    return success;
}


bool Record::hasSubfieldWithValue(const Tag &field_tag, const char subfield_code, const std::string &subfield_value) const {
    const auto &field(findTag(field_tag));
    if (field == fields_.end()) {
        return false;
    }
    return field->hasSubfieldWithValue(subfield_code, subfield_value);
}


bool Record::edit(const std::vector<EditInstruction> &edit_instructions, std::string * const error_message) {
    bool failed_at_least_once(false);
    for (const auto &edit_instruction : edit_instructions) {
        switch (edit_instruction.type_) {
        case INSERT_FIELD:
            if (not insertField(edit_instruction.tag_, std::string(1, edit_instruction.indicator1_)
                                + std::string(1, edit_instruction.indicator2_) + edit_instruction.field_or_subfield_contents_))
            {
                *error_message = "failed to insert a " + edit_instruction.tag_.toString() + " field!";
                failed_at_least_once = true;
            }
            break;
        case INSERT_SUBFIELD:
            if (not insertField(edit_instruction.tag_, { { edit_instruction.subfield_code_, edit_instruction.field_or_subfield_contents_ } },
                                edit_instruction.indicator1_, edit_instruction.indicator2_))
            {
                *error_message = "failed to insert a " + edit_instruction.tag_.toString() + std::string(1, edit_instruction.subfield_code_)
                                 + " subfield!";
                failed_at_least_once = true;
            }
            break;
        case ADD_SUBFIELD:
            if (not addSubfield(edit_instruction.tag_, edit_instruction.subfield_code_, edit_instruction.field_or_subfield_contents_)) {
                *error_message = "failed to add a " + edit_instruction.tag_.toString() + std::string(1, edit_instruction.subfield_code_)
                                 + " subfield!";
                failed_at_least_once = true;
            }
            break;
        }
    }

    return not failed_at_least_once;
}


Record::ConstantRange Record::findFieldsInLocalBlock(const Tag &local_field_tag, const const_iterator &block_start, const char indicator1,
                                                     const char indicator2) const
{
    auto local_field(block_start);
    std::string last_local_tag;
    const_iterator range_start(fields_.end()), range_end(fields_.end());
    while (local_field != fields_.end() and local_field->getTag() == "LOK") {
        if (GetLocalTag(*local_field) < last_local_tag) // We found the start of a new local block! (with local tag "000")
            return Record::ConstantRange(fields_.end(), fields_.end());

        if (LocalIndicatorsMatch(indicator1, indicator2, *local_field) and LocalTagMatches(local_field_tag, *local_field)) {
            range_start = local_field;
            range_end = range_start + 1;
            while (range_end != fields_.end() and local_field->getTag() == "LOK") {
                if (not LocalIndicatorsMatch(indicator1, indicator2, *range_end) or not LocalTagMatches(local_field_tag, *range_end))
                    break;
                ++range_end;
            }

            return Record::ConstantRange(range_start, range_end);
        }

        last_local_tag = GetLocalTag(*local_field);
        ++local_field;
    }

    return ConstantRange(range_start, fields_.end());
}


Record::Range Record::findFieldsInLocalBlock(const Tag &local_field_tag, const iterator &block_start, const char indicator1,
                                             const char indicator2)
{
    auto local_field(block_start);
    std::string last_local_tag;
    iterator range_start(fields_.end()), range_end(fields_.end());
    while (local_field != fields_.end() and local_field->getTag() == "LOK") {
        if (GetLocalTag(*local_field) < last_local_tag) // We found the start of a new local block!
            return Record::Range(fields_.end(), fields_.end());

        if (LocalIndicatorsMatch(indicator1, indicator2, *local_field) and LocalTagMatches(local_field_tag, *local_field)) {
            range_start = local_field;
            range_end = range_start + 1;
            while (range_end != fields_.end() and local_field->getTag() == "LOK") {
                if (not LocalIndicatorsMatch(indicator1, indicator2, *range_end) or not LocalTagMatches(local_field_tag, *range_end))
                    break;
                ++range_end;
            }

            return Record::Range(range_start, range_end);
        }

        last_local_tag = GetLocalTag(*local_field);
        ++local_field;
    }

    return Range(range_start, fields_.end());
}


Record::const_iterator Record::getFirstLocalField(const Tag &local_field_tag, const const_iterator &block_start) const {
    auto local_field(block_start);
    std::string last_local_tag;
    while (local_field != fields_.end() and local_field->getTag() == "LOK") {
        const auto current_tag(GetLocalTag(*local_field));
        if (local_field_tag == current_tag)
            return local_field; // Success!

        if (current_tag < last_local_tag) // We found the start of a new local block!
            return fields_.cend();

        last_local_tag = GetLocalTag(*local_field);
        ++local_field;
    }

    return fields_.cend();
}


std::unordered_set<std::string> Record::getTagSet() const {
    std::unordered_set<std::string> tags;
    for (const auto &field : fields_)
        tags.emplace(field.getTag().toString());
    return tags;
}


size_t Record::deleteFields(const Tag &field_tag) {
    const auto start_iter(findTag(field_tag));
    if (start_iter == fields_.cend())
        return 0;

    auto end_iter(start_iter + 1);
    while (end_iter != fields_.cend() and end_iter->getTag() == field_tag)
        ++end_iter;

    fields_.erase(start_iter, end_iter);
    return end_iter - start_iter;
}


void Record::deleteFields(std::vector<size_t> field_indices) {
    std::sort(field_indices.begin(), field_indices.end(), std::greater<size_t>());
    for (const auto field_index : field_indices)
        fields_.erase(fields_.begin() + field_index);
}


bool Record::isValid(std::string * const error_message) const {
    if (fields_.empty() or fields_.front().getTag() != "001") {
        *error_message = "001 field is missing!";
        return false;
    }

    for (const auto &field : fields_) {
        if (field.isDataField()) {
            // Check subfield structure:
            if (unlikely(field.contents_.length() < 5)) {
                *error_message = "field contents are too small (< 5 bytes)! (tag: " + field.getTag().toString() + ")";
                return false;
            }

            auto ch(field.contents_.begin() + 2 /* indicators */);
            while (ch != field.contents_.end()) {
                if (unlikely(*ch != '\x1F')) {
                    *error_message = "subfield does not start with 0x1F! (tag: " + field.getTag().toString() + ")";
                    return false;
                }
                ++ch; // Skip over 0x1F.
                if (unlikely(ch == field.contents_.end())) {
                    *error_message = "subfield is missing a subfield code! (tag: " + field.getTag().toString() + ")";
                    return false;
                }
                ++ch; // Skip over the subfield code.
                if (unlikely(ch == field.contents_.end() or *ch == '\x1F'))
                    LOG_WARNING("subfield '" + std::string(1, *(ch - 1)) + "' is empty! (tag: " + field.getTag().toString() + ")");

                // Skip over the subfield contents:
                while (ch != field.contents_.end() and *ch != '\x1F')
                    ++ch;
            }
        }
    }

    return true;
}


bool Record::fieldOrSubfieldMatched(const std::string &field_or_field_and_subfield_code, RegexMatcher * const regex_matcher) const {
    if (unlikely(field_or_field_and_subfield_code.length() < TAG_LENGTH or field_or_field_and_subfield_code.length() > TAG_LENGTH + 1))
        LOG_ERROR("\"field_or_field_and_subfield_code\" must be a tag or a tag plus a subfield code!");

    const char subfield_code((field_or_field_and_subfield_code.length() == TAG_LENGTH + 1) ? field_or_field_and_subfield_code[TAG_LENGTH]
                                                                                           : '\0');
    for (const auto &field : getTagRange(field_or_field_and_subfield_code.substr(0, TAG_LENGTH))) {
        if (subfield_code != '\0' and field.hasSubfield(subfield_code)) {
            if (regex_matcher->matched(field.getFirstSubfieldWithCode(subfield_code)))
                return true;
        } else if (regex_matcher->matched(field.getContents()))
            return true;
    }

    return false;
}


std::vector<Record::iterator> Record::getMatchedFields(const std::string &field_or_field_and_subfield_code,
                                                       RegexMatcher * const regex_matcher)
{
    if (unlikely(field_or_field_and_subfield_code.length() < TAG_LENGTH or field_or_field_and_subfield_code.length() > TAG_LENGTH + 1))
        LOG_ERROR("\"field_or_field_and_subfield_code\" must be a tag or a tag plus a subfield code!");

    const char subfield_code((field_or_field_and_subfield_code.length() == TAG_LENGTH + 1) ? field_or_field_and_subfield_code[TAG_LENGTH]
                                                                                               : '\0');
    std::vector<iterator> matched_fields;
    const Range field_range(getTagRange(field_or_field_and_subfield_code.substr(0, TAG_LENGTH)));
    for (auto field_itr(field_range.begin()); field_itr != field_range.end(); ++field_itr) {
        const auto &field(*field_itr);
        if (subfield_code != '\0' and field.hasSubfield(subfield_code)) {
            if (regex_matcher->matched(field.getFirstSubfieldWithCode(subfield_code)))
                matched_fields.emplace_back(field_itr);
        } else if (regex_matcher->matched(field.getContents()))
            matched_fields.emplace_back(field_itr);
    }

    return matched_fields;
}


enum class MediaType { XML, MARC21, OTHER };


const ThreadSafeRegexMatcher MARC21_MAGIC_MATCHER("(^[0-9]{5})([acdnp][^bhlnqsu-z]|[acdnosx][z]|[cdn][uvxy]|[acdn][w]|[cdn][q])");


static MediaType GetMediaType(const std::string &filename) {
    File input(filename, "r");
    if (input.anErrorOccurred())
        return MediaType::OTHER;

    char magic[8];
    const size_t read_count(input.read(magic, sizeof(magic) - 1));
    if (read_count != sizeof(magic) - 1) {
        if (read_count == 0) {
            LOG_WARNING("empty file \"" + filename + "\"!");
            const std::string extension(FileUtil::GetExtension(filename));
            if (::strcasecmp(extension.c_str(), "mrc") == 0 or ::strcasecmp(extension.c_str(), "marc") == 0
                or ::strcasecmp(extension.c_str(), "raw") == 0) {
                return MediaType::MARC21;
            } else if (::strcasecmp(extension.c_str(), "xml") == 0)
                return MediaType::XML;
        }
        return MediaType::OTHER;
    }
    magic[sizeof(magic) - 1] = '\0';

    if (StringUtil::StartsWith(magic, "<?xml"))
        return MediaType::XML;

    return MARC21_MAGIC_MATCHER.match(magic) ? MediaType::MARC21 : MediaType::XML;
}


std::string FileTypeToString(const FileType file_type) {
    switch (file_type) {
    case FileType::AUTO:
        return "AUTO";
    case FileType::BINARY:
        return "BINARY";
    case FileType::XML:
        return "XML";
    default:
        LOG_ERROR("unknown file type " + std::to_string(static_cast<int>(file_type)) + "!");
    }
}


FileType GuessFileType(const std::string &filename, const GuessFileTypeBehaviour guess_file_type_behaviour) {
    if (guess_file_type_behaviour == GuessFileTypeBehaviour::ATTEMPT_A_READ and FileUtil::Exists(filename)
        and not FileUtil::IsPipeOrFIFO(filename))
    {
        switch (GetMediaType(filename)) {
        case MediaType::XML:
            return FileType::XML;
        case MediaType::MARC21:
            return FileType::BINARY;
        default:
            LOG_ERROR("\"" + filename + "\" contains neither MARC-21 nor MARC-XML data!");
        }
    }

    if (StringUtil::EndsWith(filename, ".mrc", /* ignore_case = */true)
        or StringUtil::EndsWith(filename, ".marc", /* ignore_case = */true)
        or StringUtil::EndsWith(filename, ".raw", /* ignore_case = */true))
        return FileType::BINARY;
    if (StringUtil::EndsWith(filename, ".xml", /* ignore_case = */true))
        return FileType::XML;

    LOG_ERROR("can't guess the file type of \"" + filename + "\"!");
}


std::unique_ptr<Reader> Reader::Factory(const std::string &input_filename, FileType reader_type) {
    if (reader_type == FileType::AUTO)
        reader_type = GuessFileType(input_filename);

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    return (reader_type == FileType::XML) ? std::unique_ptr<Reader>(new XmlReader(input.release()))
                                          : std::unique_ptr<Reader>(new BinaryReader(input.release()));
}


BinaryReader::BinaryReader(File * const input)
    : Reader(input), next_record_start_(0)
{
    struct stat stat_buf;
    if (::fstat(input->getFileDescriptor(), &stat_buf) != 0)
        LOG_ERROR("fstat(2) on \"" + input->getPath() + "\" failed!");
    if (S_ISFIFO(stat_buf.st_mode))
        mmap_ = nullptr;
    else {
        offset_ = 0;
        input_file_size_ = stat_buf.st_size;
        if (input_file_size_ == 0) {
            mmap_ = nullptr;
            last_record_ = Record();
            return;
        }
        mmap_ = reinterpret_cast<char *>(::mmap(nullptr, stat_buf.st_size, PROT_READ, MAP_PRIVATE, input->getFileDescriptor(), 0));
        if (mmap_ == MAP_FAILED or mmap_ == nullptr)
            LOG_ERROR("Failed to mmap \"" + input->getPath() + "\"!");
    }

    last_record_ = actualRead();
}


BinaryReader::~BinaryReader() {
    if (mmap_ != nullptr and ::munmap((void *)(mmap_), input_file_size_) != 0)
        LOG_ERROR("munmap(2) failed!");
}


Record BinaryReader::read() {
    if (unlikely(not last_record_))
        return last_record_;

    Record new_record;
    do {
        next_record_start_ = (mmap_ == nullptr) ? input_->tell() : offset_;
        new_record = actualRead();
        if (unlikely(new_record.getControlNumber() == last_record_.getControlNumber()))
            last_record_.merge(new_record);
    } while (new_record.getControlNumber() == last_record_.getControlNumber());

    new_record.swap(last_record_);

    // This should not be necessary unless we got bad data!
    new_record.sortFieldTags(new_record.begin(), new_record.end());

    return new_record;
}


Record BinaryReader::actualRead() {
    if (mmap_ == nullptr) {
        char buf[Record::MAX_RECORD_LENGTH];
        size_t bytes_read;
        if (unlikely((bytes_read = input_->read(buf, Record::RECORD_LENGTH_FIELD_LENGTH)) == 0))
            return Record();

        if (unlikely(bytes_read != Record::RECORD_LENGTH_FIELD_LENGTH))
            LOG_ERROR("failed to read record length!");
        const unsigned record_length(ToUnsigned(buf, Record::RECORD_LENGTH_FIELD_LENGTH));

        bytes_read = input_->read(buf + Record::RECORD_LENGTH_FIELD_LENGTH, record_length - Record::RECORD_LENGTH_FIELD_LENGTH);
        if (unlikely(bytes_read != record_length - Record::RECORD_LENGTH_FIELD_LENGTH))
            LOG_ERROR("failed to read a record from \"" + input_->getPath() + "\"!");

        return Record(record_length, buf);
    } else { // Use memory-mapped I/O.
        if (unlikely(offset_ == input_file_size_))
            return Record();

        if (unlikely(offset_ + Record::RECORD_LENGTH_FIELD_LENGTH >= input_file_size_))
            LOG_ERROR("not enough remaining room in \"" + input_->getPath()
                      + "\" for a record length in the memory mapping! (input_file_size_ = "
                      + std::to_string(input_file_size_) + ", offset_ = " + std::to_string(offset_)
                      + "), file may be truncated!");
        const unsigned record_length(ToUnsigned(mmap_ + offset_, Record::RECORD_LENGTH_FIELD_LENGTH));

        if (unlikely(offset_ + record_length > input_file_size_))
            LOG_ERROR("not enough remaining room in \"" + input_->getPath()
                      + "\" for the rest of the record in the memory mapping, file may be truncated!");
        offset_ += record_length;

        return Record(record_length, mmap_ + offset_ - record_length);
    }
}


void BinaryReader::rewind() {
    if (mmap_ == nullptr) {
        struct stat stat_buf;
        if (::fstat(input_->getFileDescriptor(), &stat_buf) != 0)
            LOG_ERROR("fstat(2) on \"" + input_->getPath() + "\" failed!");
        if (S_ISFIFO(stat_buf.st_mode))
            LOG_ERROR("can't rewind a FIFO (" + input_->getPath() + ")!");
        input_->rewind();
    } else
        offset_ = 0;
    next_record_start_ = 0;
    last_record_ = actualRead();
}


bool BinaryReader::seek(const off_t offset, const int whence) {
    if (mmap_ == nullptr) {
        if (input_->seek(offset, whence)) {
            next_record_start_ = input_->tell();
            last_record_ = actualRead();
            return true;
        } else
            return false;
    } else { // Use memory-mapped I/O.
        switch (whence) {
        case SEEK_SET:
            if (offset < 0 or static_cast<size_t>(offset) > input_file_size_)
                return false;
            offset_ = offset;
            break;
        case SEEK_CUR:
            if (static_cast<ssize_t>(offset_) + offset < 0
                or static_cast<ssize_t>(offset_) + offset > static_cast<ssize_t>(input_file_size_))
                return false;
            offset_ += offset;
            break;
        case SEEK_END:
            if (offset < 0 or static_cast<size_t>(offset) > input_file_size_)
                return false;
            offset_ = input_file_size_ - offset;
            break;
        default:
            LOG_ERROR("bad value for \"whence\": " + std::to_string(whence) + "!");
        }
        next_record_start_ = offset_;
        last_record_ = actualRead();

        return true;
    }
}


Record XmlReader::read() {
    Record new_record;

    XMLSubsetParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    while (getNext(&type, &attrib_map, &data) and type == XMLSubsetParser<File>::CHARACTERS)
        /* Intentionally empty! */;

    if (unlikely(type == XMLSubsetParser<File>::CLOSING_TAG and data == namespace_prefix_ + "collection")) {
        new_record.sortFieldTags(new_record.begin(), new_record.end());
        return new_record;
    }

    //
    // Now parse a <record>:
    //

    if (unlikely(type != XMLSubsetParser<File>::OPENING_TAG or data != namespace_prefix_ + "record")) {
        const bool tag_found(type == XMLSubsetParser<File>::OPENING_TAG
                             or type == XMLSubsetParser<File>::CLOSING_TAG);
        if (type == XMLSubsetParser<File>::ERROR)
            throw std::runtime_error("in MARC::XmlReader::read: opening <" + namespace_prefix_
                                     + "record> tag expected while parsing \"" + input_->getPath() + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "! ("
                                     + xml_parser_->getLastErrorMessage() + ")");
        else
            throw std::runtime_error("in MARC::XmlReader::read: opening <" + namespace_prefix_
                                     + "record> tag expected while parsing \"" + input_->getPath() + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "! (Found: "
                                     + XMLSubsetParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data + ")") : ")"));
    }

    parseLeader(input_->getPath(), &new_record);

    bool datafield_seen(false);
    for (;;) { // Process "datafield" and "controlfield" sections.
        if (unlikely(not getNext(&type, &attrib_map, &data)))
            throw std::runtime_error("in MARC::XmlReader::read: error while parsing \"" + input_->getPath()
                                     + "\": " + xml_parser_->getLastErrorMessage() + " on line "
                                     + std::to_string(xml_parser_->getLineNo()) + "!");

        if (type == XMLSubsetParser<File>::CLOSING_TAG) {
            if (unlikely(data != namespace_prefix_ + "record"))
                throw std::runtime_error("in MARC::MarcUtil::Record::XmlFactory: closing </record> tag expected "
                                         "while parsing \"" + input_->getPath() + "\" on line "
                                         + std::to_string(xml_parser_->getLineNo()) + "!");
            new_record.sortFieldTags(new_record.begin(), new_record.end());
            return new_record;
        }

        if (type != XMLSubsetParser<File>::OPENING_TAG
            or (data != namespace_prefix_ + "datafield" and data != namespace_prefix_ + "controlfield"))
            throw std::runtime_error("in MARC::XmlReader::read: expected either <" + namespace_prefix_
                                     + "controlfield> or <" + namespace_prefix_ + "datafield> on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_->getPath()
                                     + "\"!");

        if (unlikely(attrib_map.find("tag") == attrib_map.end()))
            throw std::runtime_error("in MARC::XmlReader::read: expected a \"tag\" attribute as part of an opening "
                                     "<" + namespace_prefix_ + "controlfield> or <" + namespace_prefix_
                                     + "datafield> tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_->getPath() + "\"!");

        if (data == namespace_prefix_ + "controlfield") {
            if (unlikely(datafield_seen))
                throw std::runtime_error("in MARC::MarcUtil::Record::XmlFactory: <" + namespace_prefix_
                                         + "controlfield> found after <" + namespace_prefix_ + "datafield> on "
                                         "line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                                         + input_->getPath() + "\"!");
            parseControlfield(input_->getPath(), attrib_map["tag"], &new_record);

        } else {
            datafield_seen = true;
            parseDatafield(input_->getPath(), attrib_map, attrib_map["tag"], &new_record);
        }
    }
}


void XmlReader::rewind() {
    // We can't handle FIFO's here:
    struct stat stat_buf;
    if (unlikely(fstat(input_->getFileDescriptor(), &stat_buf) and S_ISFIFO(stat_buf.st_mode)))
        LOG_ERROR("can't rewind a FIFO!");

    input_->rewind();

    delete xml_parser_;
    xml_parser_ = new XMLSubsetParser<File>(input_);

    skipOverStartOfDocument();
}


namespace {


bool ParseLeader(const std::string &leader_string, std::string * const leader, std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();

    if (leader_string.size() != Record::LEADER_LENGTH) {
        if (err_msg != nullptr)
            *err_msg = "Leader length must be " + std::to_string(Record::LEADER_LENGTH) +
                       ", found " + std::to_string(leader_string.size()) + "! (Leader bytes are "
                       + StringUtil:: CStyleEscape(leader_string) + ")";
        return false;
    }

    unsigned record_length;
    if (std::sscanf(leader_string.substr(0, 5).data(), "%5u", &record_length) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse record length! (Found \"" + StringUtil::CStyleEscape(leader_string.substr(0, 5))
                       + "\")";
        return false;
    }

    unsigned base_address_of_data;
    if (std::sscanf(leader_string.substr(12, 5).data(), "%5u", &base_address_of_data) != 1) {
        if (err_msg != nullptr)
            *err_msg = "Can't parse base address of data!";
        return false;
    }

    //
    // Validity checks:
    //

    // Check indicator count:
    if (leader_string[10] != '2')
        LOG_WARNING("invalid indicator count '" + leader_string.substr(10, 1) + "'!");

    // Check subfield code length:
    if (leader_string[11] != '2')
        LOG_WARNING("invalid subfield code length! (Leader bytes are " + StringUtil:: CStyleEscape(leader_string) + ")");

    // Check entry map:
    if (leader_string.substr(20, 3) != "450")
        LOG_WARNING("invalid entry map!");

    *leader = leader_string;

    return true;
}


} // unnamed namespace


void XmlReader::parseLeader(const std::string &input_filename, Record * const new_record) {
    XMLSubsetParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    while (getNext(&type, &attrib_map, &data) and type == XMLSubsetParser<File>::CHARACTERS)
        /* Intentionally empty! */;
    if (unlikely(type != XMLSubsetParser<File>::OPENING_TAG or data != namespace_prefix_ + "leader"))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: opening <marc:leader> tag expected while "
                                 "parsing \"" + input_filename + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");

    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser_->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(type != XMLSubsetParser<File>::CHARACTERS or data.length() != Record::LEADER_LENGTH)) {
        LOG_WARNING("leader data expected while parsing \"" + input_filename + "\" on line "
                + std::to_string(xml_parser_->getLineNo()) + ".");
        if (unlikely(not getNext(&type, &attrib_map, &data)))
            throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while skipping to </"
                                     + namespace_prefix_ + "leader>!");
        if (unlikely(type != XMLSubsetParser<File>::CLOSING_TAG or data != namespace_prefix_ + "leader")) {
            const bool tag_found(type == XMLSubsetParser<File>::OPENING_TAG
                                 or type == XMLSubsetParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::ParseLeader: closing </" + namespace_prefix_
                                     + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                     + std::to_string(xml_parser_->getLineNo())
                                     + ". (Found: " + XMLSubsetParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
        return;
    }

    if (data.substr(0, 5) == "     ") // record length
        data = "00000" + data.substr(5);
    if (data.substr(12, 5) == "     ") // base address of data
        data = data.substr(0, 12) + "00000" + data.substr(12 + 5);
    std::string err_msg;
    if (unlikely(not ParseLeader(data, &new_record->leader_, &err_msg)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing leader data: " + err_msg);

    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: error while parsing \"" + input_filename + "\": "
                                 + xml_parser_->getLastErrorMessage() + " on line "
                                 + std::to_string(xml_parser_->getLineNo()) + ".");
    if (unlikely(type != XMLSubsetParser<File>::CLOSING_TAG or data != namespace_prefix_ + "leader")) {
        const bool tag_found(type == XMLSubsetParser<File>::OPENING_TAG
                             or type == XMLSubsetParser<File>::CLOSING_TAG);
        throw std::runtime_error("in MARC::XmlReader::ParseLeader: closing </" + namespace_prefix_
                                 + "leader> tag expected while parsing \"" + input_filename + "\" on line "
                                 + std::to_string(xml_parser_->getLineNo())
                                 + ". (Found: " + XMLSubsetParser<File>::TypeToString(type)
                                 + (tag_found ? (":" + data) : ""));
    }
}


// Returns true if we found a normal control field and false if we found an empty control field.
void XmlReader::parseControlfield(const std::string &input_filename, const std::string &tag,
                                  Record * const record)
{
    XMLSubsetParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    if (unlikely(not getNext(&type, &attrib_map, &data)))
        throw std::runtime_error("in MARC::XmlReader::parseControlfield: failed to get next XML element!");

        // Do we have an empty control field?
    if (unlikely(type == XMLSubsetParser<File>::CLOSING_TAG and data == namespace_prefix_ + "controlfield")) {
        LOG_WARNING("empty \"" + tag + "\" control field on line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                + input_filename + "\"!");
        return;
    }

    if (type != XMLSubsetParser<File>::CHARACTERS)
        std::runtime_error("in MARC::XmlReader::parseControlfield: character data expected on line "
                           + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename + "\"!");
    Record::Field new_field(tag, data);

    if (unlikely(not getNext(&type, &attrib_map, &data) or type != XMLSubsetParser<File>::CLOSING_TAG
                 or data != namespace_prefix_ + "controlfield"))
        throw std::runtime_error("in MARC::XmlReader::parseControlfield: </controlfield> expected on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");

    record->fields_.emplace_back(new_field);
    record->record_size_ += Record::DIRECTORY_ENTRY_LENGTH + new_field.getContents().size() + 1 /* end-of-field */;
    return;
}


void XmlReader::parseDatafield(const std::string &input_filename,
                               const std::map<std::string, std::string> &datafield_attrib_map,
                               const std::string &tag, Record * const record)
{
    const auto ind1(datafield_attrib_map.find("ind1"));
    if (unlikely(ind1 == datafield_attrib_map.cend() or ind1->second.length() != 1))
        throw std::runtime_error("in MARC::XmlReader::ParseDatafield: bad or missing \"ind1\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");
    std::string field_data(ind1->second);

    const auto ind2(datafield_attrib_map.find("ind2"));
    if (unlikely(ind2 == datafield_attrib_map.cend() or ind2->second.length() != 1))
        throw std::runtime_error("in MARC::XmlReader::ParseDatafield: bad or missing \"ind2\" attribute on line "
                                 + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                 + "\"!");
    field_data += ind2->second;

    XMLSubsetParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;
    for (;;) {
        while (getNext(&type, &attrib_map, &data) and type == XMLSubsetParser<File>::CHARACTERS)
            /* Intentionally empty! */;

        if (type == XMLSubsetParser<File>::ERROR)
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: error while parsing a data field on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\": " + xml_parser_->getLastErrorMessage());

        if (type == XMLSubsetParser<File>::CLOSING_TAG and data == namespace_prefix_ + "datafield") {
            // If the field contents consists of the indicators only, we drop it.
            if (unlikely(field_data.length() == 1 /*indicator1*/ + 1/*indicator2*/)) {
                LOG_WARNING("dropped empty \"" + tag + "\" field!");
                return;
            }

            record->fields_.emplace_back(tag, field_data);
            record->record_size_ += Record::DIRECTORY_ENTRY_LENGTH + field_data.length() + 1 /* end-of-field */;
            return;
        }

        // 1. <subfield code=...>
        if (unlikely(type != XMLSubsetParser<File>::OPENING_TAG or data != namespace_prefix_ + "subfield")) {
            const bool tag_found(type == XMLSubsetParser<File>::OPENING_TAG
                                 or type == XMLSubsetParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: expected <" + namespace_prefix_ +
                                     "subfield> opening tag on line " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename
                                     + "\"! (Found: " + XMLSubsetParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
        if (unlikely(attrib_map.find("code") == attrib_map.cend() or attrib_map["code"].length() != 1))
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: missing or invalid \"code\" attribute as "
                                     "rt   of the <subfield> tag " + std::to_string(xml_parser_->getLineNo())
                                     + " in file \"" + input_filename + "\"!");
        field_data += '\x1F' + attrib_map["code"];

        // 2. Subfield data.
        if (unlikely(not getNext(&type, &attrib_map, &data) or type != XMLSubsetParser<File>::CHARACTERS)) {
            if (type == XMLSubsetParser<File>::CLOSING_TAG and data == namespace_prefix_ + "subfield") {
                LOG_WARNING("found an empty subfield on line " + std::to_string(xml_parser_->getLineNo()) + " in file \""
                        + input_filename + "\"!");
                field_data.resize(field_data.length() - 2); // Remove subfield delimiter and code.
                continue;
            }
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: error while looking for character data "
                                     "after <" + namespace_prefix_ + "subfield> tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\": " + xml_parser_->getLastErrorMessage());
        }
        field_data += data;

        // 3. </subfield>
        if (unlikely(not getNext(&type, &attrib_map, &data) or type != XMLSubsetParser<File>::CLOSING_TAG
                     or data != namespace_prefix_ + "subfield"))
        {
            const bool tag_found(type == XMLSubsetParser<File>::OPENING_TAG
                                 or type == XMLSubsetParser<File>::CLOSING_TAG);
            throw std::runtime_error("in MARC::XmlReader::parseDatafield: expected </" + namespace_prefix_
                                     + "subfield> closing tag on line "
                                     + std::to_string(xml_parser_->getLineNo()) + " in file \"" + input_filename
                                     + "\"! (Found: " + XMLSubsetParser<File>::TypeToString(type)
                                     + (tag_found ? (":" + data) : ""));
        }
    }
}


void XmlReader::skipOverStartOfDocument() {
    XMLSubsetParser<File>::Type type;
    std::map<std::string, std::string> attrib_map;
    std::string data;

    while (getNext(&type, &attrib_map, &data)) {
        if (type == XMLSubsetParser<File>::OPENING_TAG and data == namespace_prefix_ + "collection")
            return;
    }

        // We should never get here!
    throw std::runtime_error("in MARC::XmlReader::skipOverStartOfDocument: error while trying to skip to "
                             "<" + namespace_prefix_ + "collection>:  \""
                             + xml_parser_->getDataSource()->getPath() + "\": "
                             + xml_parser_->getLastErrorMessage() + " on line "
                             + std::to_string(xml_parser_->getLineNo()) + "!");
}


bool XmlReader::getNext(XMLSubsetParser<File>::Type * const type,
                        std::map<std::string, std::string> * const attrib_map, std::string * const data)
{
    if (unlikely(not xml_parser_->getNext(type, attrib_map, data)))
        return false;

    if (*type != XMLSubsetParser<File>::OPENING_TAG)
        return true;

    auto key_and_value(attrib_map->find("xmlns"));
    if (unlikely(key_and_value != attrib_map->cend() and key_and_value->second != "http://www.loc.gov/MARC21/slim"))
        throw std::runtime_error("in MARC::XmlReader::getNext: opening tag has unsupported \"xmlns\" attribute "
                                 "near line #" + std::to_string(xml_parser_->getLineNo()) + " in \"" + getPath()
                                 + "\"!");

    key_and_value = attrib_map->find("xmlns:marc");
    if (unlikely(key_and_value != attrib_map->cend())) {
        if (unlikely(key_and_value->second != "http://www.loc.gov/MARC21/slim"))
            throw std::runtime_error("in MARC::XmlReader::getNext: opening tag has unsupported \"xmlns:marc\" "
                                     "attribute near line #" + std::to_string(xml_parser_->getLineNo()) + " in \""
                                     + getPath() + "\"!");
        else
            namespace_prefix_ = "marc:";
    }

    return true;
}


std::unique_ptr<Writer> Writer::Factory(const std::string &output_filename, FileType writer_type,
                                        const WriterMode writer_mode)
{
    if (writer_type == FileType::AUTO) {
        if (output_filename == "/dev/null")
            writer_type = FileType::BINARY;
        else
            writer_type = GuessFileType(output_filename, GuessFileTypeBehaviour::USE_THE_FILENAME_ONLY);
    }

    std::unique_ptr<File> output(writer_mode == WriterMode::OVERWRITE
                                 ? FileUtil::OpenOutputFileOrDie(output_filename)
                                 : FileUtil::OpenForAppendingOrDie(output_filename));

    return (writer_type == FileType::XML) ? std::unique_ptr<Writer>(new XmlWriter(output.release()))
                                          : std::unique_ptr<Writer>(new BinaryWriter(output.release()));
}


void BinaryWriter::write(const Record &record) {
    std::string error_message;
    if (not record.isValid(&error_message))
        LOG_ERROR("trying to write an invalid record: " + error_message + " (Control number: " + record.getControlNumber() + ")");
    output_->write(record.toBinaryString());
}


XmlWriter::~XmlWriter() {
    // the MarcXmlWriter owns the File pointer as well, so we cede ownership to it entirely
    output_.release();
}


void XmlWriter::write(const Record &record) {
    std::string error_message;
    if (not record.isValid(&error_message))
        LOG_ERROR("trying to write an invalid record: " + error_message + " (Control number: " + record.getControlNumber() + ")");
    record.toXmlStringHelper(&xml_writer_);
}


void FileLockedComposeAndWriteRecord(Writer * const marc_writer, const Record &record) {
    FileLocker file_locker(marc_writer->getFile().getFileDescriptor(), FileLocker::READ_WRITE);
    if (unlikely(not (marc_writer->getFile().seek(0, SEEK_END))))
        LOG_ERROR("failed to seek to the end of \"" + marc_writer->getFile().getPath() + "\"!");
    marc_writer->write(record);
    if (unlikely(not marc_writer->flush()))
        LOG_ERROR("failed to flush to \"" +  marc_writer->getFile().getPath() + "\"!");
}


unsigned RemoveDuplicateControlNumberRecords(const std::string &marc_filename) {
    unsigned dropped_count(0);
    std::string temp_filename;
    // Open a scope because we need the MARC::Reader to go out-of-scope before we unlink the associated file.
    {
        std::unique_ptr<Reader> marc_reader(Reader::Factory(marc_filename));
        temp_filename = "/tmp/" + std::string(::basename(::program_invocation_name)) + std::to_string(::getpid())
                        + (marc_reader->getReaderType() == FileType::XML ? ".xml" : ".mrc");
        std::unique_ptr<Writer> marc_writer(Writer::Factory(temp_filename));
        std::unordered_set<std::string> already_seen_control_numbers;
        while (const Record record = marc_reader->read()) {
            const std::string control_number(record.getControlNumber());
            if (already_seen_control_numbers.find(control_number) == already_seen_control_numbers.end()) {
                marc_writer->write(record);
                already_seen_control_numbers.emplace(control_number);
            } else
                ++dropped_count;
        }
    }
    FileUtil::RenameFileOrDie(temp_filename, marc_filename, true /* remove_target */, true /* copy accros filesystems */);
    return dropped_count;
}


bool IsValidMarcFile(const std::string &filename, std::string * const err_msg, const FileType file_type) {
    try {
      std::unique_ptr<Reader> reader(Reader::Factory(filename, file_type));
      while (const Record record = reader->read()) {
          if (not record.isValid(err_msg))
              return false;
      }
      return true;
    } catch (const std::exception &x) {
      *err_msg = x.what();
      return false;
    }
}


std::string GetLanguageCode(const Record &record) {
    const auto _008_field(record.getFirstField("008"));
    if (_008_field == record.end())
        return "";
    // Language codes start at offset 35 and have a length of 3.
    const auto _008_contents(_008_field->getContents());
    if (_008_contents.length() < 38)
        return "";

    return _008_contents.substr(35, 3) == "   " ? "" : _008_contents.substr(35, 3);
}


size_t GetLanguageCodes(const Record &record, std::set<std::string> * const language_codes) {
    language_codes->clear();

    const auto _008_language_code(GetLanguageCode(record));
    if (not _008_language_code.empty())
        language_codes->emplace(_008_language_code);

    // See https://www.loc.gov/marc/bibliographic/bd041.html if you ever hope to understand this eimplementation!
    static const std::set<char> LANGUAGE_CODE_SUBFIELD_CODES{ 'a', 'b', 'd', 'e', 'f', 'g', 'h', 'j', 'k', 'm', 'n' };
    for (const auto &_041_field : record.getTagRange("041")) {
        for (const auto &subfield : _041_field.getSubfields()) {
            if (LANGUAGE_CODE_SUBFIELD_CODES.find(subfield.code_) != LANGUAGE_CODE_SUBFIELD_CODES.cend())
                language_codes->emplace(subfield.value_);
        }
    }

    return language_codes->size();
}


bool GetGNDCode(const Record &record, std::string * const gnd_code) {
    gnd_code->clear();
    for (const auto &_035_field : record.getTagRange("035")) {
        const Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_field(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_field, "(DE-588)")) {
            *gnd_code = _035a_field.substr(8);
            return not gnd_code->empty();
        }
    }
    return false;
}


bool GetWikidataId(const Record &record, std::string * const wikidata_id) {
    wikidata_id->clear();
    for (const auto &_024_field : record.getTagRange("024")) {
        const Subfields _024_subfields(_024_field.getSubfields());
        const std::string _024a_field(_024_subfields.getFirstSubfieldWithCode('a'));
        const std::string _024_2_field(_024_subfields.getFirstSubfieldWithCode('2'));
        if (StringUtil::Contains(_024_2_field, "wikidata")) {
            *wikidata_id = _024a_field;
            return not wikidata_id->empty();
        }
    }
    return false;
}


bool GetWikipediaLink(const Record &record, std::string * const wikipedia_link) {
    wikipedia_link->clear();
    for (const auto &_670_field : record.getTagRange("670")) {
        const Subfields _670_subfields(_670_field.getSubfields());
        const std::string _670a_field(_670_subfields.getFirstSubfieldWithCode('a'));
        const std::string _670u_field(_670_subfields.getFirstSubfieldWithCode('u'));
        if (StringUtil::Contains(_670a_field, "Wikipedia")) {
            *wikipedia_link = _670u_field;
            return not wikipedia_link->empty();
        }
    }
    return false;
}


static inline bool CompareField(const Record::Field * const field1, const Record::Field * const field2) {
    if (field1->getTag() < field2->getTag())
        return true;
    if (field1->getTag() > field2->getTag())
        return false;
    return field1->getContents() < field2->getContents();
}


std::string CalcChecksum(const Record &record, const std::set<Tag> &excluded_fields, const bool suppress_local_fields) {
    std::vector<const Record::Field *> field_refs;
    field_refs.reserve(record.fields_.size());

    // Our Strategy here is it to sort references to fields based on tags and contents and then to create a blob using the
    // sorted order.  This allows us to generate checksums that are identical for non-equal but equivalent records.

    for (const auto &field : record.fields_) {
        if (excluded_fields.find(field.getTag()) == excluded_fields.cend() and (not suppress_local_fields or not field.getTag().isLocal()))
            field_refs.emplace_back(&field);
    }

    std::sort(field_refs.begin(), field_refs.end(), CompareField);

    std::string blob;
    blob.reserve(200000); // Roughly twice the maximum size of a single MARC-21 record.

    // Only include leader data that are parameterised
    // c.f https://www.loc.gov/marc/bibliographic/bdleader.html
    StringUtil::AppendSubstring(blob, record.leader_, 5, 12 - 5);
    StringUtil::AppendSubstring(blob, record.leader_, 17, 20 - 17);

    for (const auto &field_ref : field_refs)
        blob += field_ref->getTag().toString() + field_ref->getContents();

    return StringUtil::Sha1(blob);
}


bool UBTueIsAquisitionRecord(const Record &marc_record) {
    for (const auto &field : marc_record.getTagRange("LOK")) {
        const Subfields subfields(field.getSubfields());
        if (StringUtil::StartsWith(subfields.getFirstSubfieldWithCode('0'), "852") and subfields.getFirstSubfieldWithCode('m') == "e")
            return true;
    }

    return false;
}


const ThreadSafeRegexMatcher PARENT_PPN_MATCHER("^\\([^)]+\\)(.+)$");


std::string GetParentPPN(const Record &record) {
    static const std::vector<Tag> parent_reference_tags{ "800", "810", "830", "773", "776" };
    for (auto &field : record) {
        if (std::find_if(parent_reference_tags.cbegin(), parent_reference_tags.cend(),
                         [&field](const Tag &reference_tag){ return reference_tag == field.getTag(); }) == parent_reference_tags.cend())
            continue;

        auto matches(PARENT_PPN_MATCHER.match(field.getFirstSubfieldWithCode('w')));
        if (matches) {
            const std::string ppn_candidate(matches[1]);
            if (MiscUtil::IsValidPPN(ppn_candidate))
                return ppn_candidate;
        }
    }

    return "";
}


// See https://www.loc.gov/marc/bibliographic/ for how to construct this map:
static std::unordered_map<Tag, bool> tag_to_repeatable_map{
    { Tag("001"), false },
    { Tag("003"), false },
    { Tag("005"), false },
    { Tag("006"), true  },
    { Tag("007"), true  },
    { Tag("008"), false },
    { Tag("010"), false },
    { Tag("013"), true  },
    { Tag("015"), true  },
    { Tag("016"), true  },
    { Tag("017"), true  },
    { Tag("018"), false },
    { Tag("020"), true  },
    { Tag("022"), true  },
    { Tag("024"), true  },
    { Tag("025"), true  },
    { Tag("026"), true  },
    { Tag("027"), true  },
    { Tag("028"), true  },
    { Tag("030"), true  },
    { Tag("031"), true  },
    { Tag("032"), true  },
    { Tag("033"), true  },
    { Tag("034"), true  },
    { Tag("035"), true  },
    { Tag("036"), false },
    { Tag("037"), true  },
    { Tag("038"), false },
    { Tag("040"), false },
    { Tag("041"), true  },
    { Tag("042"), false },
    { Tag("043"), false },
    { Tag("044"), false },
    { Tag("045"), false },
    { Tag("046"), true  },
    { Tag("047"), true  },
    { Tag("048"), true  },
    { Tag("050"), true  },
    { Tag("051"), true  },
    { Tag("052"), true  },
    { Tag("055"), true  },
    { Tag("060"), true  },
    { Tag("061"), true  },
    { Tag("066"), false },
    { Tag("070"), true  },
    { Tag("071"), true  },
    { Tag("072"), true  },
    { Tag("074"), true  },
    { Tag("080"), true  },
    { Tag("082"), true  },
    { Tag("083"), true  },
    { Tag("084"), true  },
    { Tag("085"), true  },
    { Tag("086"), true  },
    { Tag("088"), true  },
    { Tag("100"), false },
    { Tag("110"), false },
    { Tag("111"), false },
    { Tag("130"), false },
    { Tag("186"), true  }, // non-standard field only used locally
    { Tag("210"), true  },
    { Tag("222"), true  },
    { Tag("240"), false },
    { Tag("242"), true  },
    { Tag("243"), false },
    { Tag("245"), false },
    { Tag("246"), true  },
    { Tag("247"), true  },
    { Tag("250"), true  },
    { Tag("254"), false },
    { Tag("255"), true  },
    { Tag("256"), false },
    { Tag("257"), true  },
    { Tag("258"), true  },
    { Tag("260"), true  },
    { Tag("263"), false },
    { Tag("264"), true  },
    { Tag("270"), true  },
    { Tag("300"), true  },
    { Tag("306"), false },
    { Tag("307"), true  },
    { Tag("310"), false },
    { Tag("321"), true  },
    { Tag("336"), true  },
    { Tag("337"), true  },
    { Tag("338"), true  },
    { Tag("340"), true  },
    { Tag("342"), true  },
    { Tag("343"), true  },
    { Tag("344"), true  },
    { Tag("345"), true  },
    { Tag("346"), true  },
    { Tag("347"), true  },
    { Tag("348"), true  },
    { Tag("351"), true  },
    { Tag("352"), true  },
    { Tag("355"), true  },
    { Tag("357"), false },
    { Tag("362"), true  },
    { Tag("363"), true  },
    { Tag("365"), true  },
    { Tag("366"), true  },
    { Tag("370"), true  },
    { Tag("377"), true  },
    { Tag("380"), true  },
    { Tag("381"), true  },
    { Tag("382"), true  },
    { Tag("383"), true  },
    { Tag("384"), false },
    { Tag("385"), true  },
    { Tag("386"), true  },
    { Tag("388"), true  },
    { Tag("490"), true  },
    { Tag("500"), true  },
    { Tag("501"), true  },
    { Tag("502"), true  },
    { Tag("504"), true  },
    { Tag("505"), true  },
    { Tag("506"), true  },
    { Tag("507"), true  },
    { Tag("508"), true  },
    { Tag("510"), true  },
    { Tag("511"), true  },
    { Tag("513"), true  },
    { Tag("514"), false },
    { Tag("515"), true  },
    { Tag("516"), true  },
    { Tag("518"), true  },
    { Tag("520"), true  },
    { Tag("521"), true  },
    { Tag("522"), true  },
    { Tag("524"), true  },
    { Tag("525"), true  },
    { Tag("526"), true  },
    { Tag("530"), true  },
    { Tag("533"), true  },
    { Tag("534"), true  },
    { Tag("535"), true  },
    { Tag("536"), true  },
    { Tag("538"), true  },
    { Tag("540"), true  },
    { Tag("541"), true  },
    { Tag("542"), true  },
    { Tag("545"), true  },
    { Tag("546"), true  },
    { Tag("547"), true  },
    { Tag("550"), true  },
    { Tag("552"), true  },
    { Tag("555"), true  },
    { Tag("556"), true  },
    { Tag("561"), true  },
    { Tag("562"), true  },
    { Tag("563"), true  },
    { Tag("565"), true  },
    { Tag("567"), true  },
    { Tag("580"), true  },
    { Tag("581"), true  },
    { Tag("583"), true  },
    { Tag("584"), true  },
    { Tag("585"), true  },
    { Tag("586"), true  },
    { Tag("588"), true  },
    { Tag("600"), true  },
    { Tag("601"), true  }, // non-standard field only used locally
    { Tag("610"), true  },
    { Tag("611"), true  },
    { Tag("630"), true  },
    { Tag("647"), true  },
    { Tag("648"), true  },
    { Tag("650"), true  },
    { Tag("651"), true  },
    { Tag("652"), false }, // non-standard field only used locally
    { Tag("653"), true  },
    { Tag("654"), true  },
    { Tag("655"), true  },
    { Tag("657"), true  },
    { Tag("658"), true  },
    { Tag("662"), true  },
    { Tag("700"), true  },
    { Tag("710"), true  },
    { Tag("711"), true  },
    { Tag("720"), true  },
    { Tag("730"), true  },
    { Tag("740"), true  },
    { Tag("750"), true  },
    { Tag("751"), true  },
    { Tag("752"), true  },
    { Tag("752"), true  },
    { Tag("754"), true  },
    { Tag("758"), true  },
    { Tag("760"), true  },
    { Tag("762"), true  },
    { Tag("765"), true  },
    { Tag("767"), true  },
    { Tag("770"), true  },
    { Tag("772"), true  },
    { Tag("773"), true  },
    { Tag("774"), true  },
    { Tag("775"), true  },
    { Tag("776"), true  },
    { Tag("777"), true  },
    { Tag("780"), true  },
    { Tag("785"), true  },
    { Tag("786"), true  },
    { Tag("787"), true  },
    { Tag("800"), true  },
    { Tag("810"), true  },
    { Tag("811"), true  },
    { Tag("830"), true  },
    { Tag("841"), false },
    { Tag("842"), false },
    { Tag("843"), true  },
    { Tag("844"), true  },
    { Tag("845"), true  },
    { Tag("850"), true  },
    { Tag("852"), true  },
    { Tag("853"), true  },
    { Tag("854"), true  },
    { Tag("855"), true  },
    { Tag("856"), true  },
    { Tag("863"), true  },
    { Tag("864"), true  },
    { Tag("865"), true  },
    { Tag("866"), true  },
    { Tag("867"), true  },
    { Tag("868"), true  },
    { Tag("876"), true  },
    { Tag("877"), true  },
    { Tag("878"), true  },
    { Tag("880"), true  },
    { Tag("882"), true  },
    { Tag("883"), true  },
    { Tag("884"), true  },
    { Tag("885"), true  },
    { Tag("886"), true  },
    { Tag("887"), true  },
};


bool IsRepeatableField(const Tag &tag) {
    // 1. Handle all local fields.
    if (tag.toString().find('9') != std::string::npos or StringUtil::IsAsciiLetter(tag.toString()[0]))
        return true;

    // 2. Handle all other fields.
    const auto tag_and_repeatable(tag_to_repeatable_map.find(tag));
    if (unlikely(tag_and_repeatable == tag_to_repeatable_map.end()))
        LOG_ERROR(tag.toString() + " is not in our map!");
    return tag_and_repeatable->second;
}


bool IsStandardTag(const Tag &tag) {
    // 1. Handle all local fields.
    if (tag.toString().find('9') != std::string::npos or StringUtil::IsAsciiLetter(tag.toString()[0]))
        return false;

    // 2. Handle all other fields.
    return tag_to_repeatable_map.find(tag) != tag_to_repeatable_map.end();
}


bool UBTueIsElectronicResource(const Record &marc_record) {
    if (std::toupper(marc_record.leader_[6]) == 'M')
        return true;

    if (marc_record.isMonograph()) {
        for (const auto &_007_field : marc_record.getTagRange("007")) {
            const std::string _007_field_contents(_007_field.getContents());
            if (not _007_field_contents.empty() and std::toupper(_007_field_contents[0]) == 'C')
                return true;
        }
    }

    for (const auto &_245_field : marc_record.getTagRange("245")) {
        for (const auto &subfield : _245_field.getSubfields()) {
            if (subfield.code_ == 'h' and subfield.value_.find("[Elektronische Ressource]") != std::string::npos)
                return true;
        }
    }

    return false;
}


bool IsOpenAccess(const Record &marc_record) {
    for (const auto &_856_field : marc_record.getTagRange("856")) {
        const Subfields subfields(_856_field.getSubfields());
        const std::string subfield_z_contents(subfields.getFirstSubfieldWithCode('z'));
        if (subfield_z_contents == "LF")
            return true;
        if (StringUtil::StartsWith(TextUtil::UTF8ToLower(subfield_z_contents), "kostenfrei")) {
            const std::string subfield_3_contents(TextUtil::UTF8ToLower(subfields.getFirstSubfieldWithCode('3')));
            if (subfield_3_contents.empty() or subfield_3_contents == "volltext")
                return true;
        }

        for (const auto &subfield : subfields) {
            if (subfield.code_ == 'x' and TextUtil::UTF8ToLower(subfield.value_) == "unpaywall")
                return true;
        }
    }

    for (const auto &_655_field : marc_record.getTagRange("655")) {
        if (TextUtil::UTF8ToLower(_655_field.getFirstSubfieldWithCode('a')) == "open access")
            return true;
    }

    return false;
}


size_t CollectRecordOffsets(MARC::Reader * const marc_reader, std::unordered_map<std::string, off_t> * const control_number_to_offset_map) {
    off_t last_offset(marc_reader->tell());
    while (const MARC::Record record = marc_reader->read()) {
        (*control_number_to_offset_map)[record.getControlNumber()] = last_offset;
        last_offset = marc_reader->tell();
    }

    return control_number_to_offset_map->size();
}


FileType GetOptionalReaderType(int * const argc, char *** const argv, const int arg_no, const FileType default_file_type) {
    FileType return_value(default_file_type);

    if (StringUtil::StartsWith((*argv)[arg_no], "--input-format=")) {
        const std::string format((*argv)[arg_no] + __builtin_strlen("--input-format="));
        if (format == "marc-21")
            return_value = FileType::BINARY;
        else if (format == "marc-xml")
            return_value = FileType::XML;
        else
            LOG_ERROR("bad MARC input format: \"" + format + "\"!");

        --*argc, ++*argv;
    }

    return return_value;
}


FileType GetOptionalWriterType(int * const argc, char *** const argv, const int arg_no, const FileType default_file_type) {
    FileType return_value(default_file_type);

    if (StringUtil::StartsWith((*argv)[arg_no], "--output-format=")) {
        const std::string format((*argv)[arg_no] + __builtin_strlen("--output-format="));
        if (format == "marc-21")
            return_value = FileType::BINARY;
        else if (format == "marc-xml")
            return_value = FileType::XML;
        else
            LOG_ERROR("bad MARC output format: \"" + format + "\"!");

        --*argc, ++*argv;
    }

    return return_value;
}


bool Record::isReviewArticle() const {
    for (const auto &_655_field : getTagRange("655")) {
        if (StringUtil::FindCaseInsensitive(_655_field.getFirstSubfieldWithCode('a'), "rezension") != std::string::npos)
            return true;
    }

    for (const auto &_935_field : getTagRange("935")) {
        if (_935_field.getFirstSubfieldWithCode('c') == "uwre")
            return true;
    }

    return false;
}


bool Record::isPossiblyReviewArticle() const {
    if (isReviewArticle())
        return true;

    return StringUtil::FindCaseInsensitive(getMainTitle(), "review") != std::string::npos
           or StringUtil::FindCaseInsensitive(getMainTitle(), "rezension") != std::string::npos;
}


const std::vector<Tag> CROSS_LINK_FIELDS{ Tag("775"), Tag("776"), Tag("780"), Tag("785") };


bool IsCrossLinkField (const MARC::Record::Field &field, std::string * const partner_control_number, const std::vector<MARC::Tag> &cross_link_fields) {
    if (not field.hasSubfield('w')
        or std::find(cross_link_fields.cbegin(), cross_link_fields.cend(), field.getTag().toString()) == cross_link_fields.cend())
        return false;

    const MARC::Subfields subfields(field.getSubfields());
    for (const auto &w_subfield : subfields.extractSubfields('w')) {
        if (StringUtil::StartsWith(w_subfield, "(DE-627)")) {
            *partner_control_number = w_subfield.substr(__builtin_strlen("(DE-627)"));
            return true;
        }
    }

    return false;
}


std::set<std::string> ExtractCrossReferencePPNs(const MARC::Record &record) {
    std::set<std::string> partner_ppns;
    for (const auto &field : record) {
        if (std::find(CROSS_LINK_FIELDS.cbegin(), CROSS_LINK_FIELDS.cend(), field.getTag()) == CROSS_LINK_FIELDS.cend())
            continue;

        std::string partner_control_number;
        if (IsCrossLinkField(field, &partner_control_number))
            partner_ppns.emplace(partner_control_number);
    }

    return partner_ppns;
}


static std::unordered_map<std::string, Record::Field> LoadTermsToFieldsMap() {
    std::unordered_map<std::string, Record::Field> terms_to_fields_map;

    const auto MAP_FILENAME(UBTools::GetTuelibPath() + "tags_and_keyword_fields.map");
    const auto map_file(FileUtil::OpenInputFileOrDie(MAP_FILENAME));
    unsigned line_no(0);
    while (not map_file->eof()) {
        std::string line;
        map_file->getline(&line);
        ++line_no;
        StringUtil::TrimWhite(&line);
        if (line.length() < 4)
            LOG_ERROR("bad entry on line #" + std::to_string(line_no) + " in \"" + MAP_FILENAME + "\"!");

        const Record::Field field(line.substr(0, Record::TAG_LENGTH), StringUtil::CStyleUnescape(line.substr(Record::TAG_LENGTH)));
        terms_to_fields_map.emplace(field.getFirstSubfieldWithCode('a'), field);
    }

    return terms_to_fields_map;
}


Record::Field GetIndexField(const std::string &index_term) {
    static const std::unordered_map<std::string, Record::Field> TERMS_TO_FIELDS_MAP(LoadTermsToFieldsMap());
    static const Tag DEFAULT_TAG("650");
    const auto term_and_field(TERMS_TO_FIELDS_MAP.find(TextUtil::UTF8ToLower(index_term)));
    if (term_and_field == TERMS_TO_FIELDS_MAP.cend())
        return Record::Field(Tag(DEFAULT_TAG), { { { 'a', index_term } } }, /* indicator1 = */' ', /* indicator2 = */'4');
    return term_and_field->second;
}


// See https://www.loc.gov/marc/bibliographic/bd6xx.html to understand our implementation.
bool IsSubjectAccessTag(const Tag &tag) {
    return tag.toString()[0] == '6';
}


std::set<std::string> ExtractOnlineCrossLinkPPNs(const MARC::Record &record) {
    std::set<std::string> cross_reference_ppns;
    for (const auto &_776_field : record.getTagRange("776")) {
        const auto ppn(BSZUtil::GetK10PlusPPNFromSubfield(_776_field, 'w'));
        if (ppn.empty())
            continue;

        const auto subfield_i_contents(_776_field.getFirstSubfieldWithCode('i'));
        if (StringUtil::StartsWith(subfield_i_contents, "Online") or subfield_i_contents == "Elektronische Reproduktion") {
            cross_reference_ppns.emplace(ppn);
            continue;
        }

        const auto subfield_n_contents(_776_field.getFirstSubfieldWithCode('n'));
        if (StringUtil::StartsWith(subfield_n_contents, "Online") ) {
            cross_reference_ppns.emplace(ppn);
            continue;
        }
    }

    return cross_reference_ppns;
}


std::set<std::string> ExtractPrintCrossLinkPPNs(const MARC::Record &record) {
    std::set<std::string> cross_reference_ppns;
    for (const auto &_776_field : record.getTagRange("776")) {
        const auto ppn(BSZUtil::GetK10PlusPPNFromSubfield(_776_field, 'w'));
        if (ppn.empty())
            continue;

        const auto subfield_i_contents(_776_field.getFirstSubfieldWithCode('i'));
        if (StringUtil::StartsWith(subfield_i_contents, "Druckausg") or subfield_i_contents == "Elektronische Reproduktion von") {
            cross_reference_ppns.emplace(ppn);
            continue;
        }

        const auto subfield_n_contents(_776_field.getFirstSubfieldWithCode('n'));
        if (StringUtil::StartsWith(subfield_n_contents, "Druck")) {
            cross_reference_ppns.emplace(ppn);
            continue;
        }
    }

    return cross_reference_ppns;
}


static void ExtractOtherCrossLinkPPNsHelper(const MARC::Record &record, const MARC::Tag &tag,
                                            std::set<std::string> * const cross_link_ppns)
{
    for (const auto &cross_link_field : record.getTagRange(tag)) {
        const auto cross_link_ppn(BSZUtil::GetK10PlusPPNFromSubfield(cross_link_field, 'w'));
        if (not cross_link_ppn.empty()) {
            cross_link_ppns->emplace(cross_link_ppn);
            return;
        }
    }
}


std::set<std::string> ExtractOtherCrossLinkPPNs(const MARC::Record &record) {
    std::set<std::string> cross_link_ppns;

    ExtractOtherCrossLinkPPNsHelper(record, "780", &cross_link_ppns);
    ExtractOtherCrossLinkPPNsHelper(record, "785", &cross_link_ppns);

    return cross_link_ppns;
}


std::set<std::string> ExtractCrossLinkPPNs(const MARC::Record &record) {
    auto cross_link_ppns(ExtractOnlineCrossLinkPPNs(record));
    const auto print_cross_link_ppns(ExtractPrintCrossLinkPPNs(record));
    cross_link_ppns.insert(print_cross_link_ppns.cbegin(), print_cross_link_ppns.cend());
    const auto other_cross_link_ppns(ExtractOtherCrossLinkPPNs(record));
    cross_link_ppns.insert(other_cross_link_ppns.cbegin(), other_cross_link_ppns.cend());

    return cross_link_ppns;
}


const std::unordered_map<std::string, std::string> UNKNOWN_CODE_TO_MARC_CODE{
    { "zz",  ""    }, // Unknown or unspecified country
    { "eng", "eng" },
    { "en",  "eng" },
    { "fre", "fre" },
    { "fr",  "fre" },
    { "por", "por" },
    { "pt",  "por" },
    { "ger", "ger" },
    { "de",  "ger" },
    { "ita", "ita" },
    { "it",  "ita" },
    { "dut", "dut" },
    { "nl",  "dut" },
    { "fin", "fin" },
    { "fi",  "fin" },
    { "spa", "spa" },
    { "es",  "spa" },
    { "lit", "lit" },
    { "lt",  "lit" },
    { "ind", "ind" },
    { "id",  "ind" },
    { "grc", "grc" },
    { "el",  "grc" },
    { "hun", "hun" },
    { "hu",  "hun" },
    { "hrv", "hrv" },
    { "hr",  "hrv" },
    { "yor", "yor" },
    { "yo",  "yor" },
    { "tai", "tai" },
    { "th",  "tai" },
    { "rus", "rus" },
    { "ru",  "rus" },
    { "cat", "cat" },
    { "ca",  "cat" },
    { "swe", "swe" },
    { "sv",  "swe" },
    { "slv", "slv" },
    { "sl",  "slv" },
    { "ukr", "ukr" },
    { "uk",  "ukr" },
    { "epo", "epo" },
    { "eo",  "epo" },
    { "dan", "dan" },
    { "da",  "dan" },
    { "mac", "mac" },
    { "mk",  "mac" },
    { "slo", "slo" },
    { "sk",  "slo" },
    { "est", "est" },
    { "et",  "est" },
    { "wel", "wel" },
    { "cy",  "wel" },
    { "pol", "pol" },
    { "pl",  "pol" },
    { "nor", "nor" },
    { "no",  "nor" },
    { "bos", "bos" },
    { "bs",  "bos" },
    { "ara", "ara" },
    { "ar",  "ara" },
    { "tur", "tur" },
    { "tr",  "tur" },
    { "bul", "bul" },
    { "bg",  "bul" },
    { "rum", "rum" },
    { "ro",  "rum" },
    { "nob", "nob" },
    { "nb",  "nob" },
    { "jpn", "jpn" },
    { "ja",  "jpn" },
    { "cze", "cze" },
    { "cs",  "cze" },
    { "baq", "baq" },
    { "eu",  "baq" },
};


std::string MapToMARCLanguageCode(const std::string &some_code) {
    const auto unknown_code_and_marc_code(UNKNOWN_CODE_TO_MARC_CODE.find(StringUtil::ASCIIToLower(some_code)));
    if (unknown_code_and_marc_code != UNKNOWN_CODE_TO_MARC_CODE.cend())
        return unknown_code_and_marc_code->second;
    return "";
}


} // namespace MARC
