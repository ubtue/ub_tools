/** \file    remove_matching_files_test.cc
 *  \brief   Tests the FileUtil::RemoveMatchingFiles() function.
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
#include <stdexcept>
#include "FileUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " [--include-directories] filename_regex [directory_to_scan]\n";
    std::cerr << "       If \"--include-directories\" has been specified, matching directories will be\n";
    std::cerr << "       recursively deleted. If \"directory_to_scan\" has been provided that diectory will\n";
    std::cerr << "       will be scanned for matching files.  If not, the current working directory will be\n";
    std::cerr << "       scanned.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 2)
        Usage();

    bool include_directories(false);
    if (std::strcmp(argv[1], "--include-directories") == 0) {
        include_directories = true;
        --argc;
        ++argv;
    }

    if (argc != 2 and argc != 3)
        Usage();
    const std::string directory_to_scan(argc == 2 ? "." : argv[2]);

    try {
        const std::string filename_regex(argv[1]);
        std::cout << "filename_regex = " << filename_regex << '\n';
        std::cout << "directory_to_scan = " << directory_to_scan << '\n';
        const ssize_t count(FileUtil::RemoveMatchingFiles(filename_regex, include_directories, directory_to_scan));
        if (count == -1)
            Error("failed to delete one or more matching files or directories!");
        std::cout << "Deleted " << count << " matching files and or directories.\n";
    } catch(const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }  
}
