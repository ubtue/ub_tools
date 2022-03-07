/** \brief Utility for displaying the count of MARC records contained in a collection.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void CountRecords(MARC::Reader * const marc_reader) {
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read())
        ++record_count;
    std::cout << record_count << '\n';
}


} // unnamed namespace

int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    CountRecords(marc_reader.get());
    return EXIT_SUCCESS;
}
