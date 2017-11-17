/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_data\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


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
    friend class Reader;
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


inline unsigned ToUnsigned(const char *cp, const unsigned count) {
    unsigned retval(0);
    for (unsigned i(0); i < count; ++i)
        retval = retval * 10 + (*cp++ - '0');

    return retval;
}


Record::Record(const size_t record_size, char * const record_start)
    : record_size_(record_size), leader_(record_start, record_size)
{
    const char * const base_address_of_data(record_start + ToUnsigned(record_start + 12, 5));

    // Process directory:
    const char *directory_entry(record_start + LEADER_LENGTH);
    while (directory_entry != base_address_of_data - 1) {
        if (unlikely(directory_entry > base_address_of_data))
            logger->error("in Record::Record: directory_entry > base_address_of_data!");
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


ssize_t Record::getFirstFieldIndex(const std::string &tag) const {
    const auto iter(std::find_if(fields_.cbegin(), fields_.cend(),
                                 [&tag](const Field &field){ return field.getTag() == tag; }));
    return (iter == fields_.cend()) ? -1 : std::distance(fields_.cbegin(), iter);
}


size_t Record::findAllLocalDataBlocks(std::vector<std::pair<size_t, size_t>> *const local_block_boundaries) const
{
    local_block_boundaries->clear();

    size_t local_block_start(getFirstFieldIndex("LOK"));
    if (static_cast<ssize_t>(local_block_start) == -1)
        return 0;

    size_t local_block_end(local_block_start + 1);
    while (local_block_end < fields_.size()) {
        if (StringUtil::StartsWith(fields_[local_block_end].getContents(), "  ""\x1F""0000")) {
            local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));
            local_block_start = local_block_end;
        }
        ++local_block_end;
    }
    local_block_boundaries->emplace_back(std::make_pair(local_block_start, local_block_end));

    return local_block_boundaries->size();
}


class Subfields {
    const std::string &field_contents_;
public:
    explicit Subfields(const Record::Field &field): field_contents_(field.getContents()) { }
    unsigned size() const __attribute__ ((pure))
        { return std::count(field_contents_.cbegin(), field_contents_.cend(), '\x1F');}
};


class Reader {
    std::unique_ptr<File> input_;
public:
    explicit Reader(const std::string &input_filename);
    Record read();
};


Reader::Reader(const std::string &input_filename): input_(FileUtil::OpenInputFileOrDie(input_filename)) {
}


Record Reader::read() {
    char buf[Record::MAX_RECORD_LENGTH];
    size_t bytes_read;
    if (unlikely((bytes_read = input_->read(buf, Record::RECORD_LENGTH_FIELD_LENGTH)) == 0))
        return Record();

    if (unlikely(bytes_read != Record::RECORD_LENGTH_FIELD_LENGTH))
        logger->error("in Reader::read: failed to read record length!");
    const unsigned record_length(ToUnsigned(buf, Record::RECORD_LENGTH_FIELD_LENGTH));

    bytes_read = input_->read(buf + Record::RECORD_LENGTH_FIELD_LENGTH,
                              record_length - Record::RECORD_LENGTH_FIELD_LENGTH);
    if (unlikely(bytes_read != record_length - Record::RECORD_LENGTH_FIELD_LENGTH))
        logger->error("Reader::read: failed to read a record!");

    return Record(record_length, buf);
}


class BinaryWriter {
    File &output_;
public:
    BinaryWriter(File * const output): output_(*output) { }
    void write(const Record &record);
};


inline std::string ToStringWithLeadingZeros(const unsigned n, const unsigned width) {
    const std::string as_string(std::to_string(n));
    return (as_string.length() < width) ? (std::string(width - as_string.length(), '0') + as_string) : as_string;
}


void BinaryWriter::write(const Record &record) {
    Record::const_iterator start(record.begin());
    do {
        Record::const_iterator end(start);
        unsigned record_size(Record::LEADER_LENGTH + 2 /* end-of-directory and end-of-record */);
        while (end != record.end()
               and (record_size + end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH
                    < Record::MAX_RECORD_LENGTH))
        {
            record_size += end->getContents().length() + 1 + Record::DIRECTORY_ENTRY_LENGTH;
            ++end;
        }

        std::string raw_record;
        raw_record.reserve(record_size);
        const unsigned no_of_fields(end - start);
        raw_record += ToStringWithLeadingZeros(record_size, /* width = */ 5);
        raw_record += record.leader_.substr(5, 12 - 5);
        const unsigned base_address_of_data(Record::LEADER_LENGTH + no_of_fields * Record::DIRECTORY_ENTRY_LENGTH
                                            + 1 /* end-of-directory */);
        raw_record += ToStringWithLeadingZeros(base_address_of_data, /* width = */ 5);
        raw_record += record.leader_.substr(17, Record::LEADER_LENGTH - 17);

        // Append the directory:
        unsigned field_start_offset(0);
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getTag()
                          + ToStringWithLeadingZeros(entry->getContents().length() + 1 /* field terminator */, 4)
                          + ToStringWithLeadingZeros(field_start_offset, /* width = */ 5);
            field_start_offset += entry->getContents().length() + 1 /* field terminator */;
        }
        raw_record += '\x1E'; // end-of-directory

        // Now append the field data:
        for (Record::const_iterator entry(start); entry != end; ++entry) {
            raw_record += entry->getContents();
            raw_record += '\x1E'; // end-of-field
        }
        raw_record += '\x1D'; // end-of-record

        output_.write(raw_record);
        
        start = end;
    } while (start != record.end());
}


} // namespace MARC


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 2)
        Usage();

    MARC::Reader reader(argv[1]);
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie("/tmp/out.mrc"));
    MARC::BinaryWriter writer(output.get());

    try {
        unsigned record_count(0);
        size_t max_record_size(0), max_field_count(0), max_local_block_count(0), max_subfield_count(0);
        std::map<MARC::Record::RecordType, unsigned> record_types_and_counts;
        while (const MARC::Record record = reader.read()) {
            writer.write(record);
            ++record_count;
            if (record.size() > max_record_size)
                max_record_size = record.size();
            if (record.getNumberOfFields() > max_field_count)
                max_field_count = record.getNumberOfFields();

            const MARC::Record::RecordType record_type(record.getRecordType());
            ++record_types_and_counts[record_type];
            if (record_type == MARC::Record::RecordType::UNKNOWN)
                std::cerr << "Unknown record type '" << record.getLeader()[6] << "' for control number "
                          << record.getControlNumber() << ".\n";

            for (const auto &field : record) {
                if (field.isDataField()) {
                    const MARC::Subfields subfields(field);
                    if (unlikely(subfields.size() > max_subfield_count))
                        max_subfield_count = subfields.size();
                }
            }

            std::vector<std::pair<size_t, size_t>> local_block_boundaries;
            const size_t local_block_count(record.findAllLocalDataBlocks(&local_block_boundaries));
            if (local_block_count > max_local_block_count)
                max_local_block_count = local_block_count;
        }

        std::cerr << "Read " << record_count << " record(s).\n";
        std::cerr << "The largest record contains " << max_record_size << " bytes.\n";
        std::cerr << "The record with the largest number of fields contains " << max_field_count << " field(s).\n";
        std::cerr << "The record with the most local data blocks has " << max_local_block_count
                  << " local block(s).\n";
        std::cerr << "Counted " << record_types_and_counts[MARC::Record::RecordType::BIBLIOGRAPHIC]
                  << " bibliographic record(s), " << record_types_and_counts[MARC::Record::RecordType::AUTHORITY]
                  << " classification record(s), " << record_types_and_counts[MARC::Record::RecordType::CLASSIFICATION]
                  << " authority record(s), and " << record_types_and_counts[MARC::Record::RecordType::UNKNOWN]
                  << " record(s) of unknown record type.\n";
        std::cerr << "The field with the most subfields has " << max_subfield_count << " subfield(s).\n";
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
