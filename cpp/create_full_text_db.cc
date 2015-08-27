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
#include <sys/types.h>
#include <sys/wait.h>
#include "Downloader.h"
#include "ExecUtil.h"
#include "FileLocker.h"
#include "FileUtil.h"
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
    if (not std::fseek(output, 0, SEEK_END))
	Error("failed to seek to the end of \"" + output_filename + "\"!");
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


bool FoundAtLeastOneNonReviewLink(const std::vector<DirectoryEntry> &dir_entries,
				  const std::vector<std::string> &field_data)
{
    ssize_t _856_index(MarcUtil::GetFieldIndex(dir_entries, "856"));
    if (_856_index == -1)
	return false;

    const ssize_t dir_entry_count(static_cast<ssize_t>(dir_entries.size()));
    for (/* Empty! */; _856_index < dir_entry_count and dir_entries[_856_index].getTag() == "856"; ++_856_index) { 
	const Subfields subfields(field_data[_856_index]);
        const auto u_begin_end(subfields.getIterators('u'));
        if (u_begin_end.first == u_begin_end.second) // No subfield 'u'.
            continue;

	if (not IsProbablyAReview(subfields))
	    return true;
    }

    return false;
}


constexpr unsigned PROCESS_COUNT_HIGHWATER_MARK(10);
constexpr unsigned PROCESS_COUNT_LOWWATER_MARK(5);


// Returns the number of child processes that returned a non-zero exit code.
unsigned CleanUpZombies(const unsigned zombies_to_collect) {
    unsigned child_reported_failure_count(0);
    for (unsigned zombie_no(0); zombie_no < zombies_to_collect; ++zombie_no) {
	int exit_code;
	::wait(&exit_code);
	if (exit_code != 0)
	    ++child_reported_failure_count;
    }

    return child_reported_failure_count;
}


void ProcessRecords(const unsigned max_record_count, const unsigned skip_count, FILE * const input,
                    const std::string &input_filename, FILE * const output, const std::string &output_filename,
		    const std::string &db_filename)
{
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned total_record_count(0), spawn_count(0), active_child_count(0), child_reported_failure_count(0);
    long offset(0L), last_offset;

    const std::string UPDATE_FULL_TEXT_DB_PATH(ExecUtil::Which("update_full_text_db"));
    if (UPDATE_FULL_TEXT_DB_PATH.empty())
	Error("can't find \"update_full_text_db\" in our $PATH!");

    while (MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg)) {
	last_offset = offset;
	offset += leader->getRecordLength();

        if (total_record_count == max_record_count)
            break;
        ++total_record_count;
        if (total_record_count <= skip_count) {
	    
            continue;
	}

	if (not FoundAtLeastOneNonReviewLink(dir_entries, field_data)) {
	    FileLockedComposeAndWriteRecord(output, output_filename, dir_entries, field_data, leader);
	    continue;
	}

	ExecUtil::Exec(UPDATE_FULL_TEXT_DB_PATH,
		       { std::to_string(last_offset), input_filename, output_filename, db_filename },
		       /* new_stdout = */"", ExecUtil::ExecMode::DETACH);
	++active_child_count;
	++spawn_count;

	if (active_child_count > PROCESS_COUNT_HIGHWATER_MARK) {
	    child_reported_failure_count += CleanUpZombies(active_child_count - PROCESS_COUNT_LOWWATER_MARK);
	    active_child_count = PROCESS_COUNT_LOWWATER_MARK;
	}
    }

    // Wait for stragglers:
    child_reported_failure_count += CleanUpZombies(active_child_count);

    if (not err_msg.empty())
        Error(err_msg);
    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Spawned " << spawn_count << " subprocesses.\n";
    std::cerr << child_reported_failure_count << " children reported a failure!\n";

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

    const std::string db_filename(*argv);
    if (not FileUtil::MakeEmpty(db_filename))
	Error("can't create an empty \"" + db_filename + "\"!");

    try {
        ProcessRecords(max_record_count, skip_count, marc_input, marc_input_filename, marc_output,
		       marc_output_filename, db_filename);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
