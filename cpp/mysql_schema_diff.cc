/** \file mysql_schema_diff.cc
 *  \brief A tool for comparing a sql file with create table statements against an existing database, using mysqldiff.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
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
#include "DbConnection.h"
#include "ExecUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("username password db_name sql_file\n\n"
            "Compare an existing MySQL Database against a sql file with CREATE TABLE statements.\n"
            "Uses \"mysqldiff\" from \"mysql-utilities\".");
}


void CleanupTemporaryDatabase(DbConnection &db_connection, const std::string &database_name_temporary) {
    if (db_connection.mySQLDatabaseExists(database_name_temporary))
        db_connection.mySQLDropDatabase(database_name_temporary);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        Usage();

    const std::string MYSQLDIFF_EXECUTABLE(ExecUtil::Which("mysqldiff"));
    if (MYSQLDIFF_EXECUTABLE.empty())
        LOG_ERROR("Dependency \"mysqldiff\" is missing, please install \"mysql-utilities\"-package first!");

    const std::string user(argv[1]);
    const std::string passwd(argv[2]);
    const std::string database_name(argv[3]);
    const std::string sql_file(argv[4]);
    const std::string host("localhost");
    const unsigned port(MYSQL_PORT);

    const std::string temporary_database_name(database_name + "_tempdiff");

    DbConnection db_connection(database_name, user, passwd);
    CleanupTemporaryDatabase(db_connection, temporary_database_name);
    db_connection.mySQLCreateDatabase(temporary_database_name);
    db_connection.mySQLSelectDatabase(temporary_database_name);
    db_connection.queryFileOrDie(sql_file);

    const int exec_result(ExecUtil::Exec(MYSQLDIFF_EXECUTABLE,
                                         {
                                             "--force",
                                             "--server1=" + user + ":" + passwd + "@" + host + ":" + std::to_string(port),
                                             database_name + ":" + temporary_database_name
                                          }));

    CleanupTemporaryDatabase(db_connection, temporary_database_name);
    return exec_result;
}
