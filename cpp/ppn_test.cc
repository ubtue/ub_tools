/** \file    ppn_test.cc
 *  \brief   Test harness for MiscUtil::IsValidPPN().
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include "MiscUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " ppn_candidate\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();
    const std::string ppn_candidate(argv[1]);

    if (MiscUtil::IsValidPPN(ppn_candidate)) {
        std::cout << ppn_candidate << " is a valid PPN.\n";
        return EXIT_SUCCESS;
    } else {
        std::cout << ppn_candidate << " is not a valid PPN.\n";
        return EXIT_FAILURE;
    }
}

