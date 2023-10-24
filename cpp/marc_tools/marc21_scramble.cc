/** \brief Utility for randomizing the order of records in a MARC-21 collection.
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

#include <algorithm>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc21_input marc21_output\n";
    std::exit(EXIT_FAILURE);
}


void WriteRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, const std::vector<std::string> &control_numbers,
                  const std::unordered_map<std::string, off_t> &control_number_to_offset_map) {
    for (const auto &control_number : control_numbers) {
        const auto control_number_and_offset(control_number_to_offset_map.find(control_number));
        if (unlikely(control_number_and_offset == control_number_to_offset_map.cend()))
            LOG_ERROR("this should *never* happen!");
        if (unlikely(not marc_reader->seek(control_number_and_offset->second)))
            LOG_ERROR("failed to seek to offset " + std::to_string(control_number_and_offset->second) + " in \"" + marc_reader->getPath()
                      + "\"!");
        const MARC::Record record = marc_reader->read();
        marc_writer->write(record);
    }

    std::cout << "Scrambled " << control_numbers.size() << " MARC record(s).\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1], MARC::FileType::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2], MARC::FileType::BINARY));

    std::unordered_map<std::string, off_t> control_number_to_offset_map;
    MARC::CollectRecordOffsets(marc_reader.get(), &control_number_to_offset_map);

    std::vector<std::string> control_numbers;
    control_numbers.reserve(control_number_to_offset_map.size());
    for (const auto &control_number_and_offset : control_number_to_offset_map)
        control_numbers.emplace_back(control_number_and_offset.first);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(control_numbers.begin(), control_numbers.end(), g);

    WriteRecords(marc_reader.get(), marc_writer.get(), control_numbers, control_number_to_offset_map);

    return EXIT_SUCCESS;
}
