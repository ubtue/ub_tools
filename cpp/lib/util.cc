/** \file    util.cc
 *  \brief   Implementation of various utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

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
#include "util.h"
#include <iostream>
#include <cstdio>


char *progname; // Must be set in main() with "progname = argv[0];";


void Error(const std::string &msg) {
    if (progname == NULL)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Error().\n";
    else
        std::cerr << progname << ": " << msg << '\n';
    std::exit(EXIT_FAILURE);
}


void Warning(const std::string &msg) {
    if (progname == NULL)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Warning().\n";
    else
        std::cerr << progname << ": " << msg << '\n';
}


bool ReadFile(const std::string &filename, std::string * const contents) {
    FILE *input(std::fopen(filename.c_str(), "r"));
    if (input == NULL)
        return false;

    contents->clear();
    while (not std::feof(input)) {
        char buf[BUFSIZ];
        const size_t byte_count(std::fread(buf, 1, sizeof buf, input));
        if (byte_count != sizeof(buf) and std::ferror(input)) {
            std::fclose(input);
            return false;
        }
        contents->append(buf, byte_count);
    }

    std::fclose(input);
    return true;
}
