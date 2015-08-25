/** \file    ixtheo_notation_stats.cc
 *  \brief   Gather statistics about the local ixTheo classification scheme.
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
    std::cerr << "Usage: " << progname << "marc_input code_to_description_map\n";
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
	const size_t line_length(std::strlen(line));
	if (line_length < 7) // 4 double quotes + 1 comma + two strings of length 1
	    continue;

	char *end(line + line_length);
	char *quote(std::find(line + 1, end, '"'));
	if (quote == end)
	    Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_filename
		  + "\" (1)!");
	*quote = '\0';
	const std::string code(line + 1);
	const char *description_start(quote + 2);
	if (*description_start != '"')
	    Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_filename
		  + "\" (2)!");
	if (line[line_length - 2] != '"')
	    Error("malformed line " + std::to_string(line_no) + " in \"" + code_to_description_map_filename
		  + "\" (3)!");
	line[line_length - 2] = '\0';
	const std::string  description(description_start + 1);
	(*code_to_description_map)[code] = description;
    }

    std::cerr << "Found " << code_to_description_map->size() << " code to description mappings.\n";
}


void CollectCounts(const std::shared_ptr<FILE> &input,
		   const std::unordered_map<std::string, std::string> &code_to_description_map,
		   std::unordered_map<std::string, unsigned> * const categories_to_counts_map)
{
    categories_to_counts_map->clear();

    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), ixtheo_notation_count(0), records_with_ixtheo_notations(0);

    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input.get(), leader, &dir_entries, &field_data, &err_msg)) {
        ++count;

        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

	int lok_index(MarcUtil::GetFieldIndex(dir_entries, "LOK"));
	bool found_at_least_one(false);
	while (lok_index != -1) {
	    const Subfields subfields(field_data[lok_index]);
	    const auto begin_end(subfields.getIterators('0'));
	    if (std::distance(begin_end.first, begin_end.second) == 2 and begin_end.first->second == "936ln") {
		const std::string ixtheo_notation(subfields.getFirstSubfieldValue('a'));
		if (not ixtheo_notation.empty()
		    and code_to_description_map.find(ixtheo_notation) != code_to_description_map.end())
		    {
			found_at_least_one = true;
			std::cout << field_data[0] << ": " << ixtheo_notation << '\n';
			++ixtheo_notation_count;
		    }
	    }

	    // Continue loop if next tag is "LOK":
	    if (lok_index == static_cast<ssize_t>(dir_entries.size()) - 1
		or (dir_entries[++lok_index].getTag() != "LOK"))
		lok_index = -1;
	}

	if (found_at_least_one)
	    ++records_with_ixtheo_notations;
    }
    
    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << records_with_ixtheo_notations << " records had ixTheo notations.\n";
    std::cerr << "Found " << ixtheo_notation_count << " ixTheo notations overall.\n";
}


void CloseFile(FILE * const file) {
    std::fclose(file);
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    std::shared_ptr<FILE> marc_input(std::fopen(marc_input_filename.c_str(), "rm"), CloseFile);
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string code_to_description_map_filename(argv[2]);
    std::shared_ptr<FILE> code_to_description_map_file(std::fopen(code_to_description_map_filename.c_str(), "rm"),
						       CloseFile);
    if (code_to_description_map_file == nullptr)
        Error("can't open \"" + code_to_description_map_filename + "\" for reading!");

    std::unordered_map<std::string, std::string> code_to_description_map;
    LoadCodeToDescriptionMap(code_to_description_map_file, code_to_description_map_filename,
                             &code_to_description_map);

    std::unordered_map<std::string, unsigned> categories_to_counts_map;
    CollectCounts(marc_input, code_to_description_map, &categories_to_counts_map);
}
