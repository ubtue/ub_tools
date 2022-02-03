/** \file   extract_zeder_data.cc
 *  \brief  Imports data from Zeder and generates a CSV file from it.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <cstring>
#include "FileUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "Zeder.h"
#include "util.h"


namespace {


void ProcessZederAndWriteCSVFile(File * const csv_file, const Zeder::SimpleZeder &zeder,
                                 const std::vector<std::string> &requested_columns) {
    unsigned journal_count(0), bad_count(0);
    for (const auto &journal : zeder) {
        ++journal_count;

        const auto zeder_id(std::to_string(journal.getId()));
        if (unlikely(not journal.hasAttribute("tit"))) {
            ++bad_count;
            LOG_WARNING("Zeder entry #" + zeder_id + " is missing a title!");
            continue;
        }

        bool first(true);
        for (const auto &requested_column : requested_columns) {
            if (first)
                first = false;
            else
                *csv_file << ',';

            *csv_file << TextUtil::CSVEscape(journal.lookup(requested_column));
        }
        *csv_file << '\n';
    }

    LOG_INFO("Processed " + std::to_string(journal_count) + " journal entries of which " + std::to_string(bad_count) + " was/were bad.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        ::Usage(
            "[--min-log-level=min_verbosity] csv_filename zeder_favour zeder_column1 [zeder_column2 .. zeder_columnN]\n"
            "Writes the values of the selected Zeder columns in the CSV file \"csv_filename\".\n"
            "Please note that rows that are missing a title are always skipped!\n");

    const auto csv_file(FileUtil::OpenOutputFileOrDie(argv[1]));

    Zeder::Flavour zeder_favour;
    if (std::strcmp(argv[2], "ixtheo") == 0)
        zeder_favour = Zeder::IXTHEO;
    else if (std::strcmp(argv[2], "krimdok") == 0)
        zeder_favour = Zeder::KRIMDOK;
    else
        LOG_ERROR("bad Zeder flavor \"" + std::string(argv[2]) + "\"!");

    std::vector<std::string> requested_columns;
    for (int arg_no(3); arg_no < argc; ++arg_no)
        requested_columns.push_back(argv[arg_no]);

    std::unordered_set<std::string> column_name_set(requested_columns.cbegin(), requested_columns.cend());
    column_name_set.insert("tit"); // Always include the title!
    const Zeder::SimpleZeder zeder(zeder_favour, column_name_set);
    if (not zeder)
        LOG_ERROR("we can't connect to the Zeder MySQL database!");
    if (unlikely(zeder.empty()))
        LOG_ERROR("found no Zeder entries matching any of our requested columns!");

    ProcessZederAndWriteCSVFile(csv_file.get(), zeder, requested_columns);

    return EXIT_SUCCESS;
}
