/** \brief Test program dealing with a record that exceeds 99999 bytes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " (xml|binary) output\n";
    std::exit(EXIT_FAILURE);
}


std::string IncrementTag(const std::string &tag) {
    std::string next_tag(tag);

    if (next_tag[2] < '9') {
        next_tag[2] += 1; // Hurray for ASCII!
        return next_tag;
    }

    if (next_tag[1] < '9') {
        next_tag[2] = '0';
        next_tag[1] += 1; // Hurray for ASCII!
        return next_tag;
    }

    if (next_tag[0] < '9') {
        next_tag[2] = '0';
        next_tag[1] = '0';
        next_tag[0] += 1; // Hurray for ASCII!
        return next_tag;
    }

    LOG_ERROR("overflow in IncrementTag()!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    MARC::Record record("     n   a22        4500");
    std::cout << "Initial record length is " << record.size() << ".\n";
    std::string tag("001");
    while (record.size() <= 99999) {
        std::cout << "Inserted new field w/ index " << record.insertField(tag, std::string(5555, 'x')) << ".\n";
        std::cout << "Record length is now " << record.size() << ".\n";
        std::string flaw_description;
        if (not record.isValid(&flaw_description))
            logger->error("after adding tag \"" + tag + "\": " + flaw_description);
        tag = IncrementTag(tag);
    }
    auto marc_writer(MARC::Writer::Factory(argv[1]));
    marc_writer->write(record);
    std::cout << "The record has been written!\n";
    return EXIT_SUCCESS;
}
