/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <kchashdb.h>
#include "Downloader.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "Semaphore.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TimeLimit.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname
              << "[--max-record-count count] [--skip-count count] [--process-count-low-and-high-watermarks low:high] "
              << "marc_input marc_output full_text_db\n"
              << "       --process-count-low-and-high-watermarks sets the maximum and minimum number of spawned\n"
              << "       child processes.  When we hit the high water mark we wait for child processes to exit\n"
              << "       until we reach the low watermark.\n\n";

    std::exit(EXIT_FAILURE);
}


// Checks subfields "3" and "z" to see if they start w/ "Rezension".
bool IsProbablyAReview(const Subfields &subfields) {
    const auto &_3_begin_end(subfields.getIterators('3'));
    if (_3_begin_end.first != _3_begin_end.second) {
        if (StringUtil::StartsWith(_3_begin_end.first->value_, "Rezension"))
            return true;
    } else {
        const auto &z_begin_end(subfields.getIterators('z'));
        if (z_begin_end.first != z_begin_end.second
            and StringUtil::StartsWith(z_begin_end.first->value_, "Rezension"))
            return true;
    }

    return false;
}


bool FoundAtLeastOneNonReviewLink(const MarcRecord &record) {
    for (size_t _856_index(record.getFieldIndex("856")); record.getTag(_856_index) == "856"; ++_856_index) {
        const Subfields subfields(record.getSubfields(_856_index));
        if (subfields.getIndicator1() == '7' or not subfields.hasSubfield('u'))
            continue;

        if (not IsProbablyAReview(subfields))
            return true;
    }

    return false;
}


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


void ProcessRecords(const unsigned max_record_count, const unsigned skip_count, MarcReader * const marc_reader,
                    MarcWriter * const marc_writer, const std::string &db_filename,
                    const unsigned process_count_low_watermark, const unsigned process_count_high_watermark)
{
    Semaphore semaphore("/full_text_cached_counter", Semaphore::CREATE);
    std::string err_msg;
    unsigned total_record_count(0), spawn_count(0), active_child_count(0), child_reported_failure_count(0);

    const std::string UPDATE_FULL_TEXT_DB_PATH("/usr/local/bin/update_full_text_db");

    std::cout << "Skip " << skip_count << " records\n";
    off_t record_start = marc_reader->tell();
    while (MarcRecord record = marc_reader->read()) {
        if (total_record_count == max_record_count)
            break;
        ++total_record_count;
        if (total_record_count <= skip_count) {
            record_start = marc_reader->tell();
            continue;
        }

        const bool insert_in_cache(FoundAtLeastOneNonReviewLink(record)
                                   or (not MarcUtil::HasTagAndSubfield(record, "856", 'u')
                                       and MarcUtil::HasTagAndSubfield(record, "520", 'a')));
        if (not insert_in_cache) {
            MarcUtil::FileLockedComposeAndWriteRecord(marc_writer, &record);
            record_start = marc_reader->tell();
            continue;
        }

        ExecUtil::Spawn(UPDATE_FULL_TEXT_DB_PATH,
                        { std::to_string(record_start), marc_reader->getPath(), marc_writer->getFile().getPath(),
                          db_filename });
        ++active_child_count;
        ++spawn_count;

        if (active_child_count > process_count_high_watermark) {
            child_reported_failure_count += CleanUpZombies(active_child_count - process_count_low_watermark);
            active_child_count = process_count_low_watermark;
        }
        record_start = marc_reader->tell();
    }

    // Wait for stragglers:
    child_reported_failure_count += CleanUpZombies(active_child_count);

    if (not err_msg.empty())
        logger->error(err_msg);
    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Spawned " << spawn_count << " subprocesses.\n";
    std::cerr << semaphore.getValue()
              << " documents were not downloaded because their cached values had not yet expired.\n";
    std::cerr << child_reported_failure_count << " children reported a failure!\n";
}


constexpr unsigned PROCESS_COUNT_DEFAULT_HIGH_WATERMARK(10);
constexpr unsigned PROCESS_COUNT_DEFAULT_LOW_WATERMARK(5);


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4 and argc != 6 and argc != 8 and argc != 10)
        Usage();
    ++argv; // skip program name

    // Process optional args:
    unsigned max_record_count(UINT_MAX), skip_count(0);
    unsigned process_count_low_watermark(PROCESS_COUNT_DEFAULT_LOW_WATERMARK),
             process_count_high_watermark(PROCESS_COUNT_DEFAULT_HIGH_WATERMARK);
    while (argc > 5) {
        std::cout << "Arg: " << *argv << "\n";
        if (std::strcmp(*argv, "--max-record-count") == 0) {
            ++argv;
            if (not StringUtil::ToNumber(*argv, &max_record_count) or max_record_count == 0)
                logger->error("bad value for --max-record-count!");
            ++argv;
            argc -= 2;
        } else if (std::strcmp(*argv, "--skip-count") == 0) {
            ++argv;
            if (not StringUtil::ToNumber(*argv, &skip_count))
                logger->error("bad value for --skip-count!");
            std::cout << "Should skip " << skip_count << " records\n";
            ++argv;
            argc -= 2;
        } else if (std::strcmp("--process-count-low-and-high-watermarks", *argv) == 0) {
            ++argv;
            char *arg_end(*argv + std::strlen(*argv));
            char * const colon(std::find(*argv, arg_end, ':'));
            if (colon == arg_end)
                logger->error("bad argument to --process-count-low-and-high-watermarks: colon is missing!");
            *colon = '\0';
            if (not StringUtil::ToNumber(*argv, &process_count_low_watermark)
                or not StringUtil::ToNumber(*argv, &process_count_high_watermark))
                logger->error("low or high watermark is not an unsigned number!");
            if (process_count_high_watermark > process_count_low_watermark)
                logger->error("the high watermark must be larger than the low watermark!");
            ++argv;
            argc -= 2;
        } else
            logger->error("unknown flag: " + std::string(*argv));
    }

    const std::string marc_input_filename(*argv++);
    const std::string marc_output_filename(*argv++);
    if (marc_input_filename == marc_output_filename)
        logger->error("input filename must not equal output filename!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    const std::string db_filename(*argv++);
    kyotocabinet::HashDB db;
    if (not db.open(db_filename, kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OCREATE))
        logger->error("Failed to create the key/valuedatabase \"" + db_filename + "\" ("
                      + std::string(db.error().message()) + ")!");
    db.close();

    const std::string UPDATE_DB_LOG_DIR_PATH(
        "/var/log/" + std::string(FileUtil::Exists("/var/log/krimdok") ? "krimdok" : "ixtheo")
        + "/update_full_text_db");
    if (not FileUtil::MakeDirectory(UPDATE_DB_LOG_DIR_PATH, /* recursive = */ true))
        logger->error("failed to create directory: " + UPDATE_DB_LOG_DIR_PATH);

    try {
        ProcessRecords(max_record_count, skip_count, marc_reader.get(), marc_writer.get(), db_filename,
                       process_count_low_watermark, process_count_high_watermark);
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
