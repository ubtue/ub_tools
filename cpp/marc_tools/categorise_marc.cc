/** \file categorise_marc.cc
 *  \brief Determines the type of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Compiler.h"
#include "MARC.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


/** \brief Appends "source" to "target". */
void Categorise(MARC::Reader * const marc_reader) {
    while (const MARC::Record record = marc_reader->read()) {
        switch (record.getRecordType()) {
        case MARC::Record::RecordType::AUTHORITY:
            std::cout << "AUTHORITY\n";
            break;
        case MARC::Record::RecordType::BIBLIOGRAPHIC:
            std::cout << "BIBLIOGRAPHIC\n";
            break;
        case MARC::Record::RecordType::CLASSIFICATION:
            std::cout << "CLASSIFICATION\n";
            break;
        case MARC::Record::RecordType::UNKNOWN:
            std::cout << "UNKNOWN\n";
            break;
        }
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    try {
        Categorise(marc_reader.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
