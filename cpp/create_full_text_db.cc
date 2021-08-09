/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include "ExecUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "Semaphore.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


namespace {


constexpr unsigned DEFAULT_PDF_EXTRACTION_TIMEOUT = 120; // seconds


[[noreturn]] void Usage() {
    ::Usage("[--min-log-level=min_verbosity] [--process-count-low-and-high-watermarks low:high] [--pdf-extraction-timeout=timeout]\n"
            "[--only-open-access] [--store-pdfs-as-html] marc_input marc_output\n"
            "\"--process-count-low-and-high-watermarks\" sets the maximum and minimum number of spawned\n"
            "    child processes.  When we hit the high water mark we wait for child processes to exit\n"
            "    until we reach the low watermark.\n"
            "\"--pdf-extraction-timeout\" which has a default of " + std::to_string(DEFAULT_PDF_EXTRACTION_TIMEOUT) + "\n"
            "    seconds is the maximum amount of time spent by a subprocess in attemting text extraction from a\n"
            "    downloaded PDF document.\n"
            "\"--only-open-access\" means that only open access texts will be processed.\n"
            "\"--store-pdfs-as-html\" means that an HTML representation of downloaded PDF's is stored if possible.\n"
            "\"--use-separate-entries-per-url\": Store individual entries for the fulltext locations in a record\n"
            "\"--include-all-tocs\": Extract TOCs even if they are not matched by the only-open-access-filter\n"
            "\"--include-list-of-references\": Extract list of references\n"
            "\"--only-pdf-fulltexts\": Download real Fulltexts only if the link points to a PDF\n"
           );

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


void ProcessNoDownloadRecords(const bool only_open_access, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
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
        if (insert_in_cache and (not only_open_access or MARC::IsOpenAccess(record)))
            download_record_offsets_and_urls->emplace_back(record_start, first_non_review_link);
        else
            marc_writer->write(record);

        record_start = marc_reader->tell();
    }

    if (unlikely(not marc_writer->flush()))
        LOG_ERROR("flush to \"" + marc_writer->getFile().getPath() + "\" failed!");

    LOG_INFO("Read " + std::to_string(total_record_count) + " records.\n");
    LOG_INFO("Wrote " + std::to_string(total_record_count - download_record_offsets_and_urls->size()) +
             " records that did not require any downloads.\n");
}


// Returns the number of child processes that returned a non-zero exit code.
void CleanUpZombies(const unsigned no_of_zombies_to_collect,
                    std::map<std::string, unsigned> * const hostname_to_outstanding_request_count_map,
                    std::map<int, std::string> * const process_id_to_hostname_map,
                    std::map<int, off_t> * const process_id_to_record_start_map,
                    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    unsigned * const child_reported_failure_count, unsigned * const active_child_count)
{
    for (unsigned zombie_no(0); zombie_no < no_of_zombies_to_collect; ++zombie_no) {
        int exit_code;
        const pid_t zombie_pid(::wait(&exit_code));
        if (exit_code != 0) {
            const auto process_id_and_record_start(process_id_to_record_start_map->find(zombie_pid));
            if (process_id_and_record_start != process_id_to_record_start_map->end()) {
                LOG_WARNING("Child process for PID "+ std::to_string(zombie_pid) + " failed: Writing out record anyway");
                if (unlikely(not marc_reader->seek(process_id_and_record_start->second)))
                    LOG_ERROR("seek failed!");
                const MARC::Record record = marc_reader->read();
                MARC::FileLockedComposeAndWriteRecord(marc_writer, record);
            }
            ++*child_reported_failure_count;
        }
        --*active_child_count;

        const auto process_id_and_hostname(process_id_to_hostname_map->find(zombie_pid));
        if (unlikely(process_id_and_hostname == process_id_to_hostname_map->end()))
            LOG_ERROR("This should *never* happen!");

        if (--(*hostname_to_outstanding_request_count_map)[process_id_and_hostname->second] == 0)
            hostname_to_outstanding_request_count_map->erase(process_id_and_hostname->second);
        process_id_to_hostname_map->erase(process_id_and_hostname);
    }
}


const std::string UPDATE_FULL_TEXT_DB_PATH("/usr/local/bin/update_full_text_db");


void ScheduleSubprocess(const std::string &server_hostname, const off_t marc_record_start, const unsigned pdf_extraction_timeout,
                        const std::string &marc_input_filename, const std::string &marc_output_filename,
                        std::map<std::string, unsigned> * const hostname_to_outstanding_request_count_map,
                        std::map<int, std::string> * const process_id_to_hostname_map, std::map<int, off_t> * const process_id_to_record_start_map,
                        MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                        unsigned * const child_reported_failure_count, unsigned * const active_child_count,
                        const bool store_pdfs_as_html, const bool use_separate_entries_per_url,
                        const bool include_all_tocs, const bool include_list_of_references, const bool only_pdf_fulltexts)

{
    constexpr unsigned MAX_CONCURRENT_DOWNLOADS_PER_SERVER = 2;

    // Wait until we have a slot available for the server:
    for (;;) {
        auto hostname_and_count(hostname_to_outstanding_request_count_map->find(server_hostname));
        if (hostname_and_count == hostname_to_outstanding_request_count_map->end()) {
            (*hostname_to_outstanding_request_count_map)[server_hostname] = 1;
            break;
        } else if (server_hostname.empty() or hostname_and_count->second < MAX_CONCURRENT_DOWNLOADS_PER_SERVER) {
            ++hostname_and_count->second;
            break;
        }

        CleanUpZombies(/*no_of_zombies*/ 1, hostname_to_outstanding_request_count_map, process_id_to_hostname_map,
                       process_id_to_record_start_map, marc_reader, marc_writer,
                       child_reported_failure_count, active_child_count);
    }

    std::vector<std::string> args;
    args.emplace_back("--pdf-extraction-timeout=" + std::to_string(pdf_extraction_timeout));
    if (store_pdfs_as_html) {
        args.emplace_back("--use-only-open-access-documents");
        args.emplace_back("--store-pdfs-as-html");
    }
    if (use_separate_entries_per_url)
        args.emplace_back("--use-separate-entries-per-url");
    if (include_all_tocs)
        args.emplace_back("--include-all-tocs");
    if (include_list_of_references)
        args.emplace_back("--include-list-of-references");
    if (only_pdf_fulltexts)
        args.emplace_back("--only-pdf-fulltexts");
    args.emplace_back(std::to_string(marc_record_start));
    args.emplace_back(marc_input_filename);
    args.emplace_back(marc_output_filename);

    const int child_pid(ExecUtil::Spawn(UPDATE_FULL_TEXT_DB_PATH, args, "" /* no new stdin */,
                                        "" /* no new stdout */, "" /* no new stderr */,
                                        { std::pair("OMP_THREAD_LIMIT", "1") }));
    if (unlikely(child_pid == -1))
        LOG_ERROR("ExecUtil::Spawn failed! (no more resources?)");

    (*process_id_to_hostname_map)[child_pid] = server_hostname;
    (*process_id_to_record_start_map)[child_pid] = marc_record_start;
    ++*active_child_count;
}


void ProcessDownloadRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                            const unsigned pdf_extraction_timeout,
                            const std::vector<std::pair<off_t, std::string>> &download_record_offsets_and_urls,
                            const unsigned process_count_low_watermark, const unsigned process_count_high_watermark,
                            const bool store_pdfs_as_html,
                            const bool use_separate_entries_per_url,
                            const bool include_all_tocs,
                            const bool include_list_of_references,
                            const bool only_pdf_fulltexts)
{
    Semaphore semaphore("full_text_cached_counter", Semaphore::CREATE);
    unsigned active_child_count(0), child_reported_failure_count(0);

    std::map<std::string, unsigned> hostname_to_outstanding_request_count_map;
    std::map<int, std::string> process_id_to_hostname_map;
    std::map<int, off_t> process_id_to_record_start_map;

    for (const auto &offset_and_url : download_record_offsets_and_urls) {
        const std::string &url(offset_and_url.second);
        std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
        if (not url.empty()
            and not UrlUtil::ParseUrl(url, &scheme, &username_password, &authority, &port, &path, &params,
                                      &query, &fragment, &relative_url))
        {
            LOG_WARNING("failed to parse URL: " + url);

            // Safely append the MARC data to the MARC output file:
            if (unlikely(not marc_reader->seek(offset_and_url.first)))
                LOG_ERROR("seek failed!");
            const MARC::Record record = marc_reader->read();
            MARC::FileLockedComposeAndWriteRecord(marc_writer, record);

            continue;
        }

        ScheduleSubprocess(authority, offset_and_url.first, pdf_extraction_timeout, marc_reader->getPath(),
                           marc_writer->getFile().getPath(), &hostname_to_outstanding_request_count_map,
                           &process_id_to_hostname_map, &process_id_to_record_start_map,
                           marc_reader, marc_writer,
                           &child_reported_failure_count, &active_child_count,
                           store_pdfs_as_html, use_separate_entries_per_url, include_all_tocs, include_list_of_references, only_pdf_fulltexts);

        if (active_child_count > process_count_high_watermark)
            CleanUpZombies(active_child_count - process_count_low_watermark, &hostname_to_outstanding_request_count_map,
                           &process_id_to_hostname_map, &process_id_to_record_start_map, marc_reader, marc_writer,
                           &child_reported_failure_count, &active_child_count);
    }

    // Wait for stragglers:
    CleanUpZombies(active_child_count, &hostname_to_outstanding_request_count_map, &process_id_to_hostname_map,
                   &process_id_to_record_start_map, marc_reader, marc_writer,
                   &child_reported_failure_count, &active_child_count);

    std::cerr << "Spawned " << download_record_offsets_and_urls.size() << " subprocesses.\n";
    std::cerr << semaphore.getValue()
              << " documents were not downloaded because their cached values had not yet expired.\n";
    std::cerr << child_reported_failure_count << " children reported a failure!\n";
}


constexpr unsigned PROCESS_COUNT_DEFAULT_HIGH_WATERMARK(10);
constexpr unsigned PROCESS_COUNT_DEFAULT_LOW_WATERMARK(5);


void ExtractLowAndHighWatermarks(const std::string &arg, unsigned * const process_count_low_watermark,
                                 unsigned * const process_count_high_watermark)
{
    const auto colon_pos(arg.find(':'));
    if (colon_pos == std::string::npos or not StringUtil::ToNumber(arg.substr(0, colon_pos), process_count_low_watermark)
        or not StringUtil::ToNumber(arg.substr(colon_pos + 1), process_count_high_watermark) or *process_count_low_watermark == 0
        or *process_count_high_watermark == 0)
        LOG_ERROR("bad low or high watermarks!");

    if (not (*process_count_low_watermark < *process_count_high_watermark))
        LOG_ERROR("the low water mark must be less than the high water mark!");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    MiscUtil::SetEnv("LOGGER_FORMAT", "process_pids");

    if (argc < 3)
        Usage();

    // Process optional args:
    unsigned process_count_low_watermark(PROCESS_COUNT_DEFAULT_LOW_WATERMARK),
             process_count_high_watermark(PROCESS_COUNT_DEFAULT_HIGH_WATERMARK);
    if (std::strcmp(argv[1], "--process-count-low-and-high-watermarks") == 0) {
        ExtractLowAndHighWatermarks(argv[2], &process_count_low_watermark, &process_count_high_watermark);
        argv += 2;
        argc -= 2;
    }

    unsigned pdf_extraction_timeout(DEFAULT_PDF_EXTRACTION_TIMEOUT);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--pdf-extraction-timeout=")) {
        if (not StringUtil::ToNumber(argv[1] + __builtin_strlen("--pdf-extraction-timeout="), &pdf_extraction_timeout)
            or pdf_extraction_timeout == 0)
                LOG_ERROR("bad value for --pdf-extraction-timeout!");
        ++argv, --argc;
    }

    bool only_open_access(false);
    if (argc > 1 and std::strcmp(argv[1], "--only-open-access") == 0) {
        only_open_access = true;
        ++argv, --argc;
    }

    bool store_pdfs_as_html(false);
    if (argc > 1 and std::strcmp(argv[1], "--store-pdfs-as-html") == 0) {
        store_pdfs_as_html = true;
        ++argv, --argc;
    }

    bool use_separate_entries_per_url(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--use-separate-entries-per-url")) {
        use_separate_entries_per_url = true;
        ++argv, --argc;
    }

    bool include_all_tocs(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--include-all-tocs")) {
        include_all_tocs = true;
        ++argv, --argc;
    }

    bool include_list_of_references(false);
        if (argc > 1 and StringUtil::StartsWith(argv[1], "--include-list-of-references")) {
        include_list_of_references = true;
        ++argv, --argc;
    }


    bool only_pdf_fulltexts(false);
    if (argc > 1 and std::strcmp(argv[1], "--only-pdf-fulltexts") == 0) {
        only_pdf_fulltexts = true;
        ++argv, --argc;
    }

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (marc_input_filename == marc_output_filename)
        LOG_ERROR("input filename must not equal output filename!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::FileType::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY));

    try {
        std::vector<std::pair<off_t, std::string>> download_record_offsets_and_urls;
        ProcessNoDownloadRecords(only_open_access, marc_reader.get(), marc_writer.get(), &download_record_offsets_and_urls);

        // Try to prevent clumps of URL's from the same server:
        std::random_shuffle(download_record_offsets_and_urls.begin(), download_record_offsets_and_urls.end());

        ProcessDownloadRecords(marc_reader.get(), marc_writer.get(), pdf_extraction_timeout,
                               download_record_offsets_and_urls, process_count_low_watermark, process_count_high_watermark,
                               store_pdfs_as_html, use_separate_entries_per_url, include_all_tocs,
                               include_list_of_references, only_pdf_fulltexts);
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }

    return EXIT_SUCCESS;
}
