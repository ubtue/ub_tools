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

        // Remove HAS field:
        const auto has_field(record.findTag("HAS"));
        if (unlikely(has_field == record.end()))
            LOG_ERROR("missing HAS field!");
        record.erase(has_field);

        // Remove URL url_field:
        const auto url_field(record.findTag("URL"));
        if (unlikely(url_field == record.end()))
            LOG_ERROR("missing URL field!");
        record.erase(url_field);

        xml_writer.write(record);

        const auto superior_title(record.getSuperiorTitle());
        std::string superior_title_sql(
            superior_title.empty() ? "" : ",superior_title=" + db_connection->escapeAndQuoteString(superior_title));

        db_connection->queryOrDie("INSERT INTO marc_records SET url=" + db_connection->escapeAndQuoteString(url)
                                  + ",hash=" + db_connection->escapeAndQuoteString(hash) + ",main_title="
                                  + db_connection->escapeAndQuoteString(record.getMainTitle()) + superior_title_sql + ",record="
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
