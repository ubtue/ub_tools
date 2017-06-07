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
#include "DbConnection.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " no_of_months_db\n"
              << "       Removes all records from the full-text database whose last_used dates are older than\n"
              << "       \"no_of_months_db\" months.\n\n";
    std::exit(EXIT_FAILURE);
}


void ExpungeOldRecords(const unsigned no_of_months) {
    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    const time_t now(std::time(nullptr));
    const std::string cutoff_datetime(SqlUtil::TimeTToDatetime(now - no_of_months * 30 * 86400));
    const std::string DELETE_STMT("DELETE FROM full_text_cache WHERE last_used < \"" + cutoff_datetime + "\"");
    if (not db_connection.query(DELETE_STMT))
        throw std::runtime_error("Query \"" + DELETE_STMT + "\" failed because: "
                                 + db_connection.getLastErrorMessage());
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    unsigned no_of_months;
    if (not StringUtil::ToUnsigned(argv[1], &no_of_months))
        Error("no_of_months must be a number!");

    try {
        ExpungeOldRecords(no_of_months);
    } catch (const std::exception &e) {
        Error("caught exception: " + std::string(e.what()));
    }
}
