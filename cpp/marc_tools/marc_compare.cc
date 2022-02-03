/** \brief A tool to compare two marc files, regardless of the file format.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
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
#include <string>
#include "Compiler.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_lhs marc_rhs\n\n";
    std::exit(EXIT_FAILURE);
}


void Compare(MARC::Reader * const lhs_reader, MARC::Reader * const rhs_reader) {
    while (true) {
        const MARC::Record lhs(lhs_reader->read());
        const MARC::Record rhs(rhs_reader->read());

        if (unlikely(not lhs and not rhs))
            return;
        if (unlikely(not lhs))
            LOG_ERROR(lhs_reader->getPath() + " has fewer records than " + rhs_reader->getPath());
        if (unlikely(not rhs))
            LOG_ERROR(lhs_reader->getPath() + " has more records than " + rhs_reader->getPath());

        if (lhs.getControlNumber() != rhs.getControlNumber())
            LOG_ERROR("PPN mismatch:\nLHS: " + lhs.getControlNumber() + "\nRHS: " + rhs.getControlNumber());

        if (lhs.getNumberOfFields() != rhs.getNumberOfFields()) {
            LOG_ERROR("Number of fields (" + lhs.getControlNumber() + "):\nLHS: " + std::to_string(lhs.getNumberOfFields())
                      + "\nRHS: " + std::to_string(rhs.getNumberOfFields()));
        }

        for (auto lhs_field(lhs.begin()), rhs_field(rhs.begin()); lhs_field != lhs.end() and rhs_field != rhs.end();
             ++lhs_field, ++rhs_field) {
            if (lhs_field->getTag() != lhs_field->getTag()) {
                LOG_ERROR("Tag mismatch (" + lhs.getControlNumber() + "):\nLHS: " + lhs_field->getTag().toString()
                          + "\nRHS: " + rhs_field->getTag().toString());
            }

            std::string lhs_data(lhs_field->getContents());
            std::string rhs_data(rhs_field->getContents());
            while (lhs_data.find("\x1F") != std::string::npos)
                lhs_data.replace(lhs_data.find("\x1F"), 1, " $");
            while (rhs_data.find("\x1F") != std::string::npos)
                rhs_data.replace(rhs_data.find("\x1F"), 1, " $");
            if (lhs_data.compare(rhs_data)) {
                LOG_ERROR("Subfield mismatch (" + lhs.getControlNumber() + ", Tag: " + lhs_field->getTag().toString()
                          + "): \nLHS:" + lhs_data + "\nRHS:" + rhs_data);
            }
        }
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    auto lhs_reader(MARC::Reader::Factory(argv[1]));
    auto rhs_reader(MARC::Reader::Factory(argv[2]));

    Compare(lhs_reader.get(), rhs_reader.get());
    return EXIT_SUCCESS;
}
