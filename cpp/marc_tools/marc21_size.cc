/** \brief Very fast record counter for MARC-21.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <cstdlib>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Compiler.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc21_data\n";
    std::exit(EXIT_FAILURE);
}


void CountRecords(const std::string &filename) {
    const int fd(::open(filename.c_str(), O_RDONLY));
    if (fd == -1)
        LOG_ERROR("Failed op open \"" + filename + "\" for reading!");

    struct stat stat_buf;
    if (::fstat(fd, &stat_buf) == -1)
        LOG_ERROR("Failed to stat \"" + filename + "\"!");

    const char * const map(reinterpret_cast<char *>(::mmap(nullptr, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)));
    if (map == MAP_FAILED)
        LOG_ERROR("Failed to mmap \"" + filename + "\"!");

    size_t record_count(0);
    for (off_t i(0); i < stat_buf.st_size; ++i) {
        if (unlikely(map[i] == '\x1D'))
            ++record_count;
    }

    std::cout << filename << " contains " << record_count << " MARC-21 record(s).\n";
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    CountRecords(argv[1]);
}
