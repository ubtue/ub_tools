/** \file    fix_article_biblio_levels.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for determing the patching up the biblilographic level of article records.
 *  Many, possibly all, article records that we get have an 'a' in leader position 7 instead of a 'b'.
 *  If the referenced parent is a serial this tool changes the 'a' to a 'b'.
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
#include <unordered_set>
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


static std::unordered_set<std::string> serial_control_numbers;


bool RecordSerialControlNumbers(std::shared_ptr<Leader> &leader, std::vector<DirectoryEntry> * const /*dir_entries*/,
				std::vector<std::string> * const field_data, std::string * const /*err_msg*/)
{
    if ((*leader)[7] == 's')
	serial_control_numbers.insert((*field_data)[0]);

    return true;
}


void CollectSerials(const bool verbose, FILE * const input) {

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, RecordSerialControlNumbers, &err_msg))
	Error("error while looking for serials: " + err_msg);

    if (verbose)
	std::cout << "Found " << serial_control_numbers.size() << " serial records.\n";
}


static FILE *output_ptr;
static unsigned patch_count;


// Changes the bibliographic level of a record from 'a' to 'b' (= serial component part) if the parent is a serial.
// Also writes all records to "output_ptr".
bool PatchUpArticle(std::shared_ptr<Leader> &leader, std::vector<DirectoryEntry> * const dir_entries,
		    std::vector<std::string> * const field_data, std::string * const /*err_msg*/)
{
    ssize_t _773_index;
    if ((*leader)[7] != 'a' or (_773_index = MarcUtil::GetFieldIndex(*dir_entries, "773")) == -1) {
	MarcUtil::ComposeAndWriteRecord(output_ptr, *dir_entries, *field_data, leader);
	return true;
    }

    const Subfields _773_subfields((*field_data)[_773_index]);
    if (not _773_subfields.hasSubfield('w')) {
	MarcUtil::ComposeAndWriteRecord(output_ptr, *dir_entries, *field_data, leader);
	return true;
    }

    const std::string _773w_contents(_773_subfields.getFirstSubfieldValue('w'));
    if (not StringUtil::StartsWith(_773w_contents, "(DE-576)")) {
	MarcUtil::ComposeAndWriteRecord(output_ptr, *dir_entries, *field_data, leader);
	return true;
    }

    const std::string parent_control_number(_773w_contents.substr(8));
    const auto iter(serial_control_numbers.find(parent_control_number));
    if (iter != serial_control_numbers.end()) {
	(*leader)[7] = 'b';
	++patch_count;
    }

    MarcUtil::ComposeAndWriteRecord(output_ptr, *dir_entries, *field_data, leader);

    return true;
}


void PatchUpSerialComponentParts(const bool verbose, FILE * const input, FILE * const output) {
    output_ptr = output;

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(input, PatchUpArticle, &err_msg))
	Error("error while patching up article records: " + err_msg);

    if (verbose)
	std::cout << "Fixed the bibliographic level of " << patch_count << " article records.\n";
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
    FILE *marc_input(std::fopen(marc_input_filename.c_str(), "rbm"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    FILE *marc_output(std::fopen(marc_output_filename.c_str(), "wb"));
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    CollectSerials(verbose, marc_input);

    std::rewind(marc_input);
    PatchUpSerialComponentParts(verbose, marc_input, marc_output);

    std::fclose(marc_input);
    std::fclose(marc_output);
}
