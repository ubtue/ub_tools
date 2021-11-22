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
                                  + " " + db_connection->mySQLGetDbName() + "." + row1[0]);
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


void ProcessProcedures(DbConnection * const db_connection, const std::string &database_name) {
    db_connection->queryOrDie("SHOW PROCEDURE STATUS");
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        if (row["Db"] == database_name)
            std::cout << "PROCEDURE NAME `" << row["Name"] << "`;\n";
    }
}


int Main(int argc, char *argv[]) {
    std::string database_name, user, passwd, host("localhost");
    unsigned port(MYSQL_PORT);
    switch (argc) {
    case 1:
        break;
    case 6:
        port = StringUtil::ToUnsigned(argv[5]);
    case 5:
        host = argv[4];
    case 4:
        passwd = argv[3];
    case 3:
        user = argv[2];
        database_name = argv[1];
        break;
    default:
        Usage();
    }

    DbConnection db_connection(database_name.empty()
                                   ? DbConnection::UBToolsFactory()
                                   : DbConnection::MySQLFactory(database_name, user, passwd, host, port));

    ProcessTablesOrViews(&db_connection, /* process_tables = */true);
    ProcessTablesOrViews(&db_connection, /* process_tables = */false);
    ProcessTriggers(&db_connection, argc == 1 ? "ub_tools" : argv[1]);
    ProcessProcedures(&db_connection, argc == 1 ? "ub_tools" : argv[1]);

    return EXIT_SUCCESS;
}
