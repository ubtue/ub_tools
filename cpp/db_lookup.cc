/** \file    db_lookup.cc
 *  \brief   A tool for database lookups in a key/value database.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2019 Library of the University of Tübingen

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
#include "KeyValueDB.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("db_path key");

    KeyValueDB db(argv[1]);
    std::cout << db.getValue(argv[2]) << '\n';

    return EXIT_SUCCESS;
}
