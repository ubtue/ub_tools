/** \file    normalise_title.cc
 *  \brief   Spits out a version of a title that is used for a matching heuristic.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2018, Library of the University of Tübingen
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
#include <cstdlib>
#include "ControlNumberGuesser.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] title\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    std::cout << ControlNumberGuesser::NormaliseTitle(argv[1]) << '\n';
    return EXIT_SUCCESS;
}
