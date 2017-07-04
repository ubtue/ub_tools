/** \brief Utility for downloading PDFs of essay collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <map>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "Subfields.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MarcReader * const marc_reader) {
    unsigned record_count(0);//, until1999_count(0), from2000_to_2009_count(0), after2009_count(0);

    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;

        const std::string _655_contents(record.getFieldData("655"));
        if (_655_contents.empty())
            continue;
        const Subfields _655_subfields(_655_contents);
        if (_655_subfields.getIndicator1() != ' ' or _655_subfields.getIndicator2() != '7')
            continue;
        if (not _655_subfields.hasSubfieldWithValue('a', "Aufsatzsammlung"))
            continue;
        
        const std::string _856_contents(record.getFieldData("856"));
        if (_856_contents.empty())
            continue;
        const Subfields _856_subfields(_856_contents);
        if (unlikely(not _856_subfields.hasSubfield('u'))
            or not _856_subfields.hasSubfieldWithValue('3', "Inhaltsverzeichnis"))
            continue;
        const std::string url(_856_subfields.getFirstSubfieldValue('u'));
        const std::string &control_number(record.getControlNumber());
        std::cout << control_number << ": " << url << '\n';
    }

    std::cout << "Data set contains " << record_count << " MARC record(s).\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        ProcessRecords(marc_reader.get());
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
