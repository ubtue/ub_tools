/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Leader.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(const bool verbose, const bool input_is_xml, File * const input) {
    std::string raw_record;
    unsigned record_count(0), max_record_length(0), max_local_block_count(0);
    std::unordered_set<std::string> control_numbers;
    std::map<MarcUtil::Record::RecordType, unsigned> record_types_and_counts;

    while (const MarcUtil::Record record =
           input_is_xml ? MarcUtil::Record::XmlFactory(input) : MarcUtil::Record::BinaryFactory(input))
    {
        ++record_count;

        const std::vector<std::string> &fields(record.getFields());
        if (unlikely(fields.empty()))
          Error("record #" + std::to_string(record_count) + " has zero fields!");
        const std::string &control_number(fields[0]);

        const MarcUtil::Record::RecordType record_type(record.getRecordType());
        ++record_types_and_counts[record_type];
        if (verbose and record_type == MarcUtil::Record::UNKNOWN)
            std::cerr << "Unknown record type '" << record.getLeader()[6] << "' for PPN " << control_number << ".\n";

        std::string err_msg;
        if (not input_is_xml and not record.recordSeemsCorrect(&err_msg))
            Error("record #" + std::to_string(record_count) + " is malformed: " + err_msg);

        if (control_numbers.find(control_number) != control_numbers.end())
            Error("found at least one duplicate control number: " + control_number);
        control_numbers.insert(control_number);

        const Leader &leader(record.getLeader());
        const unsigned record_length(leader.getRecordLength());
        if (record_length > max_record_length)
            max_record_length = record_length;

        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        const size_t local_block_count(record.findAllLocalDataBlocks(&local_block_boundaries));
        if (local_block_count > max_local_block_count)
            max_local_block_count = local_block_count;
    }

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
    std::cout << "Largest record contains " << max_record_length << " bytes.\n";
    std::cout << "The record with the largest number of \"local\" blocks has " << max_local_block_count
              << " local blocks.\n";
    std::cout << "Counted " << record_types_and_counts[MarcUtil::Record::BIBLIOGRAPHIC]
              << " bibliographic record(s), " << record_types_and_counts[MarcUtil::Record::AUTHORITY]
              << " classification record(s), " << record_types_and_counts[MarcUtil::Record::CLASSIFICATION]
              << " authority record(s), and " << record_types_and_counts[MarcUtil::Record::UNKNOWN]
              << " record(s) of unknown record type.\n";
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 2)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string media_type(MediaTypeUtil::GetFileMediaType(marc_input_filename));
    if (unlikely(media_type.empty()))
        Error("can't determine media type of \"" + marc_input_filename + "\"!");
    if (media_type != "application/xml" and media_type != "application/marc")
        Error("\"input_filename\" is neither XML nor MARC-21 data!");
    const bool input_is_xml(media_type == "application/xml");

    File marc_input(marc_input_filename, input_is_xml ? "r" : "rb");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    try {
        ProcessRecords(verbose, input_is_xml, &marc_input);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
