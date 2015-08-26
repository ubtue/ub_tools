/** \brief Utility for augmenting MARC records with links to a local full-text database.
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
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <libgen.h>
#include <strings.h>
#include "Downloader.h"
#include "ExecUtil.h"
#include "FileLocker.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TimeLimit.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname
	      << "[--max-record-count count] [--skip-count count] marc_input marc_output full_text_db\n\n";

    std::exit(EXIT_FAILURE);
}


void FileLockedComposeAndWriteRecord(FILE * const output, const std::string &output_filename,
				     const std::vector<DirectoryEntry> &dir_entries,
                                     const std::vector<std::string> &field_data, std::shared_ptr<Leader> leader)
{
    FileLocker file_locker(output_filename, FileLocker::WRITE_ONLY);
    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader);
}


// Checks subfields "3" and "z" to see if they start w/ "Rezension".
bool IsProbablyAReview(const Subfields &subfields) {
    const auto _3_begin_end(subfields.getIterators('3'));
    if (_3_begin_end.first != _3_begin_end.second) {
        if (StringUtil::StartsWith(_3_begin_end.first->second, "Rezension"))
            return true;
    } else {
        const auto z_begin_end(subfields.getIterators('z'));
        if (z_begin_end.first != z_begin_end.second
            and StringUtil::StartsWith(z_begin_end.first->second, "Rezension"))
            return true;
    }

    return false;
}


void ProcessRecords(const unsigned max_record_count, const unsigned skip_count, FILE * const input,
                    FILE * const output, const std::string &output_filename, const std::string &/*db_filename*/)
{
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned total_record_count(0);
    long offset(0L), last_offset;

    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg)) {
	last_offset = offset;
	offset += leader->getRecordLength();

        if (total_record_count == max_record_count)
            break;
        ++total_record_count;
        if (total_record_count <= skip_count) {
	    
            continue;
	}

	std::cout << "Processing record #" << total_record_count << ".\n";

        const ssize_t _856_index(MarcUtil::GetFieldIndex(dir_entries, "856"));
        if (_856_index == -1) {
            FileLockedComposeAndWriteRecord(output, output_filename, dir_entries, field_data, leader);
            continue;
        }

	const Subfields subfields(field_data[_856_index]);
	if (IsProbablyAReview(subfields)){
            FileLockedComposeAndWriteRecord(output, output_filename, dir_entries, field_data, leader);
            continue;
        }

	std::cout << "file offset: " << last_offset << '\n';
    }

    if (not err_msg.empty())
        Error(err_msg);
    std::cerr << "Read " << total_record_count << " records.\n";

    std::fclose(input);
    std::fclose(output);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4 and argc != 6 and argc != 8)
        Usage();
    ++argv; // skip program name

    // Process optional args:
    unsigned max_record_count(UINT_MAX), skip_count(0);
    while (argc > 4) {
	if (std::strcmp(*argv, "--max-record-count") == 0) {
	    ++argv;
	    if (not StringUtil::ToNumber(*argv, &max_record_count) or max_record_count == 0)
		Error("bad value for --max-record-count!");
	    argc -= 2;
	} else if (std::strcmp(*argv, "--skip-count") == 0) {
	    ++argv;
	    if (not StringUtil::ToNumber(*argv, &skip_count))
		Error("bad value for --skip-count!");
	    argc -= 2;
	} else
	    Error("unknown flag: " + std::string(*argv));
    }

    const std::string marc_input_filename(*argv++);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rb");
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(*argv++);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        ProcessRecords(max_record_count, skip_count, marc_input, marc_output, marc_output_filename, *argv);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
