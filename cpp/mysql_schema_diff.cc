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
#include <memory>
#include "DbConnection.h"
#include "ExecUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "VuFind.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("db_name [username [password]] sql_file\n"
            "Compare an existing MySQL Database against a sql file with CREATE TABLE statements.\n"
            "Uses \"mysqldiff\" from \"mysql-utilities\".\n"
            "\n"
            "For specific values of db_name, username and password will be read from the following files if not provided:\n"
            "- ub_tools: " + DbConnection::DEFAULT_CONFIG_FILE_PATH + ".\n"
            "- vufind: " + VuFind::GetDefaultDatabaseConf() + ".\n"
            "\n");
}


void CleanupTemporaryDatabase(DbConnection * const db_connection, const std::string &temporary_db_name) {
    if (db_connection->mySQLDatabaseExists(temporary_db_name))
        db_connection->mySQLDropDatabase(temporary_db_name);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3 or argc > 5)
        Usage();

    const std::string MYSQLDIFF_EXECUTABLE(ExecUtil::Which("mysqldiff"));
    if (MYSQLDIFF_EXECUTABLE.empty())
        LOG_ERROR("Dependency \"mysqldiff\" is missing, please install \"mysql-utilities\"-package first!");

    std::shared_ptr<DbConnection> db_connection;
    const std::string db_name(argv[1]);
    bool manual_password_entry;
    if (argc >= 4) {
        const std::string user(argv[2]);
        std::string password;
        if (argc >= 5) {
            password = argv[3];
            --argc, ++argv;
            manual_password_entry = false;
        } else {
            password = MiscUtil::GetPassword("Please enter the MySQL password:");
            manual_password_entry = true;
        }

        for (unsigned retry_count(0); retry_count < 3; ++retry_count) {
            ++retry_count;
            try {
                db_connection.reset(new DbConnection(db_name, user, password));
                break;
            } catch (...) {
                if (manual_password_entry)
                    password = MiscUtil::GetPassword("Please enter the MySQL password again of abort w/ Ctrl-C:");
                else
                    throw;
            }
        }

        --argc, ++argv;
    } else if (std::strcmp(db_name.c_str(), "vufind") == 0)
        db_connection.reset(new DbConnection(VuFind::GetMysqlURL()));
    else if (std::strcmp(db_name.c_str(), "ub_tools") == 0)
        db_connection.reset(new DbConnection());
    else
        LOG_ERROR("You need to specify username and password for the database \"" + db_name +"\"!");
    const std::string sql_file(argv[2]);
    const std::string temporary_db_name(db_name + "_tempdiff");

    CleanupTemporaryDatabase(db_connection.get(), temporary_db_name);
    db_connection->mySQLCreateDatabase(temporary_db_name);
    db_connection->mySQLSelectDatabase(temporary_db_name);
    db_connection->queryFileOrDie(sql_file);

    const int exec_result(ExecUtil::Exec(MYSQLDIFF_EXECUTABLE,
                                         {
                                             "--force",
                                             "--server1=" + db_connection->getUser() + ":" + db_connection->getPasswd()
                                             + "@" + db_connection->getHost() + ":" + std::to_string(db_connection->getPort()),
                                             db_name + ":" + temporary_db_name
                                          }));

    CleanupTemporaryDatabase(db_connection.get(), temporary_db_name);
    return exec_result;
}
