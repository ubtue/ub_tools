/** \file    fifo_pipe_file_test.cc
 *  \brief   Test harness for the new functionality of the File class where we can compress or uncompress an input or
 *           output more or less transparently.
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
#include <unistd.h>
#include "File.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " mode input_file_name output_file_name\n";
    std::cerr << "       Where \"mode\" has to be either \"compress\" or \"decompress\".\n";
    std::cerr << "       The compressed or uncompressed data is then written to stdout.\n";
    std::exit(EXIT_FAILURE);
}


void Compress(const std::string &input_filename, const std::string &output_filename) {
    File input(input_filename, "r");\
    File output(output_filename, "wc");
    int ch;
    while ((ch = input.get()) != EOF)
        output.put(static_cast<char>(ch));
}


void Decompress(const std::string &input_filename, const std::string &output_filename) {
    File input(input_filename, "ru");\
    File output(output_filename, "w");
    int ch;
    while ((ch = input.get()) != EOF)
        output.put(static_cast<char>(ch));
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();
    const std::string mode(argv[1]);

    try {
        if (mode == "compress")
            Compress(argv[2], argv[3]);
        else if (mode == "decompress")
            Decompress(argv[2], argv[3]);
        else
            Usage();
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
