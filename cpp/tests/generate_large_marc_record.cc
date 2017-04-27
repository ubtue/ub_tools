/** \brief Test program dealing with a record that exceeds 99999 bytes.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Leader.h"
#include "MarcWriter.h"
#include "MarcRecord.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
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
    
    Error("overflow in IncrementTag()!");
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string format(argv[1]);
    if (format != "xml" and format != "binary")
        Error("bad format, format must be \"xml\" or \"binary\"!");

    try {
        MarcRecord record;
        const Leader &leader(record.getLeader());
        std::cout << "Initial record length is " << leader.getRecordLength() << ".\n";
        std::string tag("001");
        while (leader.getRecordLength() <= 99999) {
            std::cout << "Inserted new field w/ index " << record.insertField(tag, std::string(5555, 'x')) << ".\n";
            std::cout << "Record length is now " << leader.getRecordLength() << ".\n";
            std::string flaw_description;
            if (not record.isProbablyCorrect(&flaw_description))
                Error("after adding tag \"" + tag + "\": " + flaw_description);
            tag = IncrementTag(tag);
        }
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2], format == "xml" ? MarcWriter::XML
                                                                                             : MarcWriter::BINARY));
        marc_writer->write(record);
        std::cout << "The record has been written!\n";
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
