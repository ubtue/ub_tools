/** \brief Utility for storing MARC records in our delivery history database.
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

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "GzStream.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void StoreRecords(DbConnection * const db_connection, MARC::Reader * const marc_reader) {
    unsigned record_count(0);

    std::string record_blob;
    MARC::XmlWriter xml_writer(&record_blob);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        const std::string hash(record.getFirstFieldContents("HAS"));
        const std::string url(record.getFirstFieldContents("URL"));
        const std::string zeder_id(record.getFirstFieldContents("ZID"));

        // Remove HAS, URL and ZID fields because we don't want to upload them to the BSZ FTP server:
        record.erase("HAS");
        record.erase("URL");
        record.erase("ZID");

        xml_writer.write(record);

        const auto superior_control_number(record.getSuperiorControlNumber());
        std::string superior_control_number_sql(
            superior_control_number.empty() ? "" : ",superior_control_number="
                                                   + db_connection->escapeAndQuoteString(superior_control_number));

        db_connection->queryOrDie("INSERT INTO marc_records SET url=" + db_connection->escapeAndQuoteString(url)
                                  + ",zeder_id=" + db_connection->escapeAndQuoteString(zeder_id) + ",hash="
                                  + db_connection->escapeAndQuoteString(hash) + ",main_title="
                                  + db_connection->escapeAndQuoteString(record.getMainTitle()) + superior_control_number_sql + ",record="
                                  + db_connection->escapeAndQuoteString(GzStream::CompressString(record_blob, GzStream::GZIP)));
        record_blob.clear();
        db_connection->queryOrDie("SELECT LAST_INSERT_ID() AS id");
        const DbRow id_row(db_connection->getLastResultSet().getNextRow());
        const std::string last_id(id_row["id"]);

        for (const auto &author : record.getAllAuthors())
            db_connection->queryOrDie("INSERT INTO marc_authors SET marc_records_id=" + last_id + ",author="
                                      + db_connection->escapeAndQuoteString(author));
    }

    std::cout << "Stored " << record_count << " MARC record(s).\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    DbConnection db_connection;
    auto marc_reader(MARC::Reader::Factory(argv[1]));
    StoreRecords(&db_connection, marc_reader.get());

    return EXIT_SUCCESS;
}
