/** \brief Utility for displaying various bits of info about a collection of MARC records.
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
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << "\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONF_FILE_PATH(
    "/usr/local/var/lib/tuelib/cronjobs/merge_differential_and_full_marc_updates.conf");


void LoadFilePCRE(std::string * const file_pcre) {
    IniFile ini_file(CONF_FILE_PATH);
    *file_pcre = ini_file.getString("Files", "deletion_list");
    *file_pcre += "|" + ini_file.getString("Files", "complete_dump");
    *file_pcre += "|" + ini_file.getString("Files", "incremental_dump");
    *file_pcre += "|" + ini_file.getString("Files", "incremental_authority_dump");
    *file_pcre += "|Complete-MARC-.*-\\d\\d\\d\\d\\d\\d.tar.gz";
}


// Assumes that part of "filename" matches YYMMDD.
std::string ExtractDate(const std::string &filename) {
    static auto date_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(\\d\\d[01]\\d[0123]\\d)"));
    if (date_matcher->matched(filename))
        return (*date_matcher)[1];
    LOG_ERROR("filename \"" + filename + "\" does not contain YYMMDD!");
}


inline bool Contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}


bool FileComparator(const std::string &filename1, const std::string &filename2) {
    const auto date1(ExtractDate(filename1));
    const auto date2(ExtractDate(filename2));

    if (date1 != date2)
        return date1 < date2;

    // Deletion lists come first:
    if (filename1[0] == 'L' and filename2[0] != 'L')
        return true;
    if (filename2[0] == 'L' and filename1[0] != 'L')
        return false;

    // Complete dumps come before anything else:
    if (StringUtil::StartsWith(filename1, "SA-") and not StringUtil::StartsWith(filename2, "SA-"))
        return true;
    if (StringUtil::StartsWith(filename2, "SA-") and not StringUtil::StartsWith(filename1, "SA-"))
        return false;

    // Pseudo complete dumps come before anything else:
    if (StringUtil::StartsWith(filename1, "Complete-MARC-") and not StringUtil::StartsWith(filename2, "Complete-MARC-"))
        return true;
    if (StringUtil::StartsWith(filename2, "Complete-MARC-") and not StringUtil::StartsWith(filename1, "Complete-MARC-"))
        return false;

    // Sekkor updates come before anything else:
    if (Contains(filename1, "sekkor") and not Contains(filename2, "sekkor"))
        return true;
    if (Contains(filename2, "sekkor") and not Contains(filename1, "sekkor"))
        return false;

    // Files w/o local data come before those w/ local data:
    if (Contains(filename1, "_o") and not Contains(filename2, "_o"))
        return true;
    if (Contains(filename2, "_o") and not Contains(filename1, "_o"))
        return false;

    LOG_ERROR("don't know how to compare \"" + filename1 + "\" with \"" + filename2 + "\"!");
}


// Returns file_list.end() if neither a complete dump file name nor a pseudo complete dump file name were found.
std::vector<std::string>::const_iterator FindMostRecentCompleteOrPseudoCompleteDump(const std::vector<std::string> &file_list) {
    auto file(file_list.cbegin());
    while (file != file_list.cend()) {
        if (StringUtil::StartsWith(*file, "SA-") or StringUtil::StartsWith(*file, "Complete-MARC-"))
            return file;
    }

    return file_list.cend();
}


// Shift a given YYMMDD to ten days before
std::string ShiftDateToTenDaysBefore(const std::string &cutoff_date) {
    struct tm cutoff_date_tm(TimeUtil::StringToStructTm(cutoff_date, "%y%m%d"));
    const time_t cutoff_date_time_t(TimeUtil::TimeGm(cutoff_date_tm));
    if (unlikely(cutoff_date_time_t == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (1)");
    const time_t new_cutoff_date(TimeUtil::AddDays(cutoff_date_time_t, -10));
    if (unlikely(new_cutoff_date == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (2)");
    return TimeUtil::TimeTToString(new_cutoff_date, "%y%m%d");
}


void DetermineProcessingOrder(std::vector<std::string> * const file_list) {
    std::sort(file_list->begin(), file_list->end(), FileComparator);
    const auto complete_or_pseudo_complete_dump(FindMostRecentCompleteOrPseudoCompleteDump(*file_list));
    if (complete_or_pseudo_complete_dump == file_list->end())
        LOG_ERROR("found neither a complete nor a pseudo complete dump file!");

    auto post_most_recent_complete_dump_start(complete_or_pseudo_complete_dump + 1);

    // If we have found an SA- file we likely have two, one w/ and one w/o local data:
    if (StringUtil::StartsWith(*complete_or_pseudo_complete_dump, "SA-")) {
        if (complete_or_pseudo_complete_dump + 1 == file_list->end()
            or not StringUtil::StartsWith(*(complete_or_pseudo_complete_dump + 1), "SA-")
            or not (ExtractDate(*complete_or_pseudo_complete_dump) == ExtractDate(*(complete_or_pseudo_complete_dump + 1))))
            LOG_WARNING("expected a pair of SA- files w/ the same date!");
        else
            ++post_most_recent_complete_dump_start;
    }

    auto sekkor_insertion_pos(complete_or_pseudo_complete_dump);
    const auto cutoff_date(ShiftDateToTenDaysBefore(ExtractDate(*complete_or_pseudo_complete_dump)));

    while (post_most_recent_complete_dump_start != file_list->end()
           and ExtractDate(*post_most_recent_complete_dump_start) > cutoff_date)
    {
        if (Contains(*post_most_recent_complete_dump_start, "sekkor")) {
            const size_t saved_index(post_most_recent_complete_dump_start - file_list->begin());
            const auto sekkor_filename(*post_most_recent_complete_dump_start);
            file_list->erase(post_most_recent_complete_dump_start);
            file_list->insert(sekkor_insertion_pos, sekkor_filename);
            ++sekkor_insertion_pos;
            ++post_most_recent_complete_dump_start = file_list->begin() + saved_index;
        }

        ++post_most_recent_complete_dump_start;
    }
}


} // unnamed namespace


int Main(int argc, char */*argv*/[]) {
    if (argc != 1)
        Usage();

    std::string file_pcre;
    LoadFilePCRE(&file_pcre);

    std::vector<std::string> file_list;
    if (FileUtil::GetFileNameList(file_pcre, &file_list) == 0)
        LOG_ERROR("no matches found for \"" + file_pcre + "\"!");

    DetermineProcessingOrder(&file_list);

    for (const auto &filename : file_list)
        std::cout << filename << '\n';

    return EXIT_SUCCESS;
}
