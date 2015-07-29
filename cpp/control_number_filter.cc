/** \file    control_number_filter.cc
 *  \brief   A tool for filtering MARC-21 data sets based on patterns for control numbers.
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
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << "(--keep|--delete)] pattern marc_input marc_output\n";
    std::cerr << "  Removes records whose control numbers match \"pattern\" if \"--delete\" has been specified\n";
    std::cerr << "  or only keeps those records whose control numbers match \"pattern\" if \"keep\" has \n";
    std::cerr << "  been specified.  (\"pattern\" must be a PCRE.)\n";
    std::exit(EXIT_FAILURE);
}


void FilterMarcRecords(const bool keep, const std::string &regex_pattern, FILE * const input, FILE * const output) {
    std::string err_msg;
    const RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory(regex_pattern, &err_msg));
    if (matcher == NULL)
	Error("Failed to compile pattern \"" + regex_pattern + "\": " + err_msg);

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), kept_or_deleted_count(0);

    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++count;
        std::unique_ptr<Leader> leader(raw_leader);
        if (not leader->isArticle()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

	const bool matched(matcher->matched(field_data[0], &err_msg));
	if (not err_msg.empty())
	    Error("regex matching error: " + err_msg);

	if ((keep and matched) or (not keep and not matched)) {
	    ++kept_or_deleted_count;
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	}
    }
    
    if (not err_msg.empty())
        Error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << (keep ? "Kept" : "Deleted") << kept_or_deleted_count << " record(s).\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 5)
        Usage();
    if (std::strcmp(argv[1], "--keep") != 0 and std::strcmp(argv[1], "--delete") != 0)
	Usage();
    const bool keep(std::strcmp(argv[1], "--keep") == 0);
    const std::string regex_pattern(argv[2]);

    const std::string marc_input_filename(argv[3]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rm");
    if (marc_input == NULL)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[4]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");

    FilterMarcRecords(keep, regex_pattern, marc_input, marc_output);

    std::fclose(marc_input);
    std::fclose(marc_output);
}
