/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "BSZUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << "\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "cronjobs/merge_differential_and_full_marc_updates.conf");


void LoadFilePCRE(std::string * const file_pcre) {
    IniFile ini_file(CONF_FILE_PATH);
    *file_pcre = ini_file.getString("Files", "deletion_list");
    *file_pcre += "|" + ini_file.getString("Files", "complete_dump");
    *file_pcre += "|" + ini_file.getString("Files", "incremental_dump");
    *file_pcre += "|" + ini_file.getString("Files", "incremental_authority_dump");
    *file_pcre += "|Complete-MARC-\\d\\d\\d\\d\\d\\d.tar.gz$";
}


// Shift a given YYMMDD to ten days after
std::string ShiftDateToTenDaysAfter(const std::string &cutoff_date) {
    struct tm cutoff_date_tm(TimeUtil::StringToStructTm(cutoff_date, "%y%m%d"));
    const time_t cutoff_date_time_t(TimeUtil::TimeGm(cutoff_date_tm));
    if (unlikely(cutoff_date_time_t == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (1)");
    const time_t new_cutoff_date(TimeUtil::AddDays(cutoff_date_time_t, +10));
    if (unlikely(new_cutoff_date == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (2)");
    return TimeUtil::TimeTToString(new_cutoff_date, "%y%m%d");
}


bool FileComparator(const std::string &filename1, const std::string &filename2) {
    auto date1(BSZUtil::ExtractDateFromFilenameOrDie(filename1));
    if (StringUtil::Contains(filename1, "sekkor"))
        date1 = ShiftDateToTenDaysAfter(date1);

    auto date2(BSZUtil::ExtractDateFromFilenameOrDie(filename2));
    if (StringUtil::Contains(filename2, "sekkor"))
        date2 = ShiftDateToTenDaysAfter(date2);

    if (date1 != date2)
        return date1 < date2;

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

    // Deletion lists come first:
    if (filename1[0] == 'L' and filename2[0] != 'L')
        return true;
    if (filename2[0] == 'L' and filename1[0] != 'L')
        return false;

    // Sekkor updates come before anything else:
    if (StringUtil::Contains(filename1, "sekkor") and not StringUtil::Contains(filename2, "sekkor"))
        return true;
    if (StringUtil::Contains(filename2, "sekkor") and not StringUtil::Contains(filename1, "sekkor"))
        return false;

    // Files w/o local data come before those w/ local data:
    if (StringUtil::Contains(filename1, "_o") and not StringUtil::Contains(filename2, "_o"))
        return true;
    if (StringUtil::Contains(filename2, "_o") and not StringUtil::Contains(filename1, "_o"))
        return false;

    LOG_ERROR("don't know how to compare \"" + filename1 + "\" with \"" + filename2 + "\"!");
}


inline bool IsMtexDeletionList(const std::string &filename) {
    return StringUtil::StartsWith(filename, "LOEKXP_m-");
}


inline bool IsKrexDeletionList(const std::string &filename) {
    return StringUtil::StartsWith(filename, "LOEKXP_k-");
}


inline bool IsMtexOrKrexDeletionList(const std::string &filename) {
    return (IsMtexDeletionList(filename) or IsKrexDeletionList(filename));
}


bool MtexKrexComparator(const std::string &filename1, const std::string &filename2) {
    // Since Mtex will only occur for IxTheo & Krex will only occur for KrimDok,
    // we can use the same comparator logic
    if (IsMtexOrKrexDeletionList(filename1) and IsMtexOrKrexDeletionList(filename2))
        return BSZUtil::ExtractDateFromFilenameOrDie(filename1) < BSZUtil::ExtractDateFromFilenameOrDie(filename2);
    if (IsMtexOrKrexDeletionList(filename1))
        return false;
    if (IsMtexOrKrexDeletionList(filename2))
        return true;
    return FileComparator(filename1, filename2);
}


// Returns file_list.end() if neither a complete dump file name nor a pseudo complete dump file name were found.
std::vector<std::string>::iterator FindMostRecentCompleteOrPseudoCompleteDump(std::vector<std::string> &file_list) {
    auto file(file_list.end());
    do {
        --file;
        if (StringUtil::StartsWith(*file, "SA-") or StringUtil::StartsWith(*file, "Complete-MARC-"))
            return file;
    } while (file != file_list.begin());

    return file_list.end();
}


// If our complete dump is an SA- file, we should have a "partner" w/o local data.  In that case we should return the partner.
std::vector<std::string>::iterator EarliestReferenceDump(std::vector<std::string>::iterator complete_or_pseudo_complete_dump,
                                                         std::vector<std::string> &file_list) {
    /* If we have found an SA- file we may have two, one w/ and one w/o local data: */
    if (StringUtil::StartsWith(*complete_or_pseudo_complete_dump, "SA-")) {
        if (complete_or_pseudo_complete_dump != file_list.begin() and StringUtil::StartsWith(*(complete_or_pseudo_complete_dump - 1), "SA-")
            and (BSZUtil::ExtractDateFromFilenameOrDie(*complete_or_pseudo_complete_dump)
                 == BSZUtil::ExtractDateFromFilenameOrDie(*(complete_or_pseudo_complete_dump - 1))))
            return complete_or_pseudo_complete_dump - 1;
    }

    return complete_or_pseudo_complete_dump;
}


} // unnamed namespace


int Main(int argc, char * /*argv*/[]) {
    if (argc != 1)
        Usage();

    std::string file_pcre;
    LoadFilePCRE(&file_pcre);
    LOG_DEBUG("file PCRE: \"" + file_pcre + "\".");

    std::vector<std::string> file_list;
    if (FileUtil::GetFileNameList(file_pcre, &file_list) == 0)
        LOG_ERROR("no matches found for \"" + file_pcre + "\"!");

    std::sort(file_list.begin(), file_list.end(), FileComparator);
    std::stable_sort(file_list.begin(), file_list.end(), MtexKrexComparator); // mtex/krex deletion lists must go last

    // Throw away older files before our "reference" complete dump or pseudo complete dump:
    const auto reference_dump(FindMostRecentCompleteOrPseudoCompleteDump(file_list));
    if (reference_dump == file_list.end())
        LOG_ERROR("no reference dump file found!");
    file_list.erase(file_list.begin(), EarliestReferenceDump(reference_dump, file_list));

    for (const auto &filename : file_list)
        std::cout << filename << '\n';

    return EXIT_SUCCESS;
}
