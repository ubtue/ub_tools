/** \file    update_ixtheo_notations.cc
 *  \brief   Move the ixTheo classification notations from local data into field 652a.
 *  \author  Dr. Johannes Ruscheinski
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
#include <memory>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " marc_input marc_output code_to_description_map\n";
    std::exit(EXIT_FAILURE);
}


void LoadCodeToDescriptionMap(const std::shared_ptr<FILE> &code_to_description_map_file,
			      const std::string &code_to_description_map_filename,
			      std::unordered_map<std::string, std::string> * const code_to_description_map)
{
    char line[1024];
    unsigned line_no(0);
    while (std::fgets(line, sizeof line, code_to_description_map_file.get())) {
	++line_no;
	size_t line_length(std::strlen(line));
	if (line_length < 5) // Need at least a 2 character code, a comma, some text and a newline at the end.
	    continue;

	// Zap the newline at the end:
	line[line_length] = '\0';
	--line_length;

	char *line_end(line + line_length);
	char *comma(std::find(line, line_end, ','));
	if (comma == line_end)
	    Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_filename
		  + "\"! (1)");

	*comma = '\0';
	const std::string code(line);
	if (code.length() != 2 and code.length() != 3)
	    Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_filename
		  + "\"! (2)");

	const std::string  description(comma + 1);
	(*code_to_description_map)[code] = description;
    }

    std::cerr << "Found " << code_to_description_map->size() << " code to description mappings.\n";
}


bool LocalBlockIsFromUbTueTheologians(const std::pair<size_t, size_t> &local_block_begin_and_end,
				      const std::vector<std::string> &field_data)
{
    std::vector<size_t> _852_indices;
    MarcUtil::FindFieldsInLocalBlock("852", "  ", local_block_begin_and_end, field_data, &_852_indices);

    for (const auto index : _852_indices) {
	const Subfields subfields(field_data[index]);
	if (subfields.hasSubfieldWithValue('a', "DE-21"))
	    return true;
    }

    return false;
}


unsigned ExtractIxTheoNotations(const std::pair<size_t, size_t> &local_block_begin_and_end,
				const std::vector<std::string> &field_data,
				const std::unordered_map<std::string, std::string> &code_to_description_map,
				std::string * const ixtheo_notations_list)
{
    std::vector<size_t> _936ln_indices;
    MarcUtil::FindFieldsInLocalBlock("936", "ln", local_block_begin_and_end, field_data, &_936ln_indices);

    size_t found_count(0);
    for (const auto index : _936ln_indices) {
	const Subfields subfields(field_data[index]);
	const std::string ixtheo_notation_candidate(subfields.getFirstSubfieldValue('a'));
	if (code_to_description_map.find(ixtheo_notation_candidate) != code_to_description_map.end()) {
	    ++found_count;
	    if (ixtheo_notations_list->empty())
		*ixtheo_notations_list = ixtheo_notation_candidate;
	    else
		*ixtheo_notations_list += ":" + ixtheo_notation_candidate;
	}
    }

    return found_count;
}


void ProcessRecords(const std::shared_ptr<FILE> &input, const std::shared_ptr<FILE> &output,
		    const std::unordered_map<std::string, std::string> &code_to_description_map)
{
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), ixtheo_notation_count(0), records_with_ixtheo_notations(0);

    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input.get(), leader, &dir_entries, &field_data, &err_msg)) {
        ++count;

        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

	std::vector<std::pair<size_t, size_t>> local_block_boundaries;
	if (MarcUtil::FindAllLocalDataBlocks(dir_entries, field_data, &local_block_boundaries) == 0) {
	    MarcUtil::ComposeAndWriteRecord(output.get(), dir_entries, field_data, leader);
	    continue;
	}

	std::string ixtheo_notations_list; // Colon-separated list of ixTheo notations.
	for (const auto &local_block_begin_and_end : local_block_boundaries) {
	    if (not LocalBlockIsFromUbTueTheologians(local_block_begin_and_end, field_data))
		continue;
	    const unsigned notation_count(ExtractIxTheoNotations(local_block_begin_and_end, field_data,
								 code_to_description_map, &ixtheo_notations_list));
	    if (ixtheo_notation_count > 0) {
		++records_with_ixtheo_notations;
		ixtheo_notation_count += notation_count;
	    }
	}

	if (not ixtheo_notations_list.empty()) // Insert a new 652 field w/ a $a subfield.
	    MarcUtil::InsertField("  ""\x1F""a" + ixtheo_notations_list, "652", leader, &dir_entries, &field_data);
	MarcUtil::ComposeAndWriteRecord(output.get(), dir_entries, field_data, leader);
    }
    
    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << records_with_ixtheo_notations << " records had ixTheo notations.\n";
    std::cerr << "Found " << ixtheo_notation_count << " ixTheo notations overall.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    std::shared_ptr<FILE> marc_input(std::fopen(marc_input_filename.c_str(), "rbm"), std::fclose);
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[2]);
    std::shared_ptr<FILE> marc_output(std::fopen(marc_output_filename.c_str(), "wb"), std::fclose);
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    const std::string code_to_description_map_filename(argv[3]);
    std::shared_ptr<FILE> code_to_description_map_file(std::fopen(code_to_description_map_filename.c_str(), "rbm"),
						       std::fclose);
    if (code_to_description_map_file == nullptr)
        Error("can't open \"" + code_to_description_map_filename + "\" for reading!");

    std::unordered_map<std::string, std::string> code_to_description_map;
    LoadCodeToDescriptionMap(code_to_description_map_file, code_to_description_map_filename,
                             &code_to_description_map);

    ProcessRecords(marc_input, marc_output, code_to_description_map);
}
