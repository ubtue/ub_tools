/** \brief Saves local MARC data in a database for later retrieval with the add_local_data tool.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "LocalDataDB.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_title_data_with_local_data");
}


void StoreLocalData(LocalDataDB * const local_data_db, MARC::Reader * const reader) {
    unsigned total_record_count(0), local_data_extraction_count(0);
    while (auto record = reader->read()) {
        ++total_record_count;

        const auto PPN(record.getControlNumber());

        auto local_field(record.findTag("LOK"));
        if (unlikely(local_field == record.end())) {
            LOG_WARNING("records w/ PPN " + PPN + " has no local fields!");
            continue;
        }

        std::vector<std::string> local_fields;
        do
            local_fields.emplace_back(local_field->getContents());
        while (++local_field != record.end());

        local_data_db->insertOrReplace(record.getControlNumber(), local_fields);
        ++local_data_extraction_count;
    }

    LOG_INFO("Extracted local data from " + std::to_string(local_data_extraction_count) + " of " + std::to_string(total_record_count)
             + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    LocalDataDB local_data_db(LocalDataDB::READ_WRITE);
    StoreLocalData(&local_data_db, marc_reader.get());

    return EXIT_SUCCESS;
}
