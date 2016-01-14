/** \file    get_file_media_type_test.cc
 *  \brief   Test harness for the new MediaTypeUtil::GetFileMediaType() function.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright 2016 Universitätsbibliothek Tübingen.

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
#include "MediaTypeUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
	if (argc != 2)
	    Usage();

	std::cout << MediaTypeUtil::GetFileMediaType(argv[1]) << '\n';
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}

