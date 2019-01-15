/** \brief Utility for comparing ngram-assigned languages to human-assigned languages in MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "NGram.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage( "[--verbose] [--distance-type=(simple|weighted)] marc_data [language_code1 language_code2 .. language_codeN]");
}

void ProcessRecords(const bool verbose, MARC::Reader * const marc_reader, const NGram::DistanceType distance_type,
                    const std::set<std::string> &considered_languages,
                    std::unordered_map<std::string, unsigned> * const mismatched_assignments_to_counts_map)
{
    unsigned record_count(0), untagged_count(0), agreed_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
        if (record_count == 1000)
            break;

        std::string language_code(MARC::GetLanguageCode(record));
        if (language_code.empty()) {
            ++untagged_count;
            continue;
        }
        if (StringUtil::TrimWhite(&language_code).empty())
            LOG_ERROR("D'oh!");

        const std::string complete_title(record.getCompleteTitle());
        std::vector<std::string> top_languages;
        NGram::ClassifyLanguage(complete_title, &top_languages, considered_languages, distance_type);
        if (top_languages.empty())
            continue;
        if (unlikely(top_languages.front().empty()))
            LOG_ERROR("WTF?!");

        if (top_languages.front() == language_code)
            ++agreed_count;
        else {
            const std::string key(language_code + ":" + top_languages.front());
            if (verbose)
                std::cout << key << "  " << complete_title << '\n';
            const auto mismatched_assignment_and_count(mismatched_assignments_to_counts_map->find(key));
            if (mismatched_assignment_and_count == mismatched_assignments_to_counts_map->end())
                mismatched_assignments_to_counts_map->emplace(key, 1u);
            else
                ++mismatched_assignment_and_count->second;
        }
    }

    std::cout << "Data set contains " << record_count << " MARC record(s) of which " << untagged_count << " had no language and "
              << (agreed_count * 100.0) / (record_count - untagged_count) << "% of  which had matching languages.\n";
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

    NGram::DistanceType distance_type(NGram::SIMPLE_DISTANCE);
    if (StringUtil::StartsWith(argv[1], "--distance-type=")) {
        if (std::strcmp(argv[1], "--distance-type=simple") == 0)
            distance_type = NGram::SIMPLE_DISTANCE;
        else if (std::strcmp(argv[1], "--distance-type=weighted") == 0)
            distance_type = NGram::WEIGHTED_DISTANCE;
        else
            Usage();
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::set<std::string> considered_languages;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        considered_languages.emplace(argv[arg_no]);

    std::unordered_map<std::string, unsigned> mismatched_assignments_to_counts_map;
    ProcessRecords(verbose, marc_reader.get(), distance_type, considered_languages, &mismatched_assignments_to_counts_map);

    std::vector<std::pair<std::string, unsigned>> mismatched_assignments_and_counts;
    mismatched_assignments_and_counts.reserve(mismatched_assignments_to_counts_map.size());
    for (const auto &mismatched_assignment_and_count : mismatched_assignments_to_counts_map)
        mismatched_assignments_and_counts.emplace_back(mismatched_assignment_and_count);

    std::sort(mismatched_assignments_and_counts.begin(), mismatched_assignments_and_counts.end(),
              [](const std::pair<std::string, unsigned> &pair1, const std::pair<std::string, unsigned> &pair2)
                  { return pair1.second > pair2.second; });

    for (const auto &mismatched_assignment_and_count : mismatched_assignments_and_counts)
        std::cout << mismatched_assignment_and_count.first << " = " << mismatched_assignment_and_count.second << '\n';

    return EXIT_SUCCESS;
}
