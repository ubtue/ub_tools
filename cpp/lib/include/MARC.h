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
#include <memory>
#include <string>
#include <vector>
#include "Compiler.h"
#include "File.h"
#include "MarcXmlWriter.h"
#include "SimpleXmlParser.h"


namespace MARC {


class Record {
public:
    class Field {
        std::string tag_;
        std::string contents_;
    public:
        Field(const std::string &tag, const std::string &contents): tag_(tag), contents_(contents) { }
        inline const std::string &getTag() const { return tag_; }
        inline const std::string &getContents() const { return contents_; }
        inline std::string getContents() { return contents_; }
        inline bool isControlField() const __attribute__ ((pure)) { return tag_ <= "009"; }
        inline bool isDataField() const __attribute__ ((pure)) { return tag_ > "009"; }
        inline char getIndicator1() const { return unlikely(contents_.empty()) ? '\0' : contents_[0]; }
        inline char getIndicator2() const { return unlikely(contents_.size() < 2) ? '\0' : contents_[1]; }
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
    static constexpr unsigned MAX_RECORD_LENGTH          = 99999;
    static constexpr unsigned DIRECTORY_ENTRY_LENGTH     = 12;
    static constexpr unsigned RECORD_LENGTH_FIELD_LENGTH = 5;
    static constexpr unsigned LEADER_LENGTH              = 24;

    enum RecordType { AUTHORITY, UNKNOWN, BIBLIOGRAPHIC, CLASSIFICATION };
    typedef std::vector<Field>::iterator iterator;
    typedef std::vector<Field>::const_iterator const_iterator;
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
    ssize_t getFirstFieldIndex(const std::string &tag) const;

    RecordType getRecordType() const {
        if (leader_[6] == 'z')
            return AUTHORITY;
        if (leader_[6] == 'w')
            return CLASSIFICATION;
        return __builtin_strchr("acdefgijkmoprt", leader_[6]) == nullptr ? UNKNOWN : BIBLIOGRAPHIC;
    }

    inline const std::string &getFieldData(const size_t field_index) const
        { return fields_[field_index].getContents(); }

    inline iterator begin() { return fields_.begin(); }
    inline iterator end() { return fields_.end(); }
    inline const_iterator begin() const { return fields_.cbegin(); }
    inline const_iterator end() const { return fields_.cend(); }

    /** \brief Finds local ("LOK") block boundaries.
     *  \param local_block_boundaries  Each entry contains the index of the first field of a local block in "first"
     *                                 and the index of the last field + 1 of a local block in "second".
     */
    size_t findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> * const local_block_boundaries) const;
};


class Subfields {
    const std::string &field_contents_;
public:
    explicit Subfields(const Record::Field &field): field_contents_(field.getContents()) { }
    unsigned size() const __attribute__ ((pure))
        { return std::count(field_contents_.cbegin(), field_contents_.cend(), '\x1F');}
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

    /** \return a reference to the underlying, assocaiated file. */
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
