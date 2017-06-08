/** \brief Test program for the new MySQL Db* classes.
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
#include <cstdlib>
#include <cstring>
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " [--raw] mysql_user mysql_passwd mysql_db mysql_query\n";
    std::cerr << "       Please note that \"mysql_query\" has to be a query that produces a result set.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6)
        Usage();

    bool raw(false);
    if (argc == 6) {
        if (std::strcmp(argv[1], "--raw") != 0)
            Usage();
        raw = true;
        ++argv;
    }

    const std::string user(argv[1]);
    const std::string passwd(argv[2]);
    const std::string db(argv[3]);
    const std::string query(argv[4]);

    try {
        DbConnection connection(db, user, passwd);
        connection.queryOrDie(query);
        DbResultSet result_set(connection.getLastResultSet());
        if (not raw)
            std::cout << "The number of rows in the result set is " << result_set.size() << ".\n";

        DbRow row;
        while (row = result_set.getNextRow()) {
            const size_t field_count(row.size());
            if (not raw)
                std::cout << "The current row has " << field_count << " fields.\n";
            for (unsigned field_no(0); field_no < field_count; ++ field_no) {
                const std::string column(row[field_no]);
                if (raw)
                    std::cout.write(column.data(), column.size());
                else
                    std::cout << "Field no. " << (field_no + 1) << " is \"" << column << "\".\n";
            }
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
