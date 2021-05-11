/** \brief Utility for extracting the first N records from a collection of MARC records.
 *         have some metadata for these items.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void ProcessRecords(const unsigned N, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        if (record_count == N)
            break;
        ++record_count;
        marc_writer->write(record);
    }

    LOG_INFO("Copied " + std::to_string(record_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("count marc_input marc_output");

    unsigned count;
    if (not StringUtil::ToUnsigned(argv[1], &count) or count == 0)
        LOG_ERROR("count must be a postive integer!");

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto marc_writer(MARC::Writer::Factory(argv[3]));
    ProcessRecords(count, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
