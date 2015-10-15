/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Leader.h"
#include "MarcUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(FILE * const input) {
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    std::string raw_record;
    unsigned record_count(0), max_record_length(0), max_local_block_count(0);

    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg, &raw_record)) {
        ++record_count;

	if (not MarcUtil::RecordSeemsCorrect(raw_record, &err_msg))
	    Error("record #" + std::to_string(record_count) + " is malformed: " + err_msg);

	const unsigned record_length(leader->getRecordLength());
	if (record_length > max_record_length)
	    max_record_length = record_length;

	std::vector<std::pair<size_t, size_t>> local_block_boundaries;
	const size_t local_block_count(
            MarcUtil::FindAllLocalDataBlocks(dir_entries, field_data, &local_block_boundaries));
	if (local_block_count > max_local_block_count)
	    max_local_block_count = local_block_count;
    }

    if (not err_msg.empty())
        Error(err_msg);

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
    std::cout << "Largest record contains " << max_record_length << " bytes.\n";
    std::cout << "The record with the largest number of \"local\" blocks has " << max_local_block_count
	      << " local blocks.\n";
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string marc_input_filename(argv[1]);
    FILE *marc_input(std::fopen(marc_input_filename.c_str(), "rb"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    try {
        ProcessRecords(marc_input);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }

    std::fclose(marc_input);
}
