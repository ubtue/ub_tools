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
#include "DbConnection.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "usage: " << ::progname << '\n';
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 1)
        Usage();

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);

        // The following definition would throw an exception if the "mysql_url" was invalid:
        DbConnection db_connection(mysql_url);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));    
    }
}
