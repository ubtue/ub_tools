/** \file mysql_list_schemas.cc
 *  \brief A tool for listing the schemas of all tables in a MySQL database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DbResultSet.h"
#include "DbRow.h"
#include "util.h"


int Main(int argc, char */*argv*/[]) {
    if (argc != 1)
        ::Usage("");

    DbConnection db_connection;
    db_connection.queryOrDie("SHOW TABLES");
    std::vector<std::string> table_names;

    DbResultSet result_set1(db_connection.getLastResultSet());
    DbRow row1;
    while (row1 = result_set1.getNextRow()) {
        db_connection.queryOrDie("SHOW CREATE TABLE ub_tools." + row1[0]);
        DbResultSet result_set2(db_connection.getLastResultSet());
        DbRow row2;
        while (row2 = result_set2.getNextRow())
            std::cout << row2[1] << '\n';
    }

    return EXIT_SUCCESS;
}
