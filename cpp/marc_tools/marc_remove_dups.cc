/** \file    marc_remove_dups.cc
 *  \brief   Drop records having the same control numbers.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <unordered_set>
#include <cstring>
#include "Compiler.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--use-checksums] marc_input1 [marc_input2 marc_inputN] marc_output\n";
    std::cerr << "       If --use-checksums has been specifierd only records with duplicate control numbers and\n";
    std::cerr << "       checksums will be dropped\n\n";
    std::exit(EXIT_FAILURE);
}


struct ChecksumAndControlNumber {
    std::string checksum_, control_number_;

public:
    ChecksumAndControlNumber(const std::string &checksum, const std::string &control_number)
        : checksum_(checksum), control_number_(control_number) { }
    ChecksumAndControlNumber() = default;
    ChecksumAndControlNumber(const ChecksumAndControlNumber &rhs) = default;

    inline bool operator==(const ChecksumAndControlNumber &rhs) const { return control_number_ == rhs.control_number_; }
    inline bool operator<(const ChecksumAndControlNumber &rhs) const { return control_number_ < rhs.control_number_; }
};


} // unnamed namespace


namespace std {


template <>
struct hash<ChecksumAndControlNumber> {
    inline size_t operator()(const ChecksumAndControlNumber &checksum_and_control_number) const {
        return hash<std::string>()(checksum_and_control_number.control_number_);
    }
};


} // namespace std


namespace {


void DropDups(const bool use_checksums, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
              std::unordered_set<ChecksumAndControlNumber> * const previously_seen) {
    unsigned total_count(0), dropped_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++total_count;

        const std::string checksum(
            use_checksums ? CalcChecksum(record, /* excluded_fields = */ { "001" }, /* suppress_local_fields = */ false) : "");

        ChecksumAndControlNumber new_checksum_and_control_number(checksum, record.getControlNumber());
        const auto iter(previously_seen->find(new_checksum_and_control_number));
        previously_seen->emplace(new_checksum_and_control_number);
        if (iter != previously_seen->end()) {
            if (use_checksums) {
                if (iter->checksum_ == new_checksum_and_control_number.checksum_) {
                    ++dropped_count;
                    continue;
                }
            } else {
                ++dropped_count;
                continue;
            }
        }

        previously_seen->emplace(new_checksum_and_control_number);
        marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " records and dropped " + std::to_string(dropped_count) + " dups.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    bool use_checksums(false);
    if (std::strcmp("--use-checksums", argv[1]) == 0) {
        use_checksums = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    auto marc_writer(MARC::Writer::Factory(argv[argc - 1]));

    std::unordered_set<ChecksumAndControlNumber> previously_seen;
    for (int arg_no(1); arg_no < argc - 1; ++arg_no) {
        auto marc_reader(MARC::Reader::Factory(argv[arg_no]));
        DropDups(use_checksums, marc_reader.get(), marc_writer.get(), &previously_seen);
    }

    return EXIT_SUCCESS;
}
