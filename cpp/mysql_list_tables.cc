/** \file mysql_list_tables.cc
 *  \brief A tool for listing the schemas of all tables in a MySQL database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "StringUtil.h"
#include "util.h"


[[noreturn]] static void Usage() {
    ::Usage("[database_name user [password [host [port]]]]");
}


void ProcessTablesOrViews(DbConnection * const db_connection, const bool process_tables) {
    db_connection->queryOrDie("SHOW FULL TABLES WHERE Table_Type = '"
                              + std::string(process_tables ? "BASE TABLE" : "VIEW") + "'");
    std::vector<std::string> table_names;

    DbResultSet result_set1(db_connection->getLastResultSet());
    DbRow row1;
    while (row1 = result_set1.getNextRow()) {
        db_connection->queryOrDie("SHOW CREATE " + std::string(process_tables ? "TABLE" : "VIEW")
                                  + " " + db_connection->getDbName() + "." + row1[0]);
        DbResultSet result_set2(db_connection->getLastResultSet());
        DbRow row2;
        while (row2 = result_set2.getNextRow())
            std::cout << row2[1] << '\n';
    }
}


void ProcessTriggers(DbConnection * const db_connection, const std::string &database_name) {
    db_connection->queryOrDie("SHOW TRIGGERS FROM `" + database_name + "`");
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        std::cout << "CREATE TRIGGER DEFINER `" << row.getValue("Definer") << "` `" << row["Trigger"]
                  << "` ON `" << database_name << '.' << row["Table"] << "` " << row["Timing"] << ' '
                  << row["Event"] << ' ' << row["Statement"] << ";\n";
}


int Main(int argc, char *argv[]) {
    if (argc != 1 and argc != 3 and argc != 4 and argc != 5 and argc != 6)
        Usage();

    std::unique_ptr<DbConnection> db_connection;
    switch (argc) {
    case 1:
        db_connection.reset(new DbConnection());
        break;
    case 3:
        db_connection.reset(new DbConnection(argv[1], argv[2]));
        break;
    case 4:
        db_connection.reset(new DbConnection(argv[1], argv[2], argv[3]));
        break;
    case 5:
        db_connection.reset(new DbConnection(argv[1], argv[2], argv[3], argv[4]));
        break;
    case 6:
        db_connection.reset(new DbConnection(argv[1], argv[2], argv[3], argv[4], StringUtil::ToUnsigned(argv[5])));
        break;
    }

    ProcessTablesOrViews(db_connection.get(), /* process_tables = */true);
    ProcessTablesOrViews(db_connection.get(), /* process_tables = */false);
    ProcessTriggers(db_connection.get(), "vufind");
    ProcessTriggers(db_connection.get(), "ub_tools");

    return EXIT_SUCCESS;
}
