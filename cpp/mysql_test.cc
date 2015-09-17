/** \brief Test program for the new MySQL Db* classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " mysql_user mysql_passwd mysql_db mysql_query\n";
    std::cerr << "       Please note that \"mysql_query\" has to be a query that produces a result set.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
	Usage();

    const std::string user(argv[1]);
    const std::string passwd(argv[2]);
    const std::string db(argv[3]);
    const std::string query(argv[4]);

    try {
	DbConnection connection(db, user, passwd);
	if (not connection.query(query))
	    Error("Query failed: \"" + query + "\" (" + connection.getLastErrorMessage() + ")!");

	DbResultSet result_set(connection.getLastResultSet());
	std::cout << "The number of rows in the result set is " << result_set.size() << ".\n";

	DbRow row;
	while (row = result_set.getNextRow()) {
	    const size_t field_count(row.size());
	    std::cout << "The current row has " << field_count << " fields.\n";
	    for (unsigned field_no(0); field_no < field_count; ++ field_no)
		std::cout << "Field no. " << (field_no + 1) << " is \"" << row[field_no] << "\".\n";
	}
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
