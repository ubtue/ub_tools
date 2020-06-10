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
#include "DbConnection.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_title_data_with_local_data");
}


void StoreLocalData(DbConnection * const db_connection, MARC::Reader * const reader) {
    unsigned total_record_count(0), local_data_extraction_count(0);
    while (auto record = reader->read()) {
        ++total_record_count;

        const auto PPN(record.getControlNumber());

        auto local_field(record.findTag("LOK"));
        if (unlikely(local_field == record.end())) {
            LOG_WARNING("records w/ PPN " + PPN + " has no local fields!");
            continue;
        }

        std::string local_fields_blob;
        do {
            const auto &field_contents(local_field->getContents());
            local_fields_blob += StringUtil::ToString(field_contents.length(), /* radix = */16,
                                                      /* width = */4, /* padding_char = */'0');
            local_fields_blob += field_contents;
        } while (++local_field != record.end());

        db_connection->queryOrDie("REPLACE INTO local_data (ppn, local_fields) VALUES("
                                  + db_connection->escapeAndQuoteString(PPN) + ","
                                  + db_connection->escapeAndQuoteString(local_fields_blob) + ")");

        ++local_data_extraction_count;
    }

    LOG_INFO("Extracted local data from " + std::to_string(local_data_extraction_count)
             + " of " + std::to_string(total_record_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    DbConnection db_connection(UBTools::GetTuelibPath() + "local_data.sq3", DbConnection::READWRITE);
    db_connection.queryOrDie("CREATE TABLE IF NOT EXISTS local_data ("
                             "    ppn TEXT PRIMARY KEY,"
                             "    local_fields BLOB NOT NULL"
                             ") WITHOUT ROWID");

    StoreLocalData(&db_connection, marc_reader.get());

    return EXIT_SUCCESS;
}
