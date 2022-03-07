/** \brief Utility for comparing ngram-assigned languages to human-assigned languages in MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "NGram.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--verbose] [--limit-count=count] [--cross-valiatdion-chunks=N] marc_data "
        "[language_code1 language_code2 .. language_codeN]\n"
        "If \"--limit-count\" has been specified only the first \"count\" records will be considered.\n"
        "If \"--cross-valiatdion-chunks\" has been specified, N sets will be used.\n"
        "The default for --topmost-use-count is "
        + std::to_string(NGram::DEFAULT_TOPMOST_USE_COUNT) + ".");
}


#if 0
void GenerateModels(const bool verbose, const unsigned limit_count, const unsigned cross_validation_chunk_count,
                    const unsigned leave_out_index, MARC::Reader * const marc_reader, const NGram::DistanceType distance_type,
                    const std::set<std::string> &considered_languages)
{
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
        if (record_count == limit_count)
            break;

        if (record_count % cross_validation_chunk_count == leave_out_index)
            continue;
    }
}
#endif


void ProcessRecords(const bool verbose, const unsigned limit_count, const unsigned /*cross_validation_chunk_count*/,
                    MARC::Reader * const marc_reader, const std::set<std::string> &considered_languages,
                    std::unordered_map<std::string, unsigned> * const mismatched_assignments_to_counts_map) {
    unsigned record_count(0), untagged_count(0), agreed_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        if (record_count > limit_count)
            break;

        ++record_count;
        std::string language_code(MARC::GetLanguageCode(record));
        if (language_code.empty()) {
            ++untagged_count;
            continue;
        }

        const std::string text(record.getCompleteTitle() + " " + record.getSummary());
        std::vector<NGram::DetectedLanguage> top_languages;
        NGram::ClassifyLanguage(text, &top_languages, considered_languages);
        if (top_languages.empty())
            continue;

        if (top_languages.front().language_ == language_code)
            ++agreed_count;
        else {
            const std::string key(language_code + ":" + top_languages.front().language_);
            if (verbose)
                std::cout << key << "  " << text << '\n';
            const auto mismatched_assignment_and_count(mismatched_assignments_to_counts_map->find(key));
            if (mismatched_assignment_and_count == mismatched_assignments_to_counts_map->end())
                mismatched_assignments_to_counts_map->emplace(key, 1u);
            else
                ++mismatched_assignment_and_count->second;
        }
    }

    std::cout << "Used " << record_count << " MARC record(s) of which " << untagged_count << " had no language and "
              << (agreed_count * 100.0) / (record_count - untagged_count) << "% of which had matching languages.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    unsigned limit_count(UINT_MAX);
    if (StringUtil::StartsWith(argv[1], "--limit-count=")) {
        limit_count = StringUtil::ToUnsigned(argv[1] + __builtin_strlen("--limit-count="));
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    unsigned cross_validation_chunk_count(0);
    if (StringUtil::StartsWith(argv[1], "--cross-valiatdion-chunks=")) {
        cross_validation_chunk_count = StringUtil::ToUnsigned(argv[1] + __builtin_strlen("--cross-valiatdion-chunks="));
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::set<std::string> considered_languages;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        considered_languages.emplace(argv[arg_no]);

    std::unordered_map<std::string, unsigned> mismatched_assignments_to_counts_map;
    ProcessRecords(verbose, limit_count, cross_validation_chunk_count, marc_reader.get(), considered_languages,
                   &mismatched_assignments_to_counts_map);

    std::vector<std::pair<std::string, unsigned>> mismatched_assignments_and_counts;
    mismatched_assignments_and_counts.reserve(mismatched_assignments_to_counts_map.size());
    for (const auto &mismatched_assignment_and_count : mismatched_assignments_to_counts_map)
        mismatched_assignments_and_counts.emplace_back(mismatched_assignment_and_count);

    std::sort(mismatched_assignments_and_counts.begin(), mismatched_assignments_and_counts.end(),
              [](const std::pair<std::string, unsigned> &pair1, const std::pair<std::string, unsigned> &pair2) {
                  return pair1.second > pair2.second;
              });

    for (const auto &mismatched_assignment_and_count : mismatched_assignments_and_counts)
        std::cout << mismatched_assignment_and_count.first << " = " << mismatched_assignment_and_count.second << '\n';

    return EXIT_SUCCESS;
}
