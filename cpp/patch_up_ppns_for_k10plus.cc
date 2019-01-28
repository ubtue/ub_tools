/** \file    patch_up_ppns_for_k10plus.cc
 *  \brief   Swaps out all persistent old PPN's with new PPN's.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019, Library of the University of Tübingen

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
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "ControlNumberGuesser.h"
#include "util.h"
#include "VuFind.h"


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] user_id journal_ppn1 [journal_ppn2 .. journal_ppnN]\n";
    std::exit(EXIT_FAILURE);
}


void LoadMapping(std::unordered_map<std::string, std::string> * const /*old_to_new_map*/) {
}


int Main(int /*argc*/, char **argv) {
    ::progname = argv[0];

    std::unordered_map<std::string, std::string> old_to_new_map;
    LoadMapping(&old_to_new_map);

    ControlNumberGuesser control_number_guesser;
    control_number_guesser.swapControlNumbers(old_to_new_map);

    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    return EXIT_SUCCESS;
}
