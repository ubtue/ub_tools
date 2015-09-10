/** \file    classify_leader07a.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for determing the type of object that has a lowercase A in position 07 of the leader.
 *  the create_child_refs.sh shell script.
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
#include <unordered_map>
#include <cstdlib>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input\n";
    std::exit(EXIT_FAILURE);
}


void ExtractBibliographicLevel(
    FILE * const input,
    std::unordered_map<std::string, char> * const control_number_to_bibliographic_level_map)
{
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg)) {
        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");
	(*control_number_to_bibliographic_level_map)[field_data[0]] = leader->getBibliographicLevel();
    }

    if (not err_msg.empty())
        Error(err_msg);
}


void DetermineObjectType(const bool verbose, FILE * const input,
			 const std::unordered_map<std::string, char> &control_number_to_bibliographic_level_map)
{
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned _07a_count(0), review_count(0), misclassified_count(0);
    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg)) {
        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");
	const std::string &control_number(field_data[0]);

	if (leader->getBibliographicLevel() != 'a')
	    continue;
	++_07a_count;

	bool is_a_review(false);
	ssize_t _935_index(MarcUtil::GetFieldIndex(dir_entries, "935"));
	while (_935_index != -1) {
	    const Subfields _935_subfields(field_data[_935_index]);
	    if (_935_subfields.getFirstSubfieldValue('c') == "uwre") {
		is_a_review = true;
		++review_count;
		break;
	    }

	    // Advance to the next field if it is also a 935-field:
	    if (_935_index == static_cast<ssize_t>(field_data.size() - 1)
		or dir_entries[_935_index + 1].getTag() != "935")
		_935_index = -1;
	    else
		++_935_index;
	}
	if (is_a_review) {
	    if (verbose)
		std::cout << control_number << " review\n";
	    continue;
	}

	//
	// If we get here we might assume that we have an article.
	//

	const ssize_t _773_index(MarcUtil::GetFieldIndex(dir_entries, "773"));
	if (_773_index == -1) {
	    if (verbose)
		std::cout << control_number << " missing field 773\n";
	    ++misclassified_count;
	    continue;
	}
	const Subfields _773_subfields(field_data[_773_index]);
	const std::string _773w_contents(_773_subfields.getFirstSubfieldValue('w'));
	if (_773w_contents.empty() or not StringUtil::StartsWith(_773w_contents, "(DE-576)")) {
	    if (verbose)
		std::cout << control_number << " 773$w is missing or empty\n";
	    ++misclassified_count;
	    continue;
	}
	const std::string parent_control_number(_773w_contents.substr(8));
	const auto iter(control_number_to_bibliographic_level_map.find(parent_control_number));
	if (iter == control_number_to_bibliographic_level_map.end()) {
	    if (verbose)
		std::cout << control_number << " no parent found for control number "
			  << parent_control_number << '\n';
	    ++misclassified_count;
	    continue;
	}
	if (iter->second != 's' and iter->second != 'm') { // Neither a serial nor a monograph!
	    if (verbose)
		std::cout << control_number << " parent w/ control number " << parent_control_number
			  << " is neither a serial nor a monograph\n";
	    ++misclassified_count;
	    continue;
	}
    }

    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Found " << _07a_count << " entries with an 'a' in leader postion 07.\n";
    std::cerr << review_count << " records were reviews.\n";
    std::cerr << misclassified_count
	      << " records would be  classified as unknown if we used strategy 2.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    bool verbose;
    if (argc == 2)
	verbose = false;
    else { // argc == 3
	if (std::strcmp(argv[1], "--verbose") != 0)
	    Usage();
	verbose = true;
    }

    const std::string marc_input_filename(argv[1]);
    FILE *marc_input(std::fopen(marc_input_filename.c_str(), "rb"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    std::unordered_map<std::string, char> control_number_to_bibliographic_level_map;
    ExtractBibliographicLevel(marc_input, &control_number_to_bibliographic_level_map);

    std::rewind(marc_input);
    DetermineObjectType(verbose, marc_input, control_number_to_bibliographic_level_map);

    std::fclose(marc_input);
}
