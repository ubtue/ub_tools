/** \brief Utility for displaying publication years and titles of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader) {
    while (const MARC::Record record = marc_reader->read()) {
        const auto publication_year(record.getMostRecentPublicationYear());
        std::cout << (publication_year.empty() ? "????" : publication_year) << ": " << record.getMainTitle() << '\n';
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage("marc_data1 [marc_data2 .. marc_dataN]");

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        const std::string filename(argv[arg_no]);
        auto marc_reader(MARC::Reader::Factory(filename));
        ProcessRecords(marc_reader.get());
    }

    return EXIT_SUCCESS;
}
