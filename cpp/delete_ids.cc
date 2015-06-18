/** \brief Utility for deleting MARC records based on an input list.
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
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "MarcUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << progname << " deletion_list input_marc output_marc\n";
    std::exit(EXIT_FAILURE);
}


void ExtractDeletionIds(FILE * const deletion_list, std::unordered_set<std::string> * const deletion_ids) {
    char line[100];
    while (std::fgets(line, sizeof(line), deletion_list) != NULL) {
	if (std::strlen(line) < 13)
	    Error("short line in deletion list file: \"" + std::string(line) + "\"!");
	if (line[11] == 'A')
	    deletion_ids->insert(line + 12);
    }

    std::fclose(deletion_list);
}


void ProcessRecords(const std::unordered_set<std::string> &deletion_ids, FILE * const input, FILE * const output) {
    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned total_record_count(0), deleted_record_count(0);

    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++total_record_count;

	if (dir_entries[0].getTag() != "001")
	    Error("First field is not \"001\"!");

	if (deletion_ids.find(field_data[0]) != deletion_ids.end())
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, raw_leader);
	else {
	    ++deleted_record_count;
	    std::cout << "Deleted record with ID " << field_data[0] << '\n';
	}
    }

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Deleted " << deleted_record_count << " records.\n";

    std::fclose(input);
    std::fclose(output);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 4)
	Usage();

    const std::string deletion_list_filename(argv[1]);
    FILE *deletion_list = std::fopen(deletion_list_filename.c_str(), "rb");
    if (deletion_list == NULL)
	Error("can't open \"" + deletion_list_filename + "\" for reading!");

    std::unordered_set<std::string> deletion_ids;
    ExtractDeletionIds(deletion_list, &deletion_ids);

    const std::string marc_input_filename(argv[2]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rb");
    if (marc_input == NULL)
	Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[3]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
	Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
	ProcessRecords(deletion_ids, marc_input, marc_output);
    } catch (const std::exception &e) {
	Error("Caught exception: " + std::string(e.what()));
    }
}
