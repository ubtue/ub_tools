/** \file    db_lookup.cc
 *  \brief   A tool for database lookups in a kyotokabinet key/value database.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015,2017 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <cstdlib>
#include <kchashdb.h>
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " db_path key\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3)
        Usage();

    kyotocabinet::HashDB db;
    if (not db.open(argv[1], kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::ONOLOCK))
        logger->error("Failed to open database \"" + std::string(argv[1]) + "\" for reading ("
                      + std::string(db.error().message()) + ")!");

    std::string data;
    if (not db.get(argv[2], &data))
        logger->error("Lookup failed: " + std::string(db.error().message()));

    std::cout << data;

    db.close();
}
