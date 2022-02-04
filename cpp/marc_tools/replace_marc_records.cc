/** \brief Utility for replacing MARC records in one file with records from another file with the same control number.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " reference_records source_records target_records\n"
              << "       Replaces all records in \"source_records\" that have an identical control number\n"
              << "       as a record in \"reference_records\" with the corresponding record in\n"
              << "       \"reference_records\".  The file with the replacements as well as any records\n"
              << "       that could not be replaced is the output file \"target_records\".\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessSourceRecords(MARC::Reader * const marc_source_reader, MARC::Reader * const marc_reference_reader,
                          MARC::Writer * const marc_writer, const std::unordered_map<std::string, off_t> &control_number_to_offset_map) {
    unsigned source_record_count(0), replacement_count(0);
    while (const MARC::Record source_record = marc_source_reader->read()) {
        ++source_record_count;

        const auto control_number_and_offset(control_number_to_offset_map.find(source_record.getControlNumber()));
        if (control_number_and_offset == control_number_to_offset_map.cend()) { // No replacement found.
            marc_writer->write(source_record);
            continue;
        }

        if (unlikely(not marc_reference_reader->seek(control_number_and_offset->second)))
            logger->error("failed to seek in reference records! (offset: " + std::to_string(control_number_and_offset->second));

        const MARC::Record reference_record(marc_reference_reader->read());
        marc_writer->write(reference_record);
        ++replacement_count;
    }

    std::cout << "Read " << source_record_count << " source records.\n";
    std::cout << "Replaced " << replacement_count << " records.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reference_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Reader> marc_source_reader(MARC::Reader::Factory(argv[2]));
    std::unique_ptr<MARC::Writer> marc_target_writer(MARC::Writer::Factory(argv[3]));

    std::unordered_map<std::string, off_t> control_number_to_offset_map;
    std::cout << "Read " << MARC::CollectRecordOffsets(marc_reference_reader.get(), &control_number_to_offset_map)
              << " reference records.\n";

    ProcessSourceRecords(marc_source_reader.get(), marc_reference_reader.get(), marc_target_writer.get(), control_number_to_offset_map);

    return EXIT_SUCCESS;
}
