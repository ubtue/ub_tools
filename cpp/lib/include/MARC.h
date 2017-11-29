/** \brief Various classes, functions etc. having to do with the Library of Congress MARC bibliographic format.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include "Compiler.h"
#include "File.h"
#include "MarcXmlWriter.h"
#include "SimpleXmlParser.h"


namespace MARC {


class Tag {
    /* We have to double this up, so we have one little endian integer for comparison, and one big endian integer
     * containing a char[4] for printig.
     */
    union {
        uint32_t as_int_;
        char as_cstring_[4];
    } tag_;
public:
    inline Tag() = default;
    inline Tag(const char raw_tag[4]) {
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    inline Tag(const std::string &raw_tag) {
        if (unlikely(raw_tag.length() != 3))
            throw std::runtime_error("in Tag: \"raw_tag\" must have a length of 3: " + raw_tag);
        tag_.as_int_ = 0;
        tag_.as_cstring_[0] = raw_tag[0];
        tag_.as_cstring_[1] = raw_tag[1];
        tag_.as_cstring_[2] = raw_tag[2];
    }

    /** Copy constructor. */
    Tag(const Tag &other_tag): tag_(other_tag.tag_) {}

    bool operator==(const Tag &rhs) const { return to_int() == rhs.to_int(); }
    bool operator!=(const Tag &rhs) const { return to_int() != rhs.to_int(); }
    bool operator>(const Tag &rhs) const  { return to_int() >  rhs.to_int(); }
    bool operator>=(const Tag &rhs) const { return to_int() >= rhs.to_int(); }
    bool operator<(const Tag &rhs) const  { return to_int() <  rhs.to_int(); }
    bool operator<=(const Tag &rhs) const { return to_int() <= rhs.to_int(); }

    bool operator==(const std::string &rhs) const { return ::strcmp(c_str(), rhs.c_str()) == 0; }
    bool operator==(const char rhs[4]) const { return ::strcmp(c_str(), rhs) == 0; }

    std::ostream& operator<<(std::ostream& os) const { return os << to_string(); }
    friend std::ostream &operator<<(std::ostream &output,  const Tag &tag) { return output << tag.to_string(); }

    inline const char *c_str() const { return tag_.as_cstring_; }
    inline const std::string to_string() const { return std::string(c_str(), 3); }
    inline uint32_t to_int() const { return htonl(tag_.as_int_); }

    inline bool isTagOfControlField() const { return tag_.as_cstring_[0] == '0' and tag_.as_cstring_[1] == '0'; }
};


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
public:
    inline Subfields(std::vector<Subfield> &&subfields): subfields_(subfields) { }
    Subfields(const Subfields &other) = default;
    inline explicit Subfields(const std::string &field_contents) {
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
    Subfields(Subfields &&other) = default;

    inline const_iterator begin() const { return subfields_.cbegin(); }
    inline const_iterator end() const { return subfields_.cend(); }
    unsigned size() const { return subfields_.size(); }

    inline bool hasSubfield(const char subfield_code) const {
        return std::find_if(subfields_.cbegin(), subfields_.cend(),
                            [subfield_code](const Subfield subfield) -> bool
                                { return subfield.code_ == subfield_code; }) != subfields_.cend();
    }

    void addSubfield(const char subfield_code, const std::string &subfield_value);

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

    inline std::string toString() const {
        std::string as_string;
        for (const auto &subfield : subfields_)
            as_string += subfield.toString();
        return as_string;
    }
};


class Record {
public:
    class Field {
        friend class Record;
        Tag tag_;
        std::string contents_;
    private:
    public:
        Field(const std::string &tag, const std::string &contents): tag_(tag), contents_(contents) { }
        Field(const Tag &tag, const std::string &contents): tag_(tag), contents_(contents) { }
        inline const Tag &getTag() const { return tag_; }
        inline const std::string &getContents() const { return contents_; }
        inline std::string getContents() { return contents_; }
        inline bool isControlField() const __attribute__ ((pure)) { return tag_ <= "009"; }
        inline bool isDataField() const __attribute__ ((pure)) { return tag_ > "009"; }
        inline char getIndicator1() const { return unlikely(contents_.empty()) ? '\0' : contents_[0]; }
        inline char getIndicator2() const { return unlikely(contents_.size() < 2) ? '\0' : contents_[1]; }
        inline Subfields getSubfields() const { return Subfields(contents_); }
    };

    enum RecordType { AUTHORITY, UNKNOWN, BIBLIOGRAPHIC, CLASSIFICATION };
    typedef std::vector<Field>::iterator iterator;
    typedef std::vector<Field>::const_iterator const_iterator;

    /** \brief Represents a range of fields.
     *  \note  Returning this from a Record member function allows for a for-each loop.
     */
    class Range {
        const_iterator begin_;
        const_iterator end_;
    public:
        inline Range(const_iterator begin, const_iterator end): begin_(begin), end_(end) { }
        inline const_iterator begin() const { return begin_; }
        inline const_iterator end() const { return end_; }
        inline bool empty() const { return begin_ == end_; }
    };
private:
    friend class BinaryReader;
    friend class XmlReader;
    friend class BinaryWriter;
    friend class XmlWriter;
    size_t record_size_; // in bytes
    std::string leader_;
    std::vector<Field> fields_;
public:
    static constexpr unsigned MAX_RECORD_LENGTH                        = 99999;
    static constexpr unsigned MAX_VARIABLE_FIELD_DATA_LENGTH           = 9998; // Max length without trailing terminator
    static constexpr unsigned DIRECTORY_ENTRY_LENGTH                   = 12;
    static constexpr unsigned RECORD_LENGTH_FIELD_LENGTH               = 5;
    static constexpr unsigned LEADER_LENGTH                            = 24;
private:
    Record(): record_size_(LEADER_LENGTH + 1 /* end-of-directory */ + 1 /* end-of-record */) { }
public:
    explicit Record(const size_t record_size, char * const record_start);

    inline Record(Record &&other) {
        std::swap(record_size_, other.record_size_);
        leader_.swap(other.leader_);
        fields_.swap(other.fields_);
    }

    operator bool () const { return not fields_.empty(); }
    inline size_t size() const { return record_size_; }
    inline size_t getNumberOfFields() const { return fields_.size(); }
    inline const std::string &getLeader() const { return leader_; }
    inline std::string getControlNumber() const
        { return likely(fields_.front().getTag() == "001") ? fields_.front().getContents() : ""; }

    /** \return An iterator pointing to the first field w/ tag "field_tag" or end() if no such field was found. */
    inline const_iterator getFirstField(const Tag &field_tag) const {
        return std::find_if(fields_.cbegin(), fields_.cend(),
                            [&field_tag](const Field &field){ return field.getTag() == field_tag; });
    }

    RecordType getRecordType() const {
        if (leader_[6] == 'z')
            return AUTHORITY;
        if (leader_[6] == 'w')
            return CLASSIFICATION;
        return __builtin_strchr("acdefgijkmoprt", leader_[6]) == nullptr ? UNKNOWN : BIBLIOGRAPHIC;
    }

    void insertField(const Tag &new_field_tag, const std::string &new_field_value);

    inline void insertField(const Tag &new_field_tag, const Subfields &subfields, const char indicator1 = ' ',
                            const char indicator2 = ' ')
    {
        std::string new_field_value;
        new_field_value += indicator1;
        new_field_value += indicator2;
        for (const auto &subfield : subfields)
            new_field_value += subfield.toString();
        insertField(new_field_tag, new_field_value);
    }

    inline void insertField(const Tag &new_field_tag, std::vector<Subfield> subfields, const char indicator1 = ' ',
                            const char indicator2 = ' ')
    {
        std::string new_field_value;
        new_field_value += indicator1;
        new_field_value += indicator2;
        for (const auto &subfield : subfields)
            new_field_value += subfield.toString();
        insertField(new_field_tag, new_field_value);
    }

    /** \brief  Adds a subfield to the first existing field with tag "field_tag".
     *  \return True if a field with field tag "field_tag" existed and false if no such field was found.
     */
    bool addSubfield(const Tag &field_tag, const char subfield_code, const std::string &subfield_value);

    inline iterator begin() { return fields_.begin(); }
    inline iterator end() { return fields_.end(); }
    inline const_iterator begin() const { return fields_.cbegin(); }
    inline const_iterator end() const { return fields_.cend(); }

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
    Range getTagRange(const Tag &tag) const;

    /** \return True if field with tag "tag" exists. */
    inline bool hasTag(const Tag &tag) const {
        return std::find_if(fields_.begin(), fields_.end(),
                            [&tag](const Field &field) -> bool { return field.getTag() == tag; }) != fields_.end();
    }

    /** \return True if field with tag "tag" and indicators "indicator1" and "indicator2" exists. */
    bool hasTagWithIndicators(const Tag &tag, const char indicator1, const char indicator2) const;

    /** \return Values for all fields with tag "tag" and subfield code "subfield_code". */
    std::vector<std::string> getSubfieldValues(const Tag &tag, const char subfield_code) const;

    /** \return Values for all fields with tag "tag" and subfield code "subfield_code". */
    std::vector<std::string> getSubfieldValues(const Tag &tag, const std::string &subfield_codes) const;

    /** \brief Finds local ("LOK") block boundaries.
     *  \param local_block_boundaries  Each entry contains the iterator pointing to the first field of a local block
     *                                 in "first" and the iterator pointing past the last field of a local block in
     *                                 "second".
     */
    size_t findAllLocalDataBlocks(
        std::vector<std::pair<const_iterator, const_iterator>> * const local_block_boundaries) const;

    /** \brief Locate a field in a local block.
     *  \param indicators           The two 1-character indicators that we're looking for. A question mark here
     *                              means: don't care.  So, if you want to match any indicators you should pass "??"
     *                              here.
     *  \param field_tag            The 3 character tag that we're looking for.
     *  \param block_start_and_end  "first" must point to the first entry in "field_data" that belongs to the local
     *                              block that we're scanning and "second" one past the last entry.
     *  \param fields               The iterators pointing at the matched fields.
     *  \return The number of times the field was found in the block.
     */
    size_t findFieldsInLocalBlock(const Tag &field_tag, const std::string &indicators,
                                  const std::pair<const_iterator, const_iterator> &block_start_and_end,
                                  std::vector<const_iterator> * const fields) const;
};


class Reader {
public:
    enum ReaderType { AUTO, BINARY, XML };
protected:
    File *input_;
    Reader(File * const input): input_(input) { }
public:
    virtual ~Reader() { delete input_; }

    virtual ReaderType getReaderType() = 0;
    virtual Record read() = 0;

    /** \brief Rewind the underlying file. */
    virtual void rewind() = 0;

    /** \return The path of the underlying file. */
    inline const std::string &getPath() const { return input_->getPath(); }

    /** \return The current file position of the underlying file. */
    inline off_t tell() const { return input_->tell(); }

    inline bool seek(const off_t offset, const int whence = SEEK_SET) { return input_->seek(offset, whence); }

    /** \return a BinaryMarcReader or an XmlMarcReader. */
    static std::unique_ptr<Reader> Factory(const std::string &input_filename,
                                               ReaderType reader_type = AUTO);
};


class BinaryReader: public Reader {
public:
    explicit BinaryReader(File * const input): Reader(input) { }
    virtual ~BinaryReader() = default;

    virtual ReaderType getReaderType() final { return Reader::BINARY; }
    virtual Record read() final;
    virtual void rewind() final { input_->rewind(); }
};


class XmlReader: public Reader {
    SimpleXmlParser<File> *xml_parser_;
    std::string namespace_prefix_;
public:
    /** \brief Initialise a XmlReader instance.
     *  \param input                        Where to read from.
     *  \param skip_over_start_of_document  Skips to the first marc:record tag.  Do not set this if you intend
     *                                      to seek to an offset on \"input\" before calling this constructor.
     */
    explicit XmlReader(File * const input, const bool skip_over_start_of_document = true)
        : Reader(input), xml_parser_(new SimpleXmlParser<File>(input))
    {
        if (skip_over_start_of_document)
            skipOverStartOfDocument();
    }
    virtual ~XmlReader() = default;

    virtual ReaderType getReaderType() final { return Reader::XML; }
    virtual Record read() final;
    virtual void rewind() final;
private:
    void parseLeader(const std::string &input_filename, Record * const new_record);
    void parseControlfield(const std::string &input_filename, const std::string &tag, Record * const record);
    void parseDatafield(const std::string &input_filename,
                        const std::map<std::string, std::string> &datafield_attrib_map,
                        const std::string &tag, Record * const record);
    void skipOverStartOfDocument();
    bool getNext(SimpleXmlParser<File>::Type * const type, std::map<std::string, std::string> * const attrib_map,
                 std::string * const data);
};


class Writer {
public:
    enum WriterMode { OVERWRITE, APPEND };
    enum WriterType { XML, BINARY, AUTO };
public:
    virtual ~Writer() { }

    virtual void write(const Record &record) = 0;

    /** \return a reference to the underlying, assocaiated file. */
    virtual File &getFile() = 0;

    /** \note If you pass in AUTO for "writer_type", "output_filename" must end in ".mrc" or ".xml"! */
    static std::unique_ptr<Writer> Factory(const std::string &output_filename, WriterType writer_type = AUTO,
                                           const WriterMode writer_mode = WriterMode::OVERWRITE);
};


class BinaryWriter: public Writer {
    File &output_;
public:
    BinaryWriter(File * const output): output_(*output) { }

    virtual void write(const Record &record) final;

    /** \return a reference to the underlying, associated file. */
    virtual File &getFile() final { return output_; }
};


class XmlWriter: public Writer {
    MarcXmlWriter *xml_writer_;
public:
    explicit XmlWriter(File * const output_file, const unsigned indent_amount = 0,
                       const MarcXmlWriter::TextConversionType text_conversion_type = MarcXmlWriter::NoConversion);
    virtual ~XmlWriter() final { delete xml_writer_; }

    virtual void write(const Record &record) final;

    /** \return a reference to the underlying, assocaiated file. */
    virtual File &getFile() final { return *xml_writer_->getAssociatedOutputFile(); }
};


} // namespace MARC
