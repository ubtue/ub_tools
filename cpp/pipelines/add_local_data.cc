/** \brief Adds local MARC data from a database to MARC title records w/o local data.
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
    ::Usage("input_marc_title_data output_marc_title_data");
}


bool AddLocalData(const LocalDataDB &local_data_db, MARC::Record * const record, const std::string &ppn) {
    const auto local_fields(local_data_db.getLocalFields(ppn));
    if (local_fields.empty())
        return false;

    for (const auto &local_field : local_fields)
        record->insertFieldAtEnd("LOK", local_field);
    return true;
}


// Appends local data for each record, for which local data will be found in our database.
// The local data is stored in a format where the contents of each field is preceeded by a 4-character
// hex string indicating the length of the immediately following field contents.
// Multiple local fields may occur per record.
void ProcessRecords(const LocalDataDB &local_data_db, MARC::Reader * const reader, MARC::Writer * const writer) {
    unsigned total_record_count(0), added_count(0);
    while (auto record = reader->read()) {
        ++total_record_count;

        std::set<std::string> local_data_ppns{ record.getControlNumber() };

        for (const auto &zwi_field : record.getTagRange("ZWI")) {
            for (const auto &sub_field_code_and_value : zwi_field.getSubfields()) {
                if (sub_field_code_and_value.code_ == 'b')
                    local_data_ppns.emplace(sub_field_code_and_value.value_);
            }
        }

        bool added_at_least_one_local_data_block(false);
        for (const auto &local_data_ppn : local_data_ppns) {
            if (AddLocalData(local_data_db, &record, local_data_ppn))
                added_at_least_one_local_data_block = true;
        }
        if (added_at_least_one_local_data_block)
            ++added_count;

        writer->write(record);
    }

    LOG_INFO("Added local data to " + std::to_string(added_count) + " out of " + std::to_string(total_record_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    LocalDataDB local_data_db(LocalDataDB::READ_ONLY);
    ProcessRecords(local_data_db, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
