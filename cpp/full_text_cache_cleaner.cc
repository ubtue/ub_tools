/** \brief Tool to delete old cache entries from the KrimDok full text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <SqlUtil.h>
#include <StringUtil.h>
#include <TimeUtil.h>
#include <util.h>
#include <VuFind.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " max_last_used_age\n";
    std::cerr << "       Deletes all records in the full text cache that are older than \"max_last_used_age\".\n";
    std::cerr << "       \"max_last_used_age\" is in days.\n";
    std::exit(EXIT_FAILURE);
}


unsigned GetTableSize(DbConnection * const connection, const std::string &table_name) {
    connection->queryOrDie("SELECT COUNT(*) FROM " + table_name);
    DbResultSet result_set(connection->getLastResultSet());
    const DbRow first_row(result_set.getNextRow());

    return StringUtil::ToUnsigned(first_row[0]);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();
    
    unsigned age_in_days;
    if (not StringUtil::ToUnsigned(argv[1], &age_in_days))
        Error("max_last_used_age is not a valid unsigned number!");
    const time_t now(std::time(nullptr));
    const time_t cutoff_time(TimeUtil::AddDays(now, -static_cast<int>(age_in_days)));
    const std::string cutoff_datetime(SqlUtil::TimeTToDatetime(cutoff_time));

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);

        DbConnection db_connection(mysql_url);
        const unsigned size_before_deletion(GetTableSize(&db_connection, "full_text_cache"));

        db_connection.queryOrDie("DELETE FROM full_text_cache WHERE last_used < \"" + cutoff_datetime + "\"");

        const unsigned size_after_deletion(GetTableSize(&db_connection, "full_text_cache"));
        std::cout << "Expired " << (size_before_deletion - size_after_deletion)
                  << " records from the full_text_cache table.\n";
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));    
    }
}
