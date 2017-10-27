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
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(const bool verbose, MarcReader * const marc_reader) {
    std::string raw_record;
    unsigned record_count(0), max_record_length(0), max_local_block_count(0), oversized_record_count(0),
        max_subfield_count(0), cumulative_field_count(0);
    std::unordered_set<std::string> control_numbers;
    std::map<Leader::RecordType, unsigned> record_types_and_counts;

    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;
        cumulative_field_count += record.getNumberOfFields();
        
        if (unlikely(record.getNumberOfFields() == 0))
            logger->error("record #" + std::to_string(record_count) + " has zero fields!");
        const std::string &control_number(record.getControlNumber());

        if (control_numbers.find(control_number) != control_numbers.end())
            logger->warning("found at least one duplicate control number: " + control_number);
        control_numbers.insert(control_number);

        const Leader::RecordType record_type(record.getRecordType());
        ++record_types_and_counts[record_type];
        if (verbose and record_type == Leader::RecordType::UNKNOWN)
            std::cerr << "Unknown record type '" << record.getLeader()[6] << "' for PPN " << control_number
                      << ".\n";

        const Leader &leader(record.getLeader());
        const unsigned record_length(leader.getRecordLength());
        if (record_length > max_record_length)
            max_record_length = record_length;
        if (record_length >= 100000)
            ++oversized_record_count;

        for (unsigned i(0); i < record.getNumberOfFields(); ++i) {
            if (record.isControlField(i))
                continue;

            const Subfields subfields(record.getFieldData(i));
            const size_t subfield_count(subfields.size());
            if (unlikely(subfield_count > max_subfield_count))
                max_subfield_count = subfield_count;
        }

        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        const size_t local_block_count(record.findAllLocalDataBlocks(&local_block_boundaries));
        if (local_block_count > max_local_block_count)
            max_local_block_count = local_block_count;
        for (const auto local_block_boundary : local_block_boundaries) {
            std::vector<size_t> field_indices;
            record.findFieldsInLocalBlock("001", "??", local_block_boundary, &field_indices);
            if (field_indices.size() != 1)
                logger->error("Every local data block has to have exactly one 001 field. (Record: "
                              + record.getControlNumber() + ", Local data block: "
                              + std::to_string(local_block_boundary.first) + " - "
                              + std::to_string(local_block_boundary.second));
        }
    }

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
    std::cout << "Largest record contains " << max_record_length << " bytes.\n";
    std::cout << "The record with the largest number of \"local\" blocks has " << max_local_block_count
              << " local blocks.\n";
    std::cout << "Counted " << record_types_and_counts[Leader::RecordType::BIBLIOGRAPHIC]
              << " bibliographic record(s), " << record_types_and_counts[Leader::RecordType::AUTHORITY]
              << " classification record(s), " << record_types_and_counts[Leader::RecordType::CLASSIFICATION]
              << " authority record(s), and " << record_types_and_counts[Leader::RecordType::UNKNOWN]
              << " record(s) of unknown record type.\n";
    std::cout << "Found " << oversized_record_count << " oversized records.\n";
    std::cout << "The field with the most subfields has " << max_subfield_count << " subfield(s).\n";
    std::cout << "The average no. of fields per record is "
              << (static_cast<double>(cumulative_field_count) / record_count) << ".\n";
    std::cout << "The average record size in bytes is "
              << (static_cast<double>(FileUtil::GetFileSize(marc_reader->getPath())) / record_count) << ".\n";
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

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        ProcessRecords(verbose, marc_reader.get());
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
