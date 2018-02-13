/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DnsUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MARC.h"
#include "Semaphore.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--process-count-low-and-high-watermarks low:high] marc_input marc_output\n"
              << "       --process-count-low-and-high-watermarks sets the maximum and minimum number of spawned\n"
              << "       child processes.  When we hit the high water mark we wait for child processes to exit\n"
              << "       until we reach the low watermark.\n\n";

    std::exit(EXIT_FAILURE);
}


// Checks subfields "3" and "z" to see if they start w/ "Rezension" or contain "Cover".
bool IsProbablyAReviewOrCover(const MARC::Subfields &subfields) {
    for (const auto &subfield_contents : subfields.extractSubfields("3z")) {
        if (StringUtil::StartsWith(subfield_contents, "Rezension") or subfield_contents == "Cover")
            return true;
    }

    return false;
}


bool FoundAtLeastOneNonReviewOrCoverLink(const MARC::Record &record, std::string * const first_non_review_link) {
    for (const auto &field : record.getTagRange("856")) {
        const MARC::Subfields subfields(field.getSubfields());
        if (field.getIndicator1() == '7' or not subfields.hasSubfield('u'))
            continue;

        if (not IsProbablyAReviewOrCover(subfields)) {
            *first_non_review_link = subfields.getFirstSubfieldWithCode('u');
            return true;
        }
    }

    return false;
}


void ProcessNoDownloadRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                              std::vector<std::pair<off_t, std::string>> * const download_record_offsets_and_urls)
{
    unsigned total_record_count(0);
    off_t record_start(marc_reader->tell());

    while (const MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        std::string first_non_review_link;
        const bool insert_in_cache(FoundAtLeastOneNonReviewOrCoverLink(record, &first_non_review_link)
                                   or (record.getSubfieldValues("856", 'u').empty()
                                       and not record.getSubfieldValues("520", 'a').empty()));
        if (insert_in_cache)
            download_record_offsets_and_urls->emplace_back(record_start, first_non_review_link);
        else {
            marc_writer->write(record);
            record_start = marc_reader->tell();
        }

        record_start = marc_reader->tell();
    }

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Wrote " << (total_record_count - download_record_offsets_and_urls->size())
              << " records that did not require any downloads.\n";
}


// Returns the number of child processes that returned a non-zero exit code.
unsigned CleanUpZombies(const unsigned no_of_zombies_to_collect,
                        std::map<std::string, unsigned> * const hostname_to_outstanding_request_count_map,
                        std::map<int, std::string> * const process_id_to_hostname_map)
{
    unsigned child_reported_failure_count(0);
    for (unsigned zombie_no(0); zombie_no < no_of_zombies_to_collect; ++zombie_no) {
        int exit_code;
        const pid_t zombie_pid(::wait(&exit_code));
        if (exit_code != 0)
            ++child_reported_failure_count;

        const auto process_id_and_hostname(process_id_to_hostname_map->find(zombie_pid));
        if (process_id_and_hostname == process_id_to_hostname_map->end())
            ERROR("This should never happen!");

        if (--(*hostname_to_outstanding_request_count_map)[process_id_and_hostname->second] == 0)
            hostname_to_outstanding_request_count_map->erase(process_id_and_hostname->second);
        process_id_to_hostname_map->erase(process_id_and_hostname);
    }

    return child_reported_failure_count;
}


void ProcessDownloadRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                            const std::vector<std::pair<off_t, std::string>> &download_record_offsets_and_urls,
                            const unsigned process_count_low_watermark, const unsigned process_count_high_watermark)
{
    Semaphore semaphore("/full_text_cached_counter", Semaphore::CREATE);
    unsigned total_record_count(0), spawn_count(0), active_child_count(0), child_reported_failure_count(0);

    const std::string UPDATE_FULL_TEXT_DB_PATH("/usr/local/bin/update_full_text_db");
    const unsigned MAX_CONCURRENT_DOWNLOADS_PER_SERVER(1);
    std::map<std::string, unsigned> hostname_to_outstanding_request_count_map;
    std::map<int, std::string> process_id_to_hostname_map;
    
    for (const auto &offset_and_url : download_record_offsets_and_urls) {
        ++total_record_count;
        
        std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
        if (not UrlUtil::ParseUrl(offset_and_url.second, &scheme, &username_password, &authority, &port, &path, &params,
                                  &query, &fragment, &relative_url))
        {
            WARNING("failed to parse URL: " + offset_and_url.second);

            // Safely append the MARC data to the MARC output file:
            if (unlikely(not marc_reader->seek(offset_and_url.first)))
                ERROR("seek failed!");
            const MARC::Record record = marc_reader->read();
            MARC::FileLockedComposeAndWriteRecord(marc_writer, record);

            continue;
        }

        for (;;) {
            auto hostname_and_count(hostname_to_outstanding_request_count_map.find(authority));
            if (hostname_and_count == hostname_to_outstanding_request_count_map.end()) {
                hostname_to_outstanding_request_count_map[authority] = 1;
                break;
            } else if (hostname_and_count->second < MAX_CONCURRENT_DOWNLOADS_PER_SERVER) {
                ++hostname_and_count->second;
                break;
            }
            
            child_reported_failure_count += CleanUpZombies(/*no_of_zombies*/ 1, &hostname_to_outstanding_request_count_map,
                                                           &process_id_to_hostname_map);
            --active_child_count;

            ::sleep(5 /* seconds */);
        }
        
        const int child_pid(ExecUtil::Spawn(UPDATE_FULL_TEXT_DB_PATH,
                                            { std::to_string(offset_and_url.first), marc_reader->getPath(),
                                              marc_writer->getFile().getPath() }));
        process_id_to_hostname_map[child_pid] = authority;
        
        ++active_child_count;
        ++spawn_count;

        if (active_child_count > process_count_high_watermark) {
            child_reported_failure_count += CleanUpZombies(active_child_count - process_count_low_watermark,
                                                           &hostname_to_outstanding_request_count_map,
                                                           &process_id_to_hostname_map);
            active_child_count = process_count_low_watermark;
        }
    }

    // Wait for stragglers:
    child_reported_failure_count += CleanUpZombies(active_child_count, &hostname_to_outstanding_request_count_map,
                                                   &process_id_to_hostname_map);

    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Spawned " << spawn_count << " subprocesses.\n";
    std::cerr << semaphore.getValue()
              << " documents were not downloaded because their cached values had not yet expired.\n";
    std::cerr << child_reported_failure_count << " children reported a failure!\n";
}


constexpr unsigned PROCESS_COUNT_DEFAULT_HIGH_WATERMARK(10);
constexpr unsigned PROCESS_COUNT_DEFAULT_LOW_WATERMARK(5);


} // unnamed namespace


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3 and argc != 5 and argc != 7 and argc != 9)
        Usage();
    ++argv; // skip program name

    // Process optional args:
    unsigned process_count_low_watermark(PROCESS_COUNT_DEFAULT_LOW_WATERMARK),
             process_count_high_watermark(PROCESS_COUNT_DEFAULT_HIGH_WATERMARK);
    while (argc > 4) {
        std::cout << "Arg: " << *argv << "\n";
        if (std::strcmp("--process-count-low-and-high-watermarks", *argv) == 0) {
            ++argv;
            char *arg_end(*argv + std::strlen(*argv));
            char * const colon(std::find(*argv, arg_end, ':'));
            if (colon == arg_end)
                ERROR("bad argument to --process-count-low-and-high-watermarks: colon is missing!");
            *colon = '\0';
            if (not StringUtil::ToNumber(*argv, &process_count_low_watermark)
                or not StringUtil::ToNumber(*argv, &process_count_high_watermark))
                EERROR("low or high watermark is not an unsigned number!");
            if (process_count_high_watermark > process_count_low_watermark)
                ERROR("the high watermark must be larger than the low watermark!");
            ++argv;
            argc -= 2;
        } else
            logger->error("unknown flag: " + std::string(*argv));
    }

    const std::string marc_input_filename(*argv++);
    const std::string marc_output_filename(*argv++);
    if (marc_input_filename == marc_output_filename)
        ERROR("input filename must not equal output filename!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::Reader::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::Writer::BINARY));

    try {
        std::vector<std::pair<off_t, std::string>> download_record_offsets_and_urls;
        ProcessNoDownloadRecords(marc_reader.get(), marc_writer.get(), &download_record_offsets_and_urls);

        // Try to prevent clumps of URL's from the same server:
        std::random_shuffle(download_record_offsets_and_urls.begin(), download_record_offsets_and_urls.end());

        ProcessDownloadRecords(marc_reader.get(), marc_writer.get(), download_record_offsets_and_urls,
                               process_count_low_watermark, process_count_high_watermark);
    } catch (const std::exception &e) {
        ERROR("Caught exception: " + std::string(e.what()));
    }
}
