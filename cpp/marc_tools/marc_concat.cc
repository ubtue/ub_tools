/** \brief Utility for concatenating multiple MARC files into one.
 *  \author Hjordis Lindeboom
 *
 *  \copyright 2025 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "MARC.h"
#include "util.h"


namespace {


void Usage() {
    ::Usage("marc_input1 marc_input2 ... --output-file marc_output");
}


void ProcessRecords(MARC::Reader* const marc_reader, MARC::Writer* const marc_writer) {
    while (MARC::Record record = marc_reader->read())
        marc_writer->write(record);
}


} // unnamed namespace


int Main(int argc, char* argv[]) {
    std::vector<std::string> inputFiles;
    std::string outputFile;

    if (argc < 4)
        Usage();

    --argc, ++argv;

    while (argc > 0) {
        std::string arg = *argv;

        if (arg == "--output-file") {
            --argc, ++argv;

            if (argc == 0)
                Usage();

            outputFile = *argv;
            break;
        }

        inputFiles.push_back(arg);
        --argc, ++argv;
    }

    if (outputFile.empty())
        Usage();

    std::unique_ptr<MARC::Writer> marc_writer = MARC::Writer::Factory(outputFile);
    if (not marc_writer) {
        LOG_ERROR("Error: Could not create MARC writer for output file: " + outputFile);
    }

    for (const auto& file : inputFiles) {
        std::unique_ptr<MARC::Reader> marc_reader = MARC::Reader::Factory(file);

        if (not marc_reader) {
            LOG_ERROR("Error: Could not open input file: " + file);
        }
        ProcessRecords(marc_reader.get(), marc_writer.get());
    }

    return EXIT_SUCCESS;
}
