/** \brief Utility for displaying the count of MARC records contained in a collection.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void CountRecords(MarcReader * const marc_reader) {
    unsigned record_count(0);
    while (const MarcRecord record = marc_reader->read())
        ++record_count;
    std::cout << record_count << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        CountRecords(marc_reader.get());
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
