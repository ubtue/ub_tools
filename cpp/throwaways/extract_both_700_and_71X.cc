/** \brief Extracts list of PPNs that contain both 700 and 710 and 711 fields\n
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <unordered_set>
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "title_data list.txt\n"
        "Extracts list of PPN that contain both 700 and 710 and 711 fields\n");
}


void ProcessRecords(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const target_ppns, unsigned *record_count) {
    while (const MARC::Record &record = marc_reader->read()) {
        ++(*record_count);
        if (record.hasTag("710") and (record.hasTag("710") or record.hasTag("711")))
            target_ppns->emplace(record.getControlNumber());
    }
}


} // unnamed namespace

int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unordered_set<std::string> target_ppns;
    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    unsigned record_count(0);
    ProcessRecords(marc_reader.get(), &target_ppns, &record_count);
    for (const auto &ppn : target_ppns)
        (*output) << ppn << '\n';
    LOG_INFO("Found " + std::to_string(target_ppns.size()) + " records of " + std::to_string(record_count) + '\n');
    return EXIT_SUCCESS;
}
