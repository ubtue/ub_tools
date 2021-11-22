/** \brief Test program for interfacing to the Sqlite3 tables.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DbConnection.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << '\n';
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 1)
        Usage();

    try {
        DbConnection db_connection(DbConnection::Sqlite3Factory("/tmp/test.sq3", DbConnection::CREATE));
        db_connection.queryFileOrDie("sqlite3_test.sq3");

        db_connection.queryOrDie("DELETE FROM contacts;");

        db_connection.queryOrDie(
            "INSERT INTO contacts"
            "    (contact_id, first_name, last_name, email, phone)"
            "VALUES"
            "   (1, 'Fred', 'Flintstone', 'fred@example.com', '999-999-9999'),"
            "   (2, 'Homer', 'Simpson', 'homer@example.com', '888-888-8888');"
        );

        db_connection.queryOrDie("SELECT contact_id, last_name FROM contacts;");
        DbResultSet result_set(db_connection.getLastResultSet());
        if (result_set.empty())
            std::cout << "Result set is empty!\n";
        else {
            std::cout << "Result set contains " << result_set.size() << " rows.\n";
            while (DbRow row = result_set.getNextRow())
                std::cout << "contact_id=" << row["contact_id"] << ", last_name=" << row["last_name"] << '\n';
        }
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
