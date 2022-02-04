/** \brief Utility for removing MARC records from a collection if we likely already
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
#include "ControlNumberGuesser.h"
#include "MARC.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), dropped_record_count(0);
    ControlNumberGuesser control_number_guesser;

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
        const auto guessed_control_numbers(control_number_guesser.getGuessedControlNumbers(
            record.getCompleteTitle(), record.getAllAuthors(), record.getMostRecentPublicationYear(), record.getDOIs(), record.getISSNs(),
            record.getISBNs()));
        if (guessed_control_numbers.empty())
            marc_writer->write(record);
        else
            ++dropped_record_count;
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) and dropped " + std::to_string(dropped_record_count)
             + " likely duplicate(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
