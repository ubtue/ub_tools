/** \brief Test program for interfacing to the VuFind MySQL tables.
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
#include <string>
#include <cstdlib>
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <File.h>
#include <StringUtil.h>
#include <util.h>
#include <VuFind.h>


void Usage() {
    std::cerr << "usage: " << ::progname << '\n';
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 1)
	Usage();

    try {
	const std::string database_conf_filename(VuFind::VUFIND_HOME + "/" + VuFind::DATABASE_CONF);
	File database_conf(database_conf_filename, "r", File::THROW_ON_ERROR);
	const std::string line(database_conf.getline());
	const size_t schema_pos(line.find("mysql://"));
	if (schema_pos == std::string::npos)
	    Error("MySQL schema not found in \"" + database_conf_filename + "\"!");
	std::string mysql_url(StringUtil::RightTrim(line.substr(schema_pos)));
	mysql_url.resize(mysql_url.size() - 1); // Remove trailing double quote.

	// The following definition would throw an exception if the "mysql_url" was invalid:
	DbConnection db_connection(mysql_url);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));	
    }
}
