/** \brief Utility for adding an ELC field to all records of electronic/online resources.
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
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--input-format=(marc_binary|marc_xml)] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), flagged_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (record.isElectronicResource() and record.getFirstField("ELC") == record.end()) {
            record.insertField("ELC", { { 'a', "1" } });
            ++flagged_count;
        }

        marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
    LOG_INFO("Flagged " + std::to_string(flagged_count) + " record(s) as electronic resource(s).");
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    MARC::Reader::ReaderType reader_type(MARC::Reader::AUTO);
    if (argc == 4) {
        if (std::strcmp(argv[1], "--input-format=marc-21") == 0)
            reader_type = MARC::Reader::BINARY;
        else if (std::strcmp(argv[1], "--input-format=marc-xml") == 0)
            reader_type = MARC::Reader::XML;
        else
            Usage();
        ++argv, --argc;
    }

    try {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1], reader_type));
        std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));

        ProcessRecords(marc_reader.get(), marc_writer.get());
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
