/** \brief A MARC-21 filter utility that selects records based on Library of Congress Subject Headings and
 *         counts LCSH frequencies in the selected set.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input subject1 [subject2 .. [subjectN]]\n\n"
              << "       where \"subject_list\" must contain LCSH's, one per line.\n";
    std::exit(EXIT_FAILURE);
}


/** Returns true if we match at least one subject in "subjects". */
inline bool Matched(const std::vector<std::string> &subjects,
                    const std::unordered_set<std::string> &loc_subject_headings)
{
    for (const auto &subject : subjects) {
        if (loc_subject_headings.find(subject) != loc_subject_headings.cend())
            return true;
    }

    return false;
}


// \return The number of entries that were removed.
size_t RemoveEmptyEntries(std::vector<std::string> * const entries) {
    std::vector<std::string> cleaned_up_entries;
    cleaned_up_entries.reserve(entries->size());

    for (const auto &entry : *entries) {
        if (not entry.empty())
            cleaned_up_entries.emplace_back(entry);
    }

    const size_t removed_count(entries->size() - cleaned_up_entries.size());
    entries->swap(cleaned_up_entries);
    return removed_count;
}


void CollectStats(MarcReader * const marc_reader, const std::unordered_set<std::string> &loc_subject_headings,
                  std::unordered_map<std::string, unsigned> * const subjects_to_counts_map,
                  unsigned * const match_count)
{
    *match_count = 0;
    unsigned total_count(0), duplicate_count(0), empty_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++total_count;

        std::vector<std::string> subjects;
        if (record.extractSubfield("650", 'a', &subjects) == 0)
            continue;

        for (auto &subject : subjects)
            StringUtil::RightTrim(" .", &subject);

        empty_count += RemoveEmptyEntries(&subjects);

        if (not Matched(subjects, loc_subject_headings))
            continue;

        ++*match_count;

        // Record our findings:
        std::unordered_set<std::string> already_inserted;
        for (const auto &subject : subjects) {
            if (already_inserted.find(subject) != already_inserted.cend()) {
                ++duplicate_count;
                continue;
            } else
                already_inserted.insert(subject);

            auto subject_and_count(subjects_to_counts_map->find(subject));
            if (subject_and_count == subjects_to_counts_map->end())
                (*subjects_to_counts_map)[subject] = 1;
            else
                ++subject_and_count->second;
        }
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Matched " << (*match_count) << " record(s).\n";
    std::cerr << "Found " << duplicate_count << " duplicate LCSH entries in some records.\n";
    std::cerr << "Removed " << empty_count << " empty entries.\n";
}


inline bool CompSubjectsAndSizes(const std::pair<std::string, unsigned> &subject_and_size1,
                                 const std::pair<std::string, unsigned> &subject_and_size2)
{
    return subject_and_size1.second > subject_and_size2.second;
}


void DisplayStats(const std::unordered_map<std::string, unsigned> &subjects_to_counts_map,
                  const unsigned total_count)
{
    std::vector<std::pair<std::string, unsigned>> subjects_and_counts;
    subjects_and_counts.reserve(subjects_to_counts_map.size());
    for (const auto &subject_and_count : subjects_to_counts_map)
        subjects_and_counts.emplace_back(subject_and_count);

    std::sort(subjects_and_counts.begin(), subjects_and_counts.end(), CompSubjectsAndSizes);

    for (const auto subject_and_count : subjects_and_counts)
        std::cout << subject_and_count.first << ' '
                  << StringUtil::ToString(subject_and_count.second * 100.0 / total_count, 5) << "%\n";
}


int main(int /*argc*/, char **argv) {
    ::progname = argv[0];
    ++argv;
    if (*argv == nullptr)
        Usage();

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(*argv++));
        if (*argv == nullptr)
            Usage();

        std::unordered_set<std::string> loc_subject_headings;
        while (*argv != nullptr)
            loc_subject_headings.insert(*argv++);

        std::unordered_map<std::string, unsigned> subjects_to_counts_map;
        unsigned match_count;
        CollectStats(marc_reader.get(), loc_subject_headings, &subjects_to_counts_map, &match_count);
        DisplayStats(subjects_to_counts_map, match_count);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
