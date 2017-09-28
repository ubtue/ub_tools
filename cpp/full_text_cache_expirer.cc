/** \brief Utility for expunging old records from our full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include <kchashdb.h>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " no_of_months full_text_db\n"
              << "       Removes all records from the full-text database whose last_used dates are older than\n"
              << "       \"no_of_months\" months.\n\n";
    std::exit(EXIT_FAILURE);
}


// Returns the date of the oldest entry in our table or the empty string if the table was empty.
std::string GetDateOfOldestEntry(DbConnection * const db_connection) {
    const std::string SELECT_STMT("SELECT MAX(last_used) AS max_last_used FROM full_text_cache");
    db_connection->queryOrDie(SELECT_STMT);
    DbResultSet result_set(db_connection->getLastResultSet());

    return result_set.empty() ? "" : result_set.getNextRow()["max_last_used"];
}


void DeleteOldEntriesFromTheKeyValueDatabase(DbConnection * const db_connection, const std::string &full_text_db_path)
{
    kyotocabinet::HashDB db;
    if (not db.open(full_text_db_path, kyotocabinet::HashDB::OWRITER))
        Error("in DeleteOldEntriesFromTheKeyValueDatabase: Failed to open database \"" + full_text_db_path
              + "\" for writing (" + std::string(db.error().message()) + ")!");

    DbResultSet result_set(db_connection->getLastResultSet());
    while (const DbRow row = result_set.getNextRow()) {
        if (unlikely(not db.remove(row["key_value_db_key"])))
            Error("in DeleteOldEntriesFromTheKeyValueDatabase: Failed to delete an entry w/ key \""
                  + row["key_value_db_key"] + "\" from the database \"" + full_text_db_path + "\"!");
    }

    if (unlikely(not db.defrag()))
        Error("in DeleteOldEntriesFromTheKeyValueDatabase: Failed to defragment the database \"" + full_text_db_path
              + "\"!");
}


void ExpungeOldRecords(const unsigned no_of_months, const std::string &full_text_db_path) {
    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    const std::string oldest_date(GetDateOfOldestEntry(&db_connection));
    if (oldest_date.empty())
        std::cout << "The \"full_text_cache\" table was empty!\n";
    else {
        const time_t now(std::time(nullptr));
        const std::string cutoff_datetime(SqlUtil::TimeTToDatetime(now - no_of_months * 30 * 86400));
        db_connection.queryOrDie("SELECT key_value_db_key WHERE created < \"" + cutoff_datetime + "\"");
        DeleteOldEntriesFromTheKeyValueDatabase(&db_connection, full_text_db_path);
        const std::string DELETE_STMT("DELETE FROM full_text_cache WHERE created < \"" + cutoff_datetime + "\"");
        db_connection.queryOrDie(DELETE_STMT);
        std::cout << "Deleted " << db_connection.getNoOfAffectedRows() << " rows from the cache.\n";
        std::cout << "The date of the oldest entry was " << oldest_date << ".\n";
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    unsigned no_of_months;
    if (not StringUtil::ToUnsigned(argv[1], &no_of_months))
        Error("no_of_months must be a number!");

    try {
        ExpungeOldRecords(no_of_months, argv[2]);
    } catch (const std::exception &e) {
        Error("caught exception: " + std::string(e.what()));
    }
}
