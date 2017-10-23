/** \file    remove_directory_test.cc
 *  \brief   A test harness for the FileUtil::RemoveDirectory function.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2017, Library of the University of Tübingen

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
#include <cstring>
#include "FileUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " directory_name\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string directory_name(argv[1]);
    if (not FileUtil::RemoveDirectory(directory_name))
        logger->error("failed to recursively delete \"" + directory_name + "\"! (" + std::string(::strerror(errno)) + ")");
}
