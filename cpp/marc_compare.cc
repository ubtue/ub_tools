/** \brief A tool to compare two marc files, regardless of the file format.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MediaTypeUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_lhs marc_rhs\n\n";
    std::exit(EXIT_FAILURE);
}


void Compare(MarcReader * const lhs_reader, MarcReader * const rhs_reader) {
    while (true) {
        const MarcRecord lhs(lhs_reader->read());
        const MarcRecord rhs(rhs_reader->read());

        if (unlikely(not lhs and not rhs))
            return;
        if (unlikely(not lhs))
            logger->error(lhs_reader->getPath() + " has fewer records than " + rhs_reader->getPath());
        if (unlikely(not rhs))
            logger->error(lhs_reader->getPath() + " has more records than " + rhs_reader->getPath());

        if (lhs.getControlNumber() != rhs.getControlNumber())
            logger->error("PPN mismatch:\nLHS: " + lhs.getControlNumber() + "\nRHS: " + rhs.getControlNumber());

        if (lhs.getNumberOfFields() != rhs.getNumberOfFields())
            logger->error("Number of fields (" + lhs.getControlNumber() + "):\nLHS: "
                  + std::to_string(lhs.getNumberOfFields()) + "\nRHS: " + std::to_string(rhs.getNumberOfFields()));

        for (size_t index(0); index < lhs.getNumberOfFields(); ++index) {
            if (lhs.getTag(index) != rhs.getTag(index))
                logger->error("Tag mismatch (" + lhs.getControlNumber() + "):\nLHS: " + lhs.getTag(index).to_string()
                      + "\nRHS: " + rhs.getTag(index).to_string());

            std::string lhs_data(lhs.getFieldData(index));
            std::string rhs_data(rhs.getFieldData(index));
            while (lhs_data.find("\x1F") != std::string::npos)
                lhs_data.replace(lhs_data.find("\x1F"), 1, " $");
            while (rhs_data.find("\x1F") != std::string::npos)
                rhs_data.replace(rhs_data.find("\x1F"), 1, " $");
            if (lhs_data.compare(rhs_data))
                logger->error("Subfield mismatch (" + lhs.getControlNumber() + ", Tag: "
                              + lhs.getTag(index).to_string() + "): \nLHS:" + lhs_data + "\nRHS:" + rhs_data);
        }
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    std::unique_ptr<MarcReader> lhs_reader(MarcReader::Factory(argv[1]));
    std::unique_ptr<MarcReader> rhs_reader(MarcReader::Factory(argv[2]));

    Compare(lhs_reader.get(), rhs_reader.get());
}
