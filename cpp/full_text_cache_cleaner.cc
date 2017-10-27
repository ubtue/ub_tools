/** \brief Tool to delete old cache entries from the KrimDok full text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <cstdlib>
#include <ctime>
#include <kchashdb.h>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "SqlUtil.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " full_text_db_path\n";
    std::cerr << "       Deletes all expired records from the full text cache\n";
    std::exit(EXIT_FAILURE);
}


void ExpireRecords(DbConnection * const db_connection, kyotocabinet::HashDB * const key_value_db) {
    const std::string now(SqlUtil::TimeTToDatetime(std::time(nullptr)));

    db_connection->queryOrDie("SELECT id FROM full_text_cache WHERE expiration < \"" + now + "\"");
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const DbRow row = result_set.getNextRow())
        key_value_db->remove(row["id"]);

    db_connection->queryOrDie("DELETE FROM full_text_cache WHERE expiration < \"" + now + "\"");
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        const std::string db_filename(argv[1]);
        kyotocabinet::HashDB key_value_db;
        if (not key_value_db.open(db_filename, kyotocabinet::HashDB::OWRITER))
            logger->error("Failed to open the key/valuedatabase \"" + db_filename + "\" ("
                          + std::string(key_value_db.error().message()) + ")!");

        const unsigned size_before_deletion(SqlUtil::GetTableSize(&db_connection, "full_text_cache"));
            ExpireRecords(&db_connection, &key_value_db);
        const unsigned size_after_deletion(SqlUtil::GetTableSize(&db_connection, "full_text_cache"));

        std::cerr << "Deleted " << (size_before_deletion - size_after_deletion)
                  << " records from the full-text cache.\n";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));    
    }
}
