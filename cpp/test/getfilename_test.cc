/** \file    xml_parser_test.cc
 *  \brief   Tests the FileUtil::GetFileName() function.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2017, Library of the University of Tübingen

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
#include <stdexcept>
#include "FileUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " path\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    try {
        FILE *file(std::fopen(argv[1], "r"));
        std::cout << "File pointer points to \"" << FileUtil::GetFileName(file) << "\".\n";
    } catch(const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }  
}
