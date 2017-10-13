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

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_data\n";
    std::exit(EXIT_FAILURE);
}


class Field {
    std::string tag_;
    std::string contents_;
public:
    Field(const std::string &tag, const std::string &contents): tag_(tag), contents_(contents) { }
    inline const std::string &getTag() const { return tag_; }
    inline const std::string &getContents() const { return contents_; }
    inline std::string getContents() { return contents_; }
};


class Record {
    size_t record_size_; // in bytes
    std::vector<Field> fields_;
public:
    explicit Record(const size_t record_size, char * const record_start);
    inline size_t size() const { return record_size_; }
    inline size_t getNumberOfFields() const { return fields_.size(); }
    inline std::string getControlNumber()
        { return fields_.front().getTag() == "001" ? fields_.front().getContents() : ""; }
};


inline unsigned ToUnsigned(const char *cp, const unsigned count) {
    unsigned retval(0);
    for (unsigned i(0); i < count; ++i)
        retval = retval * 10 + (*cp++ - '0');

    return retval;
}


const unsigned LEADER_LENGTH(24);


Record::Record(const size_t record_size, char * const record_start): record_size_(record_size) {
    record_start[17] = '\0';
    const char * const base_address_of_data(record_start + StringUtil::ToUnsigned(record_start + 12));
    
    // Process directory:
    const char *directory_entry(record_start + LEADER_LENGTH);
    while (directory_entry != base_address_of_data - 1) {
        if (unlikely(directory_entry > base_address_of_data))
            Error("in Record::Record: directory_entry > base_address_of_data!");
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


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 2)
        Usage();

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(argv[1]));

    try {
        char size_buf[5 + 1];
        unsigned record_count(0);
        size_t bytes_read;
        size_t max_record_size(0), max_field_count(0);
        while ((bytes_read = input->read(size_buf, 5))) {
            if (unlikely(bytes_read != 5))
                Error("failed to read record length!");
            size_buf[5] = '\0';
            const unsigned record_length(ToUnsigned(size_buf, 5));

            char buf[99999];
            bytes_read = input->read(buf, record_length - 5);
            if (unlikely(bytes_read != record_length - 5))
                Error("failed to read a record!");

            Record record(record_length, buf - 5);
            ++record_count;
            if (record.size() > max_record_size)
                max_record_size = record.size();
            if (record.getNumberOfFields() > max_field_count)
                max_field_count = record.getNumberOfFields();
        }

        std::cerr << "Read " << record_count << " records.\n";
        std::cerr << "The largest record contains " << max_record_size << " bytes.\n";
        std::cerr << "The record with the largest number of fields contains " << max_field_count << " fields.\n";
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
