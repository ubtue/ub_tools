/** \file    populate_in_tuebingen_available.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool that adds a new "SIG" field to a MARC record if there are UB or IFK call numbers in a record.
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

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
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


static FILE *output_ptr;
static unsigned in_tuebingen_available_count;


bool ProcessRecord(std::shared_ptr<Leader> &leader, std::vector<DirectoryEntry> * const dir_entries,
		   std::vector<std::string> * const field_data, std::string * const /*err_msg*/)
{
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    MarcUtil::FindAllLocalDataBlocks(*dir_entries, *field_data, &local_block_boundaries);

    std::string local_call_numbers;
    for (const auto &block_start_and_end : local_block_boundaries) {
	std::vector<size_t> _852_field_indices;
	if (MarcUtil::FindFieldsInLocalBlock("852", "??", block_start_and_end, *field_data, &_852_field_indices) == 0)
	    continue;
	for (const size_t _852_index : _852_field_indices) {
	    const Subfields subfields1((*field_data)[_852_index]);
	    const std::string a_subfield(subfields1.getFirstSubfieldValue('a'));
	    if (a_subfield == "DE-21" or a_subfield == "DE-21-110") {
		const std::string institution(a_subfield == "DE-21" ? "UB: " : "IFK: ");
		if (_852_index + 1 < block_start_and_end.second) {
		    const Subfields subfields2((*field_data)[_852_index + 1]);
		    const std::string c_subfield(subfields2.getFirstSubfieldValue('c'));
		    if (not c_subfield.empty()) {
			const std::string institution_and_call_number(institution +  c_subfield);
			if (local_call_numbers.empty())
			    local_call_numbers = institution_and_call_number;
			else
			    local_call_numbers += ", " + institution_and_call_number;
		    }
		}
		break;
	    }
	}
    }

    if (not local_call_numbers.empty()) {
	++in_tuebingen_available_count;
	MarcUtil::InsertField(local_call_numbers, "SIG", leader, dir_entries, field_data);
    }

    MarcUtil::ComposeAndWriteRecord(output_ptr, *dir_entries, *field_data, leader);
    return true;
}


void PopulateTheInTuebingenAvailableField(const bool verbose, FILE * const input, FILE * const output) {
    output_ptr = output;

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, ProcessRecord, &err_msg))
	Error("error while processing records: " + err_msg);

    if (verbose)
	std::cout << "Added a in-Tübingen-available field in " << in_tuebingen_available_count << " records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    bool verbose;
    if (argc == 3)
	verbose = false;
    else { // argc == 4
	if (std::strcmp(argv[1], "--verbose") != 0)
	    Usage();
	verbose = true;
    }

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    FILE * const marc_input(std::fopen(marc_input_filename.c_str(), "rbm"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    FILE * const marc_output(std::fopen(marc_output_filename.c_str(), "wb"));
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    PopulateTheInTuebingenAvailableField(verbose, marc_input, marc_output);

    std::fclose(marc_input);
    std::fclose(marc_output);
}
