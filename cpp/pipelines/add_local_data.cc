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
#include "DbConnection.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("input_marc_title_data output_marc_title_data");
}


// Appends local data for each record, for which local data will be found in our database.
// The local data is store in a format where the contents of each field is preceeded by a 4-character
// hex string indicating the length of the immediately following field contents.
// Multiple local fields may occur per record.
void AddLocalData(DbConnection * const db_connection, MARC::Reader * const reader, MARC::Writer * const writer) {
    unsigned total_record_count(0), added_count(0);
    while (auto record = reader->read()) {
        ++total_record_count;

        db_connection->queryOrDie("SELECT local_fields FROM local_data WHERE ppn = "
                                  + db_connection->escapeAndQuoteString(record.getControlNumber()));
        auto result_set(db_connection->getLastResultSet());
        if (not result_set.empty()) {
            const auto row(result_set.getNextRow());
            const auto local_fields_blob(row["local_fields"]);
            size_t processed_size(0);
            do {
                // Convert the 4 character hex string to size of the following field contents:
                const size_t field_contents_size(StringUtil::ToUnsignedLong(local_fields_blob.substr(processed_size, 4), 16));
                processed_size += 4;

                if (unlikely(processed_size + field_contents_size > local_fields_blob.size()))
                    LOG_ERROR("Inconsitent blob length for record with PPN " + record.getControlNumber());

                record.appendField("LOK", local_fields_blob.substr(processed_size, field_contents_size));
                processed_size += field_contents_size;
            } while (processed_size < local_fields_blob.size());
        }

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

    DbConnection db_connection(UBTools::GetTuelibPath() + "local_data.sq3", DbConnection::READONLY);
    AddLocalData(&db_connection, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
