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
#pragma once


#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <arpa/inet.h>
#include "Compiler.h"
#include "File.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "XMLSubsetParser.h"


// Forward declaration:
class RegexMatcher;


namespace MARC {


// These tags are for fields that may contain w-subfields with cross or uplink PPNs following "(DE-627)".
// Important:  You *must* keep a alphanumerically increasing order of these tags!
const std::set<std::string> CROSS_LINK_FIELD_TAGS{ "689", "700", "770", "772", "773", "775", "776", "780",
                                                   "785", "787", "800", "810", "811", "830", "880", "889" };


class Tag {
    /* We have to double this up, so we have one little endian integer for comparison, and one big endian integer
     * containing a char[4] for printing.
     */
    union {
        uint32_t as_int_;
        char as_cstring_[4];
    } tag_;
public:
    inline Tag() { tag_.as_int_ = 0; }
    inline Tag(const char raw_tag[4]) {
        if (unlikely(std::strlen(raw_tag) != 3))
            throw std::runtime_error("in Tag: \"raw_tag\" must have a length of 3: \"" + std::string(raw_tag) + "\"! (1)");
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    inline Tag(const std::string &raw_tag) {
        if (unlikely(raw_tag.length() != 3))
            throw std::runtime_error("in Tag: \"raw_tag\" must have a length of 3: \"" + raw_tag + "\"! (2)");
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    /** Copy constructor. */
    Tag(const Tag &other_tag): tag_(other_tag.tag_) { }

    Tag &operator=(const Tag &rhs) { tag_ = rhs.tag_; return *this; }
    bool operator==(const Tag &rhs) const { return to_int() == rhs.to_int(); }
    bool operator!=(const Tag &rhs) const { return to_int() != rhs.to_int(); }
    bool operator>(const Tag &rhs) const  { return to_int() >  rhs.to_int(); }
    bool operator>=(const Tag &rhs) const { return to_int() >= rhs.to_int(); }
    bool operator<(const Tag &rhs) const  { return to_int() <  rhs.to_int(); }
    bool operator<=(const Tag &rhs) const { return to_int() <= rhs.to_int(); }

    bool operator==(const std::string &rhs) const { return ::strcmp(c_str(), rhs.c_str()) == 0; }
    bool operator==(const char rhs[4]) const { return ::strcmp(c_str(), rhs) == 0; }

    std::ostream& operator<<(std::ostream& os) const { return os << toString(); }
    friend std::ostream &operator<<(std::ostream &output,  const Tag &tag) { return output << tag.toString(); }

    inline const char *c_str() const { return tag_.as_cstring_; }
    inline const std::string toString() const { return std::string(c_str(), 3); }
    inline uint32_t to_int() const { return htonl(tag_.as_int_); }

    inline bool isTagOfControlField() const { return tag_.as_cstring_[0] == '0' and tag_.as_cstring_[1] == '0'; }
    bool isLocal() const;
    Tag &swap(Tag &other);
};


bool IsRepeatableField(const Tag &tag);
bool IsStandardTag(const Tag &tag);


} // namespace MARC


namespace std {
    template <>
    struct hash<MARC::Tag> {
        size_t operator()(const MARC::Tag &m) const {
            // hash method here.
            return hash<int>()(m.to_int());
        }
    };
} // namespace std


namespace MARC {


struct Subfield {
    char code_;
    std::string value_;
public:
    Subfield(const char code, const std::string &value): code_(code), value_(value) { }
    Subfield(const Subfield &other) = default;

    Subfield &operator=(const Subfield &rhs) = default;
    inline bool empty() const { return value_.empty(); }

    inline std::string toString() const {
        std::string as_string;
        as_string += '\x1F';
        as_string += code_;
        as_string += value_;
        return as_string;
    }
};


class Subfields {
    std::vector<Subfield> subfields_;
public:
    typedef std::vector<Subfield>::const_iterator const_iterator;
    typedef std::vector<Subfield>::iterator iterator;
public:
    Subfields() = default;
    inline Subfields(std::vector<Subfield> &&subfields): subfields_(subfields) { }
    Subfields(const Subfields &other) = default;
    explicit Subfields(const std::string &field_contents);
    Subfields(Subfields &&other) = default;

    inline const_iterator begin() const { return subfields_.cbegin(); }
    inline const_iterator end() const { return subfields_.cend(); }
    inline iterator begin() { return subfields_.begin(); }
    inline iterator end() { return subfields_.end(); }
    inline bool empty() const { return subfields_.empty(); }
    inline size_t size() const { return subfields_.size(); }
    inline void reserve(const size_t size) { subfields_.reserve(size); }
    void clear() { return subfields_.clear(); }

    inline bool hasSubfield(const char subfield_code) const {
        return std::find_if(subfields_.cbegin(), subfields_.cend(),
                            [subfield_code](const Subfield subfield) -> bool
                                { return subfield.code_ == subfield_code; }) != subfields_.cend();
    }

    inline bool hasSubfieldWithValue(const char subfield_code, const std::string &value, const bool case_insensitive = false) const {
        return std::find_if(subfields_.cbegin(), subfields_.cend(),
                            [subfield_code, value, case_insensitive](const Subfield subfield) -> bool
                                { return subfield.code_ == subfield_code and
                                  (case_insensitive ? StringUtil::ASCIIToUpper(subfield.value_) == StringUtil::ASCIIToUpper(value) :
                                                      subfield.value_ == value);
                                }) != subfields_.cend();
    }

    // Inserts the new subfield after all leading subfields that have a code that preceeds "subfield_code" when using an
    // alphanumeric comparison.
    void addSubfield(const char subfield_code, const std::string &subfield_value);

    // Appends the new subfield at the end of any existing subfields.
    inline void appendSubfield(const char subfield_code, const std::string &subfield_value)
        { subfields_.emplace_back(subfield_code, subfield_value); }

    inline void appendSubfield(const Subfield &subfield)
        { subfields_.emplace_back(subfield.code_, subfield.value_); }

    /** \brief Replaces the contents of the first subfield w/ the specified subfield code.
     *  \return True if we replaced the subfield contents and false if a subfield w/ the given code was not found.
     */
    bool replaceFirstSubfield(const char subfield_code, const std::string &new_subfield_value);

    /** \brief Replaces the contents of all subfields w/ the specified subfield code and given content.
     *  \return True if we replaced the contents of at least one subfield.
     */
    bool replaceAllSubfields(const char subfield_code, const std::string &old_subfield_value,
                             const std::string &new_subfield_value);

    /** \brief Extracts all values from subfields with codes in the "list" of codes in "subfield_codes".
     *  \return The values of the subfields with matching codes.
     */
    inline std::vector<std::string> extractSubfields(const std::string &subfield_codes) const {
        std::vector<std::string> extracted_values;
        for (const auto &subfield : subfields_) {
            if (subfield_codes.find(subfield.code_) != std::string::npos)
                extracted_values.emplace_back(subfield.value_);
        }
        return extracted_values;
    }

   /** \brief Extracts all values from subfields with codes in the "list" of codes in "subfield_codes".
     * \note In the case of numeric subfield codes the following character in "subfield_codes" will also be considered and
     *       only subfield values starting with this character followed by a colon will be extracted (w/o the character and
     *       colon).
     *  \example "abcd9v" => Subfield values for subfields with codes "abcd9" will be extracted with the proviso that only
     *           those values of subfields with code '9' will be extracted that start w/ "v:" and those will be returned w/o the
     *           leading "v:".
     *  \return The values of the subfields with matching codes.
     */
    std::vector<std::string> extractSubfieldsAndNumericSubfields(const std::string &subfield_spec) const;

    /** \return Either the contents of the subfield or the empty string if no corresponding subfield was found. */
    inline std::string getFirstSubfieldWithCode(const char subfield_code) const {
        const auto iter(std::find_if(subfields_.cbegin(), subfields_.cend(),
                                     [subfield_code](const Subfield subfield) -> bool
                                         { return subfield.code_ == subfield_code; }));
        return (iter == subfields_.cend()) ? "" : iter->value_;
    }

    /** \brief Extracts all values from subfields with a matching subfield code.
     *  \return The values of the subfields with matching codes.
     */
    inline std::vector<std::string> extractSubfields(const char subfield_code) const {
        std::vector<std::string> extracted_values;
        for (const auto &subfield : subfields_) {
            if (subfield_code == subfield.code_)
                extracted_values.emplace_back(subfield.value_);
        }
        return extracted_values;
    }

    inline void deleteFirstSubfieldWithCode(const char subfield_code) {
        auto location(std::find_if(subfields_.begin(), subfields_.end(),
                                   [subfield_code](const Subfield subfield) -> bool
                                       { return subfield.code_ == subfield_code; }));
        if (location != subfields_.end())
            subfields_.erase(location);
    }

    inline void deleteAllSubfieldsWithCode(const char subfield_code) {
        subfields_.erase(std::remove_if(subfields_.begin(), subfields_.end(),
                         [subfield_code](const Subfield subfield) -> bool
                         { return subfield.code_ == subfield_code; }), subfields_.end());
    }


    inline void deleteAllSubfieldsWithCodeMatching(const char subfield_code, const ThreadSafeRegexMatcher &regex) {
        subfields_.erase(std::remove_if(subfields_.begin(), subfields_.end(),
                         [subfield_code, regex](const Subfield subfield) -> bool
                         { return (subfield.code_ == subfield_code) and regex.match(subfield.value_); }), subfields_.end());
    }

    inline bool replaceSubfieldCode(const char old_code, const char new_code) {
        bool replaced_at_least_one_code(false);
        for (auto &subfield : *this) {
            if (subfield.code_ == old_code) {
                subfield.code_ = new_code;
                replaced_at_least_one_code = true;
            }
        }
        return replaced_at_least_one_code;
    }

    /** \brief Moves the contents from the subfield with subfield code "from_subfield_code" to the
     *         subfield with the subfield code "to_subfield_code".
     *  \note  Pre-existing entries w/ subfield code "to_subfield_code" will be deleted.
     */
    inline void moveSubfield(const char from_subfield_code, const char to_subfield_code) {
        deleteAllSubfieldsWithCode(to_subfield_code);
        replaceSubfieldCode(from_subfield_code, to_subfield_code);
    }

    inline std::string toString() const {
        std::string as_string;
        for (const auto &subfield : subfields_) {
            if (likely(not subfield.empty()))
                as_string += subfield.toString();
        }
        return as_string;
    }
};


enum EditInstructionType { INSERT_FIELD, INSERT_SUBFIELD, ADD_SUBFIELD };


struct EditInstruction {
    EditInstructionType type_;
    Tag tag_;
    char subfield_code_;
    std::string field_or_subfield_contents_;
    char indicator1_, indicator2_;
public:
    inline static EditInstruction CreateInsertFieldInstruction(const Tag &tag, const std::string &field_contents,
                                                               const char indicator1 = ' ', const char indicator2 = ' ')
        { return EditInstruction(INSERT_FIELD, tag, '\0', field_contents, indicator1, indicator2); }
    inline static EditInstruction CreateInsertSubfieldInstruction(const Tag &tag, const char subfield_code,
                                                                  const std::string &subfield_contents, const char indicator1 = ' ',
                                                                  const char indicator2 = ' ')
        { return EditInstruction(INSERT_SUBFIELD, tag, subfield_code, subfield_contents, indicator1, indicator2); }
    inline static EditInstruction CreateAddSubfieldInstruction(const Tag &tag, const char subfield_code,
                                                               const std::string &subfield_contents)
        { return EditInstruction(ADD_SUBFIELD, tag, subfield_code, subfield_contents, '\0', '\0'); }
private:
    EditInstruction(const EditInstructionType type, const Tag &tag, const char subfield_code, const std::string &field_or_subfield_contents,
                    const char indicator1, const char indicator2)
        : type_(type), tag_(tag), subfield_code_(subfield_code), field_or_subfield_contents_(field_or_subfield_contents),
          indicator1_(indicator1), indicator2_(indicator2) { }
};


class Record {
public:
    class Field {
        friend class Record;
        Tag tag_;
        std::string contents_;
    public:
        Field(const Field &other): tag_(other.tag_), contents_(other.contents_) { }
        Field(const std::string &tag, const std::string &contents): tag_(tag), contents_(contents) { }
        Field(const std::string &tag, const char indicator1 = ' ', const char indicator2 = ' ')
            : tag_(tag), contents_(std::string(1, indicator1) + std::string(1, indicator2)) { }
        Field(const Tag &tag, const std::string &contents): tag_(tag), contents_(contents) { }
        Field(const Tag &tag, const char indicator1 = ' ', const char indicator2 = ' ')
            : Field(tag, std::string(1, indicator1) + std::string(1, indicator2)) { }
        Field(const Tag &tag, const Subfields &subfields, const char indicator1 = ' ', const char indicator2 = ' ')
            : Field(tag, std::string(1, indicator1) + std::string(1, indicator2) + subfields.toString()) { }

        inline bool empty() const
            { return isControlField() ? contents_.empty() : contents_.size() == 2 /* indicators */; }
        Field &operator=(const Field &rhs) = default;
        inline bool operator==(const Field &rhs) const { return tag_ == rhs.tag_ and contents_ == rhs.contents_; }
        inline bool operator!=(const Field &rhs) const { return not operator==(rhs); }
        bool operator<(const Field &rhs) const;
        inline const Tag &getTag() const { return tag_; }
        inline void setTag(const Tag &new_tag) { tag_ = new_tag; }
        inline const std::string &getContents() const { return contents_; }
        inline std::string getContents() { return contents_; }
        inline void setContents(const std::string &new_field_contents) { contents_ = new_field_contents; }
        inline void setContents(const Subfields &subfields, const char indicator1 = ' ', const char indicator2 = ' ') {
            setContents(std::string(1, indicator1) + std::string(1, indicator2) + subfields.toString());
        }
        inline bool isControlField() const __attribute__ ((pure)) { return tag_ <= "009"; }
        inline bool isDataField() const __attribute__ ((pure)) { return tag_ > "009"; }
        inline bool isRepeatableField() const { return MARC::IsRepeatableField(tag_); };
        inline bool isCrossLinkField() const
            { return CROSS_LINK_FIELD_TAGS.find(tag_.toString()) != CROSS_LINK_FIELD_TAGS.cend(); }
        inline char getIndicator1() const { return unlikely(contents_.empty()) ? '\0' : contents_[0]; }
        inline char getIndicator2() const { return unlikely(contents_.size() < 2) ? '\0' : contents_[1]; }
        inline void setIndicator1(const char new_indicator1) { if (likely(not contents_.empty())) contents_[0] = new_indicator1; }
        inline void setIndicator2(const char new_indicator2) { if (likely(not contents_.empty())) contents_[1] = new_indicator2; }
        inline Subfields getSubfields() const { return Subfields(contents_); }
        inline void setSubfields(const Subfields &subfields) {
            setContents(subfields, getIndicator1(), getIndicator2());
        }
        inline bool isLocal() const { return tag_.isLocal(); }
        Tag getLocalTag() const;

        /** \warning Do not call the following two functions on local control fields! */
        inline char getLocalIndicator1() const { return contents_[2 /*indicators*/ + 2/*delimiter and subfield code*/ + 3 /*pseudo tag*/]; }
        inline char getLocalIndicator2() const {
            return contents_[2 /*indicators*/ + 2/*delimiter and subfield code*/ + 3 /*pseudo tag*/] + 1;
        }

        /** \brief  Filter out all subfields that do not have subfield codes found in "codes_to_keep".
         *  \return True if at least one subfield has been removed, o/w false.
         */
        bool filterSubfields(const std::string &codes_to_keep);

        /** \return Either the contents of the subfield or the empty string if no corresponding subfield was found. */
        std::string getFirstSubfieldWithCode(const char subfield_code) const;

        /** \return Either the contents of the subfield with prefix or the empty string if no corresponding subfield was found. */
        std::string getFirstSubfieldWithCodeAndPrefix(const char subfield_code, const std::string prefix) const;

        bool hasSubfield(const char subfield_code) const;
        bool hasSubfieldWithValue(const char subfield_code, const std::string &value, const bool case_insensitive = false) const;

        /** \param value  Where to store the extracted data, if we have a match.
         *  \return True, if a subfield with subfield code "subfield_code" matching "regex" exists, else false.
         */
        bool extractSubfieldWithPattern(const char subfield_code, RegexMatcher &regex, std::string * const value) const;

        bool removeSubfieldWithPattern(const char subfield_code, const ThreadSafeRegexMatcher &regex);

        inline void appendSubfield(const char subfield_code, const std::string &subfield_value)
            { contents_ += std::string(1, '\x1F') + std::string(1, subfield_code) + subfield_value; }

        void insertOrReplaceSubfield(const char subfield_code, const std::string &subfield_contents);

        /** \return True if one or more subfield codes were replaces, else false. */
        bool replaceSubfieldCode(const char old_code, const char new_code);

        std::string toString() const { return tag_.toString() + contents_; }

        /** \note Do *not* call this on control fields! */
        void deleteAllSubfieldsWithCode(const char subfield_code);

        std::string getHash() const { return StringUtil::Sha1(toString()); }

        inline void swap(Field &other) { tag_.swap(other.tag_); contents_.swap(other.contents_); }
    };

    enum class RecordType { AUTHORITY, UNKNOWN, BIBLIOGRAPHIC, CLASSIFICATION };
    enum class TypeOfRecord {
        LANGUAGE_MATERIAL, NOTATED_MUSIC, MANUSCRIPT_NOTATED_MUSIC, CARTOGRAPHIC_MATERIAL, MANUSCRIPT_CARTOGRAPHIC_MATERIAL,
        PROJECTED_MEDIUM, NONMUSICAL_SOUND_RECORDING, MUSICAL_SOUND_RECORDING, TWO_DIMENSIONAL_NONPROJECTABLE_GRAPHIC,
        COMPUTER_FILE, KIT, MIXED_MATERIALS, THREE_DIMENSIONAL_ARTIFACT_OR_NATURALLY_OCCURRING_OBJECT,
        MANUSCRIPT_LANGUAGE_MATERIAL, AUTHORITY
    };
    enum BibliographicLevel {
        MONOGRAPHIC_COMPONENT_PART, SERIAL_COMPONENT_PART, COLLECTION, SUBUNIT, INTEGRATING_RESOURCE, MONOGRAPH_OR_ITEM, SERIAL, UNDEFINED
    };
    typedef std::vector<Field>::iterator iterator;
    typedef std::vector<Field>::const_iterator const_iterator;

    /** \brief Represents a range of fields.
     *  \note  Returning this from a Record member function allows for a for-each loop.
     */
    class ConstantRange {
        const_iterator begin_;
        const_iterator end_;
    public:
        inline ConstantRange(const_iterator begin, const_iterator end): begin_(begin), end_(end) { }
        inline size_t size() const { return end_ - begin_; }
        inline const_iterator begin() const { return begin_; }
        inline const_iterator end() const { return end_; }
        inline bool empty() const { return begin_ == end_; }

        // \warning Only call the following on non-empty ranges!
        inline const Field &front() const { return *begin_; }
        inline const Field &back() const { return *(end_ - 1); }
    };

    /** \brief Represents a range of fields.
     *  \note  Returning this from a Record member function allows for a for-each loop.
     */
    class Range {
        iterator begin_;
        iterator end_;
    public:
        inline Range(iterator begin, iterator end): begin_(begin), end_(end) { }
        inline size_t size() const { return end_ - begin_; }
        inline iterator begin() const { return begin_; }
        inline iterator end() const { return end_; }
        inline bool empty() const { return begin_ == end_; }

        // \warning Only call the following on non-empty ranges!
        inline Field &front() { return *begin_; }
        inline Field &back() { return *(end_ - 1); }
    };

    class KeywordAndSynonyms {
        Tag tag_;
        std::string keyword_;
        std::vector<std::string> synonyms_;
    public:
        KeywordAndSynonyms() = default;
        KeywordAndSynonyms(const KeywordAndSynonyms &other) = default;
        KeywordAndSynonyms(const Tag &tag, const std::string &keyword, const std::vector<std::string> &synonyms)
            : tag_(tag), keyword_(keyword), synonyms_(synonyms) { }

        KeywordAndSynonyms &operator=(const KeywordAndSynonyms &rhs) = default;
        inline const Tag &getTag() const { return tag_; }
        inline const std::string &getKeyword() const { return keyword_; }
        inline std::vector<std::string>::const_iterator begin() const { return synonyms_.begin(); }
        inline std::vector<std::string>::const_iterator end() const { return synonyms_.end(); }
        KeywordAndSynonyms &swap(KeywordAndSynonyms &other);
    };
private:
    friend class BinaryReader;
    friend class XmlReader;
    friend class BinaryWriter;
    friend class XmlWriter;
    friend std::string CalcChecksum(const Record &record, const std::set<Tag> &excluded_fields, const bool suppress_local_fields);
    friend bool UBTueIsElectronicResource(const Record &marc_record);
    size_t record_size_; // in bytes
    std::string leader_;
    std::vector<Field> fields_;
public:
    static constexpr unsigned MAX_RECORD_LENGTH                        = 99999;
    static constexpr unsigned MAX_VARIABLE_FIELD_DATA_LENGTH           = 9998; // Max length without trailing terminator
    static constexpr unsigned DIRECTORY_ENTRY_LENGTH                   = 12;
    static constexpr unsigned RECORD_LENGTH_FIELD_LENGTH               = 5;
    static constexpr unsigned TAG_LENGTH                               = 3;
    static constexpr unsigned LEADER_LENGTH                            = 24;
private:
    Record(): record_size_(LEADER_LENGTH + 1 /* end-of-directory */ + 1 /* end-of-record */) { }
public:
    explicit Record(const std::string &leader); // Make an empty record that only has a leader and sets the record size to
                                                // LEADER_LENGTH + 1 /* end-of-directory */ + 1 /* end-of-record */
    explicit Record(const size_t record_size, const char * const record_start);
    Record(const TypeOfRecord type_of_record, const BibliographicLevel bibliographic_level,
           const std::string &control_number = "");
    Record(const Record &other) = default;

    inline Record(Record &&other) { this->swap(other); }

    // Copy-assignment operator.
    Record &operator=(const Record &rhs) = default;

    inline bool empty() const { return fields_.empty(); }

    inline void swap(Record &other) {
        std::swap(record_size_, other.record_size_);
        leader_.swap(other.leader_);
        fields_.swap(other.fields_);
    }
    operator bool () const { return not fields_.empty(); }
    inline size_t size() const { return record_size_; }
    inline void clear() {
        record_size_  = 0;
        leader_.clear();
        fields_.clear();
    }

    enum class RecordFormat { MARC21_BINARY, MARC_XML };

    /** \brief Creates a string representation of a MARC record.
     *  \note  "indent_amount" and "text_conversion_type" are only used if "record_format" == MARC_XML.
     */
    std::string toString(const RecordFormat record_format, const unsigned indent_amount = 0,
                         const MarcXmlWriter::TextConversionType text_conversion_type = MarcXmlWriter::NoConversion) const;

    bool isProbablyNewerThan(const Record &other) const;

    /** \brief Adds fields of "other" to this.
     *  \note  If non-repeatable fields of "other" already exist in this they will be silently ignored.
     */
    void merge(const Record &other);
    inline size_t getNumberOfFields() const { return fields_.size(); }
    inline const std::string &getLeader() const { return leader_; }
    inline std::string &getLeader() { return leader_; }
    inline bool hasValidLeader() const { return leader_.length() == LEADER_LENGTH; }
    bool isMonograph() const;
    inline bool isSerial() const { return leader_[7] == 's'; }
    bool isArticle() const;
    bool isPossiblyReviewArticle() const;
    bool isReviewArticle() const;
    bool isWebsite() const;
    inline bool isReproduction() const { return getFirstField("534") != end(); }
    bool isElectronicResource() const;
    bool isPrintResource() const;
    inline bool isCorporateBody() const { return getRecordType() == RecordType::AUTHORITY and hasTag("110"); }
    inline bool isMeeting() const { return getRecordType() == RecordType::AUTHORITY and hasTag("111"); }
    inline bool isPerson() const { return getRecordType() == RecordType::AUTHORITY and hasTag("100"); }

    inline std::string getControlNumber() const {
        if (unlikely(fields_.empty() or fields_.front().getTag() != "001"))
            return "";
        else
            return fields_.front().getContents();
    }

    /** \return The main title (contents of 245$a or, if that does not exist, the contents of 245$b) or the empty string
     *          in the very unlikely case that we can't find it.
     */
    std::string getMainTitle() const;

    /** \return An approximation of the complete title generated from various subfields of field 245. */
    std::string getCompleteTitle() const;

    /** \return The title of the superior work, if applicable. (contents of 773$a) */
    std::string getSuperiorTitle() const;

    /** \return The control number of the superior work, if found, else the empty string. */
    std::string getSuperiorControlNumber() const;

    /** \return A "summary" (could be an abstract etc.), if found, else the empty string. */
    std::string getSummary() const;

    /** \return A guess at the publication year or the fallback value if we could not find one. */
    std::string getMostRecentPublicationYear(const std::string &fallback = "") const;

    /** \return Date of production, publication, distribution, manufacture, or copyright notice.
     *          For merged records this is typically a list.
     */
    std::vector<std::string> getDatesOfProductionEtc() const;

    // \return The author from 100$a or the empty string if the record has no 100$a.
    std::string getMainAuthor() const;

    /** \return All author names in fields 100$a and 700$a. */
    std::set<std::string> getAllAuthors() const;

    /** \return All author names in fields 100$a and 700$a and their associated authority record PPN's. */
    std::map<std::string, std::string> getAllAuthorsAndPPNs() const;

    /** \return All ISSN's including ISSN's of superior works */
    std::set<std::string> getAllISSNs() const;

    std::set<std::string> getDOIs() const;

    /** Returns all ISSN's as normalised ISSN's, i.e. w/o hypens. */
    std::set<std::string> getISSNs() const;

    /** Returns all superior ISSN's as normalised ISSN's, i.e. w/o hypens. */
    std::set<std::string> getSuperiorISSNs() const;

    /** Returns all ISBN's as normalised ISBN's, i.e. w/o hypens. */
    std::set<std::string> getISBNs() const;

    std::set<std::string> getDDCs() const;
    std::set<std::string> getRVKs() const;
    std::set<std::string> getSSGNs() const;

    /** \brief  Return the extracted GND codes from the fields determined by the provided tags.
     *  \param  tags  If non-empty extract codes from the fields w/ these tags o/w extract codes from all data fields.
     *  \return The extracted GND codes.
     */
    std::set<std::string> getReferencedGNDNumbers(const std::set<std::string> &tags = {}) const;

    /** \brief  Extracts a keyword and its synonyms from an authority record.
     *  \note   Aborts if the record is not an authority record.
     *  \return true if a keyword was found and false o/w.
     */
    bool getKeywordAndSynonyms(KeywordAndSynonyms * const keyword_synonyms) const;

    /** \return An iterator pointing to the first field w/ tag "field_tag" or end() if no such field was found. */
    inline const_iterator getFirstField(const Tag &field_tag) const {
        return std::find_if(fields_.cbegin(), fields_.cend(),
                            [&field_tag](const Field &field){ return field.getTag() == field_tag; });
    }

    /** \return An iterator pointing to the first field w/ tag "field_tag" or end() if no such field was found. */
    inline iterator getFirstField(const Tag &field_tag) {
        return std::find_if(fields_.begin(), fields_.end(),
                            [&field_tag](const Field &field){ return field.getTag() == field_tag; });
    }

    /** \return Returns the content of the first field with given tag or an empty string if the tag is not present. */
    const std::string getFirstFieldContents(const Tag &field_tag) const {
        const_iterator field(getFirstField(field_tag));
        if (field == fields_.cend())
            return "";
        else
            return field->getContents();
    }

    /** \return Returns the content of the first subfield found in a given tag or an empty string if the subfield cannot be found. */
    const std::string getFirstSubfieldValue(const Tag &field_tag, const char code) const {
        for (const auto &field : getTagRange(field_tag)) {
            const std::string value(field.getSubfields().getFirstSubfieldWithCode(code));
            if (not value.empty())
                return value;
        }
        return "";
    }

    inline RecordType getRecordType() const {
        if (leader_[6] == 'z')
            return RecordType::AUTHORITY;
        if (leader_[6] == 'w')
            return RecordType::CLASSIFICATION;
        return __builtin_strchr("acdefgijkmoprt", leader_[6]) == nullptr ? RecordType::UNKNOWN : RecordType::BIBLIOGRAPHIC;
    }

    enum BibliographicLevel getBibliographicLevel();
    void setBibliographicLevel(const Record::BibliographicLevel new_bibliographic_level);
    inline bool hasFieldWithTag(const MARC::Tag &tag) const { return findTag(tag) != end(); }
    inline Field getField(const size_t field_index) { return fields_[field_index]; }
    inline const Field &getField(const size_t field_index) const { return fields_[field_index]; }
    inline size_t getFieldIndex(const const_iterator &field) const { return field - fields_.begin(); }

    /** Insert a new field at the beginning of the range for that field.
     *  \return True if we added the new field and false if it is a non-repeatable field and we already have this tag.
     *  \note   "new_field_value" includes the two indicators and any subfield structure if "new_field_tag" references a
     *          variable field.
     */
    bool insertField(const Tag &new_field_tag, const std::string &new_field_value);

    inline bool insertField(const Tag &new_field_tag, const Subfields &subfields, const char indicator1 = ' ',
                            const char indicator2 = ' ')
        { return insertField(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + subfields.toString()); }

    inline bool insertField(const Field &field) { return insertField(field.getTag(), field.getContents()); }

    inline bool insertField(const Tag &new_field_tag, const std::vector<Subfield> &subfields, const char indicator1 = ' ',
                            const char indicator2 = ' ')
    {
        std::string new_field_value;
        new_field_value += indicator1;
        new_field_value += indicator2;
        for (const auto &subfield : subfields)
            new_field_value += subfield.toString();
        return insertField(new_field_tag, new_field_value);
    }

    inline bool insertField(const Tag &new_field_tag, const char subfield_code, const std::string &new_subfield_value,
                            const char indicator1 = ' ', const char indicator2 = ' ')
    {
        return insertField(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + "\x1F"
                           + std::string(1, subfield_code) + new_subfield_value);
    }

    /** Insert a new field at the end of the range for that field.
     *  \return True if we added the new field and false if it is a non-repeatable field and we already have this tag.
     *  \note   "new_field_value" includes the two indicators and any subfield structure if "new_field_tag" references a
     *          variable field.
     */
    bool insertFieldAtEnd(const Tag &new_field_tag, const std::string &new_field_value);

    inline bool insertFieldAtEnd(const Tag &new_field_tag, const Subfields &subfields, const char indicator1 = ' ',
                                 const char indicator2 = ' ')
        { return insertFieldAtEnd(new_field_tag, std::string(1, indicator1) + std::string(1, indicator2) + subfields.toString()); }

    inline bool insertFieldAtEnd(const Field &field) { return insertFieldAtEnd(field.getTag(), field.getContents()); }

    inline bool insertFieldAtEnd(const Tag &new_field_tag, const std::vector<Subfield> &subfields, const char indicator1 = ' ',
                                 const char indicator2 = ' ')
    {
        std::string new_field_value;
        new_field_value += indicator1;
        new_field_value += indicator2;
        for (const auto &subfield : subfields)
            new_field_value += subfield.toString();
        return insertFieldAtEnd(new_field_tag, new_field_value);
    }

    void appendField(const Tag &new_field_tag, const std::string &field_contents, const char indicator1 = ' ', const char indicator2 = ' ');
    void appendField(const Tag &new_field_tag, const Subfields &subfields, const char indicator1 = ' ', const char indicator2 = ' ');
    void appendField(const Field &field);

    /** \brief Replaces the first field w/ tag "field_tag".
     *  \note  If no field w/ tag "field_tag" exists, a new field will be inserted.
     */
    void replaceField(const Tag &field_tag, const std::string &field_contents, const char indicator1 = ' ', const char indicator2 = ' ');
    inline void replaceField(const Tag &field_tag, const Subfields &subfields, const char indicator1 = ' ', const char indicator2 = ' ') {
        replaceField(field_tag, subfields.toString(), indicator1, indicator2);
    }

    /** \brief  Adds a subfield to the first existing field with tag "field_tag".
     *  \return True if a field with field tag "field_tag" existed and false if no such field was found.
     */
    bool addSubfield(const Tag &field_tag, const char subfield_code, const std::string &subfield_value);

    /** \brief Performs edits on MARC rercord.
     *  \param error_message  If errors occurred this will contain explanatory text for the last error only.
     *  \return True if all edits succeeded and false if at least one edit failed.
     */
    bool edit(const std::vector<EditInstruction> &edit_instructions, std::string * const error_message);

    /** \brief Locate a field in a local block.
     *  \param local_field_tag      The nested tag that we're looking for.
     *  \param block_start          An iterator referencing the start of the local block that we're scanning.
     *  \param indicator1           An indicator that we're looking for. A question mark here means don't care.
     *  \param indicator2           An indicator that we're looking for. A question mark here means don't care.
     *  \return The half-open range of fields that matched our criteria.
     */
    ConstantRange findFieldsInLocalBlock(const Tag &local_field_tag, const const_iterator &block_start, const char indicator1 = '?',
                                         const char indicator2 = '?') const;

    /** \brief Locate a field in a local block.
     *  \param local_field_tag      The nested tag that we're looking for.
     *  \param block_start          An iterator referencing the start of the local block that we're scanning.
     *  \param indicator1           An indicator that we're looking for. A question mark here means don't care.
     *  \param indicator2           An indicator that we're looking for. A question mark here means don't care.
     *  \return The half-open range of fields that matched our criteria.
     */
    Range findFieldsInLocalBlock(const Tag &local_field_tag, const iterator &block_start, const char indicator1 = '?',
                                 const char indicator2 = '?');

    inline iterator begin() { return fields_.begin(); }
    inline iterator end() { return fields_.end(); }
    inline const_iterator begin() const { return fields_.cbegin(); }
    inline const_iterator end() const { return fields_.cend(); }

    // Warning: you can only call back() on a non-empty record!!
    inline Field &back() { return fields_.back(); }
    inline const Field &back() const { return fields_.back(); }

    // Alphanumerically sorts the fields in the range [begin_field, end_field).
    inline void sortFields(const iterator &begin_field, const iterator &end_field) { std::stable_sort(begin_field, end_field); }
    inline void sortFieldTags(const iterator &begin_field, const iterator &end_field)  {
        std::stable_sort(begin_field, end_field, [](const Field &lhs, const Field &rhs){ return lhs.tag_ < rhs.tag_; });
    }

    ConstantRange getTagRange(const std::vector<Tag> &tags) const;

    /** \return Iterators pointing to the half-open interval of the first range of fields corresponding to the tag "tag".
     *  \remark {
     *     Typical usage of this function looks like this:<br />
     *     \code{.cpp}
     *         for (auto &field : record.getTagRange("022")) {
     *             field.doSomething();
     *             ...
     *         }
     *
     *     \endcode
     *  }
     */
    inline ConstantRange getTagRange(const Tag &tag) const { return getTagRange(std::vector<Tag>{ tag }); }

    /** \return Iterators pointing to the half-open interval of the first range of fields corresponding to the tag "tag".
     *  \remark {
     *     Typical usage of this function looks like this:<br />
     *     \code{.cpp}
     *         for (auto &field : record.getTagRange("022")) {
     *             field.doSomething();
     *             ...
     *         }
     *
     *     \endcode
     *  }
     */
    Range getTagRange(const Tag &tag);

    /** \return An iterator that references the first fields w/ tag "tag" or end() if no such fields exist. */
    inline iterator findTag(const Tag &tag, iterator start_iterator) {
        return std::find_if(start_iterator, fields_.end(), [&tag](const Field &field) -> bool { return field.getTag() == tag; });
    }

    /** \return An iterator that references the first fields w/ tag "tag" or end() if no such fields exist. */
    inline iterator findTag(const Tag &tag) {
        return findTag(tag, begin());
    }

    /** \return An iterator that references the first fields w/ tag "tag" or end() if no such fields exist. */
    const_iterator findTag(const Tag &tag, const_iterator start_iterator) const {
        return std::find_if(start_iterator, fields_.cend(),
                            [&tag](const Field &field) -> bool { return field.getTag() == tag; });
    }

    /** \return An iterator that references the first fields w/ tag "tag" or end() if no such fields exist. */
    const_iterator findTag(const Tag &tag) const {
        return findTag(tag, begin());
    }

    /** \brief  Changes all from-tags to to-tags.
     *  \return The number of fields whose tags were changed.
     */
    size_t reTag(const Tag &from_tag, const Tag &to_tag);

    /** \return The list of tags in the record, including duplicates. */
    inline std::vector<std::string> getTags()  {
        std::vector<std::string> tags;
        tags.reserve(fields_.size());
        for (const auto &field : fields_)
            tags.emplace_back(field.getTag().toString());
        return tags;
    }

    /** \brief Removes the element at pos
     *  \return The iterator following pos.
     */
    inline iterator erase(const iterator pos) { return fields_.erase(pos); }

    /** \brief Removes one or more fields w/ tag "tag".
     *  \param  tag                    Delete fields w/ this tag.
     *  \param  first_occurrence_only  If true, we delete at most one field.
     *  \return The iterator following the deleted fields.
     */
    iterator erase(const Tag &tag, const bool first_occurrence_only = false);

    bool deleteFieldWithSubfieldCodeMatching(const Tag &tag, const char subfield_code, const ThreadSafeRegexMatcher &matcher);

    /** \return True if field with tag "tag" exists. */
    inline bool hasTag(const Tag &tag) const { return findTag(tag) != fields_.cend(); }

    /** \return True if field with tag "tag" and indicators "indicator1" and "indicator2" exists. */
    bool hasTagWithIndicators(const Tag &tag, const char indicator1, const char indicator2) const;

    /** \return Values for all fields with tag "tag" and subfield code "subfield_code". */
    std::vector<std::string> getSubfieldValues(const Tag &tag, const char subfield_code) const;

    /** \return Values for all fields with tag "tag" and subfield codes "subfield_codes". */
    std::vector<std::string> getSubfieldValues(const Tag &tag, const std::string &subfield_codes) const;

    /** \return Values for all fields with tag "tag" and subfield codes "subfield_codes". Handle subfields of numeric subfields like 9v appropriately*/
    std::vector<std::string> getSubfieldAndNumericSubfieldValues(const Tag &tag, const std::string &subfield_spec) const;

    /** \return Iterators pointing to the first field of each local data block, i.e. blocks of LOK fields.
     */
    std::vector<const_iterator> findStartOfAllLocalDataBlocks() const;

    /** \return Iterators pointing to the first field of each local data block, i.e. blocks of LOK fields.
     */
    std::vector<iterator> findStartOfAllLocalDataBlocks();

    void deleteLocalBlocks(std::vector<iterator> &local_block_starts);

    /** \param indicator1  The returned range only includes fields matching this indicator. A question mark is the wildcard character here.
     *  \param indicator2  The returned range only includes fields matching this indicator. A question mark is the wildcard character here.
     *  \return Iterators pointing to the half-open interval of the first range of fields corresponding to the tag "tag" in a local block
     *          starting at "local_block_start".
     *
     *  \remark {
     *     Typical usage of this function looks like this:<br />
     *     \code{.cpp}
     *         for (auto &local_field : record.getLocalTagRange("852", local_block_start)) {
     *             local_field.doSomething();
     *             ...
     *         }
     *
     *     \endcode
     *  }
     */
    ConstantRange getLocalTagRange(const Tag &local_field_tag, const const_iterator &block_start, const char indicator1 = '?',
                                   const char indicator2 = '?') const;

    /** \return An iterator pointing to the first field w/ local tag "local_field_tag" or end() if no such field was found. */
    const_iterator getFirstLocalField(const Tag &local_field_tag, const const_iterator &block_start) const;

    /** \return The set of all tags in the record. */
    std::unordered_set<std::string> getTagSet() const;

    /** \brief Delete all fields w/ tag "field_tag"
     *  \return The number of deleted fields.
     */
    size_t deleteFields(const Tag &field_tag);

    void deleteFields(std::vector<size_t> field_indices);
    bool isValid(std::string * const error_message) const;

    /** \brief Match a field or subfield against a regular expression.
     *  \param field_or_field_and_subfield_code Must be either a field tag or a field tag plus a single subfield code.
     */
    bool fieldOrSubfieldMatched(const std::string &field_or_field_and_subfield_code, RegexMatcher * const regex_matcher) const;

    /** \return A vector of iterators to fields that match the regular expression.
     */
    std::vector<iterator> getMatchedFields(const std::string &field_or_field_and_subfield_code, RegexMatcher * const regex_matcher);

    std::string toBinaryString() const;
    void toXmlStringHelper(MarcXmlWriter * const xml_writer) const;

    /** \brief   Removes all fields starting at and including "field_iter".
     *  \return  The number of fields that were removed at the end of the records.
     *  \warning "field_iter" must be a valid interator into the fields of the current record!
     */
    size_t truncate(const const_iterator field_iter);

    static char BibliographicLevelToChar(const BibliographicLevel bibliographic_level);
    static std::string RecordTypeToString(const RecordType record_type);
};


enum class FileType { AUTO, BINARY, XML };
enum class GuessFileTypeBehaviour { ATTEMPT_A_READ, USE_THE_FILENAME_ONLY };


std::string FileTypeToString(const FileType file_type);


/** \brief  Determines the file type of "filename".
 *  \param  filename                   The file whose type we want to determine.
 *  \param  guess_file_type_behaviour  Whether to just use the filename or, for existing files, to attempt a read.
 *  \return FileType::BINARY or FileType::XML.
 *  \note   Aborts if we can't determine the file type or if it is not FileType::BINARY nor FileType::XML.
 */
FileType GuessFileType(const std::string &filename,
                       const GuessFileTypeBehaviour guess_file_type_behaviour = GuessFileTypeBehaviour::ATTEMPT_A_READ);


class Reader {
protected:
    File *input_;
    Reader(File * const input): input_(input) { }
public:
    virtual ~Reader() { delete input_; }

    virtual FileType getReaderType() = 0;
    virtual Record read() = 0;

    /** \brief Rewind the underlying file. */
    virtual void rewind() = 0;

    /** \return The path of the underlying file. */
    inline const std::string &getPath() const { return input_->getPath(); }

    /** \return The file position of the start of the next record. */
    virtual off_t tell() const = 0;

    virtual bool seek(const off_t offset, const int whence = SEEK_SET) = 0;

    /** \return a BinaryMarcReader or an XmlMarcReader. */
    static std::unique_ptr<Reader> Factory(const std::string &input_filename, FileType reader_type = FileType::AUTO);
};


class BinaryReader final : public Reader {
    friend class Reader;
    Record last_record_;
    off_t next_record_start_;
    const char *mmap_;
    size_t offset_, input_file_size_;
private:
    explicit BinaryReader(File * const input);
public:
    virtual ~BinaryReader() final;

    virtual FileType getReaderType() override final { return FileType::BINARY; }
    virtual Record read() override final;
    virtual void rewind() override final;

    /** \return The file position of the start of the next record. */
    virtual off_t tell() const override final { return next_record_start_; }

    virtual inline bool seek(const off_t offset, const int whence = SEEK_SET) override final;
private:
    Record actualRead();
};


class XmlReader: public Reader {
    friend class Reader;
    XMLSubsetParser<File> *xml_parser_;
    std::string namespace_prefix_;
private:
    /** \brief Initialise a XmlReader instance.
     *  \param input                        Where to read from.
     *  \param skip_over_start_of_document  Skips to the first marc:record tag.  Do not set this if you intend
     *                                      to seek to an offset on \"input\" before calling this constructor.
     */
    explicit XmlReader(File * const input, const bool skip_over_start_of_document = true)
        : Reader(input), xml_parser_(new XMLSubsetParser<File>(input))
    {
        if (skip_over_start_of_document)
            skipOverStartOfDocument();
    }
public:
    virtual ~XmlReader() { delete xml_parser_; }

    virtual FileType getReaderType() override final { return FileType::XML; }
    virtual Record read() override final;
    virtual void rewind() override final;

    /** \return The file position of the start of the next record. */
    virtual inline off_t tell() const override final { return xml_parser_->tell(); }

    virtual inline bool seek(const off_t offset, const int whence = SEEK_SET) override final { return xml_parser_->seek(offset, whence); }
private:
    void parseLeader(const std::string &input_filename, Record * const new_record);
    void parseControlfield(const std::string &input_filename, const std::string &tag, Record * const record);
    void parseDatafield(const std::string &input_filename,
                        const std::map<std::string, std::string> &datafield_attrib_map,
                        const std::string &tag, Record * const record);
    void skipOverStartOfDocument();
    bool getNext(XMLSubsetParser<File>::Type * const type, std::map<std::string, std::string> * const attrib_map,
                 std::string * const data);
};


class Writer {
protected:
    std::unique_ptr<File> output_;
protected:
    explicit Writer(File * const output): output_(output) { }
public:
    enum WriterMode { OVERWRITE, APPEND };
public:
    virtual ~Writer() = default;

    virtual void write(const Record &record) = 0;

    /** \return a reference to the underlying, assocaiated file. */
    File &getFile() { return *output_; }

    /** \brief Flushes the buffers of the underlying File to the storage medium.
     *  \return True on success and false on failure.  Sets errno if there is a failure.
     */
    bool flush() { return output_->flush(); }

    /** \note If you pass in AUTO for "writer_type", "output_filename" must end in ".mrc" or ".xml"! */
    static std::unique_ptr<Writer> Factory(const std::string &output_filename, FileType writer_type = FileType::AUTO,
                                           const WriterMode writer_mode = WriterMode::OVERWRITE);
};


class BinaryWriter final : public Writer {
    friend class Writer;
private:
    BinaryWriter(File * const output): Writer(output) { }
public:
    virtual ~BinaryWriter() override final = default;

    virtual void write(const Record &record) override final;
};


class XmlWriter final : public Writer {
    friend class Writer;
    MarcXmlWriter xml_writer_;
private:
    explicit XmlWriter(File * const output, const unsigned indent_amount = 0,
                       const MarcXmlWriter::TextConversionType text_conversion_type = MarcXmlWriter::NoConversion)
        : Writer(output), xml_writer_(output, /* suppress_header_and_tailer = */false, indent_amount, text_conversion_type) { }
public:
    virtual ~XmlWriter() override final;

    virtual void write(const Record &record) override final;
};


void FileLockedComposeAndWriteRecord(Writer * const marc_writer, const Record &record);


/** \brief  Does an in-place filtering for records with duplicate control numbers.
 *  \return The number of dropped records.
 *  \note   We keep the first occurrence of a record with a given control number and drop and drop any subsequent ones.
 */
unsigned RemoveDuplicateControlNumberRecords(const std::string &marc_filename);


/** \brief Checks the validity of an entire file.
 *  \return true if the file was a valid MARC file, else false
 */
bool IsValidMarcFile(const std::string &filename, std::string * const err_msg, const FileType file_type = FileType::AUTO);


/** \brief Extracts the optional language code from field 008.
 *  \return The extracted language code or the empty string if no language code was found.
 */
std::string GetLanguageCode(const Record &record);


/** \brief Extracts all language codes from a MARC record
 *  \return The count of the extracted language codes
 */
size_t GetLanguageCodes(const Record &record, std::set<std::string> * const language_codes);


/** \brief True if a GND code was found in 035$a else false. */
bool GetGNDCode(const MARC::Record &record, std::string * const gnd_code);


/** \brief True if a wikidata id was found in 024$a ($2 = wikidata) else false. */
bool GetWikidataId(const Record &record, std::string * const wikidata_id);


/** \brief Generates a reproducible SHA-1 hash over our internal data.
 *  \param excluded_fields        The list of tags specified here will be excluded from the checksum calculation.
 *  \param suppress_local_fields  If true we exclude fields that have non-pure-digit tags or tags that contain at least one digit nine.
 *  \return the hash
 *  \note Equivalent records with different field order generate the same hash.  (This can only happen if at least one tag
 *        has been repeated.)
 */
std::string CalcChecksum(const Record &record, const std::set<Tag> &excluded_fields = { "001" }, const bool suppress_local_fields = true);


// Takes local UB Tübingen criteria into account.
bool UBTueIsElectronicResource(const Record &marc_record);


/** \brief Takes local UB Tübingen criteria into account.
 *  \return True if the referenced item has been ordered but is not yet available, else false.
 */
bool UBTueIsAquisitionRecord(const Record &marc_record);


/** \return A Non-empty string if we managed to find a parent PPN o/w the empty string. */
std::string GetParentPPN(const Record &record);


bool IsOpenAccess(const Record &marc_record);


// \warning After a call to this function you may want to rewind the MARC Reader.
size_t CollectRecordOffsets(MARC::Reader * const marc_reader, std::unordered_map<std::string, off_t> * const control_number_to_offset_map);


/** \brief Handles optional command-line arguments of the form "--input-format=marc-21" and "--input-format=marc-xml"
 *
 *  If an optional argument of one of the expected forms is found in argv[arg_no], argc and argv will be incremented and
 *  decremented respectively and the corresponding file type will be returned.  If not, the value of "default_file_type" will
 *  be returned instead and "argc" and "argv" will remain unmodified.  If an unrecognized format has been provided, we call LOG_ERROR.
 */
FileType GetOptionalReaderType(int * const argc, char *** const argv, const int arg_no, const FileType default_file_type = FileType::AUTO);


/** \brief Handles optional command-line arguments of the form "--output-format=marc-21" and "--output-format=marc-xml"
 *
 *  If an optional argument of one of the expected forms is found in argv[arg_no], argc and argv will be incremented and
 *  decremented respectively and the corresponding file type will be returned.  If not, the value of "default_file_type" will
 *  be returned instead and "argc" and "argv" will remain unmodified.  If an unrecognized format has been provided, we call LOG_ERROR.
 */
FileType GetOptionalWriterType(int * const argc, char *** const argv, const int arg_no, const FileType default_file_type = FileType::AUTO);


/** \return True if field "field" contains a reference to another MARC record that is not a link to a superior work and false, if not. */
extern const std::vector<Tag> CROSS_LINK_FIELDS;
bool IsCrossLinkField(const MARC::Record::Field &field, std::string * const partner_control_number, const std::vector<MARC::Tag> &cross_link_fields = CROSS_LINK_FIELDS);


/** \return partner PPN's or the empty set if none were found. */
std::set<std::string> ExtractCrossReferencePPNs(const MARC::Record &record);


// Returns the field where "index_term" should be stored.
Record::Field GetIndexField(const std::string &index_term);


bool IsSubjectAccessTag(const Tag &tag);

std::set<std::string> ExtractOnlineCrossLinkPPNs(const MARC::Record &record);
std::set<std::string> ExtractPrintCrossLinkPPNs(const MARC::Record &record);
std::set<std::string> ExtractCrossLinkPPNs(const MARC::Record &record);


// \warning Only very few codes are currently supported!
// \return The mapped code if found o/w an empty string.
std::string MapToMARCLanguageCode(const std::string &some_code);


} // namespace MARC
