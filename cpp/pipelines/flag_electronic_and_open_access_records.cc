/** \brief Utility for adding an ELC field to all records of electronic/online resources.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0), flagged_as_electronic_count(0), flagged_as_open_access_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (record.getFirstField("ELC") == record.end()) {
            MARC::Subfields subfields;
            if (record.isElectronicResource())
                subfields.appendSubfield('a', "1");
            if (record.isPrintResource())
                subfields.appendSubfield('b', "1");
            if (not subfields.empty()) {
                ++flagged_as_electronic_count;
                record.insertField("ELC", subfields);
            }
        }

        if (record.getFirstField("OAS") == record.end()) {
            MARC::Subfields subfields;
            if (MARC::IsOpenAccess(record)) {
                subfields.appendSubfield('a', "1");
                ++flagged_as_open_access_count;
                record.insertField("OAS", subfields);
            }
        }

        marc_writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
    LOG_INFO("Flagged " + std::to_string(flagged_as_electronic_count) + " record(s) as electronic resource(s).");
    LOG_INFO("Flagged " + std::to_string(flagged_as_open_access_count) + " record(s) as open-access resource(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));

    ProcessRecords(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
