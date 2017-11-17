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
    friend class BinaryWriter;
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
private:
    Record() { }
};


class Subfields {
    const std::string &field_contents_;
public:
    explicit Subfields(const Record::Field &field): field_contents_(field.getContents()) { }
    unsigned size() const __attribute__ ((pure))
        { return std::count(field_contents_.cbegin(), field_contents_.cend(), '\x1F');}
};


class BinaryReader {
    std::unique_ptr<File> input_;
public:
    explicit BinaryReader(const std::string &input_filename);
    Record read();
};


class BinaryWriter {
    File &output_;
public:
    BinaryWriter(File * const output): output_(*output) { }
    void write(const Record &record);
};


} // namespace MARC
