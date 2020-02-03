/** \brief A tool to find changed article records for our partners in Cologne.
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

#include <unordered_set>
#include <cstdlib>
#include "DbConnection.h"
#include "MARC.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_input marc_output");
}


void ExtractChangedRelevantArticles(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                                    const std::unordered_set<std::string> &superior_ppns_of_interest)
{
    DbConnection db_connection(UBTools::GetTuelibPath() + "cologne_article_hashes.sq3", DbConnection::CREATE);
    db_connection.queryOrDie("CREATE TABLE IF NOT EXISTS record_hashes ("
                             "    ppn TEXT PRIMARY KEY,"
                             "    hash TEXT NOT NULL"
                             ") WITHOUT ROWID");

    unsigned relevant_article_count(0), changed_article_count(0);
    while (auto record = marc_reader->read()) {
        if (not record.isArticle()
            or superior_ppns_of_interest.find(record.getSuperiorControlNumber()) == superior_ppns_of_interest.cend())
            continue;
        ++relevant_article_count;

        const auto current_hash(MARC::CalcChecksum(record));

        db_connection.queryOrDie("SELECT hash FROM record_hashes WHERE ppn='" + record.getControlNumber() + "'");
        DbResultSet result_set(db_connection.getLastResultSet());
        std::string stored_hash;
        if (not result_set.empty()) {
            const DbRow row(result_set.getNextRow());
            stored_hash = row["hash"];
        }

        if (stored_hash != current_hash) {
            record.erase(MARC::Tag("LOK"));
            marc_writer->write(record);
            ++changed_article_count;
            db_connection.queryOrDie("REPLACE INTO record_hashes (ppn, hash) VALUES ('" + record.getControlNumber() + "', '"
                                     + current_hash + "')");
        }
    }

    LOG_INFO("Found " + std::to_string(relevant_article_count) + " of which " + std::to_string(changed_article_count)
             + " had not been encountered before or were changed.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    const std::unordered_set<std::string> superior_ppns{ "1665710918", "1662989814", "1664641068", "1668994887" };
    ExtractChangedRelevantArticles(marc_reader.get(), marc_writer.get(), superior_ppns);

    return EXIT_SUCCESS;
}
