/** \file marc_split.cc
 *  \brief Splits a MARC 21 file in equally sized files.
 *
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
#include <vector>
#include "File.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input marc_output_name split_count\n";
    std::exit(EXIT_FAILURE);
}


void Split(MarcReader * const marc_reader, std::vector<std::unique_ptr<MarcWriter>> &marc_writers) {
    unsigned index(0);
    while (const MarcRecord record = marc_reader->read()) {
        marc_writers[index % marc_writers.size()]->write(record);
        ++index ;
    }
    std::cout << "~" << (index / marc_writers.size()) << " records per file.\n";
}


int main(int argc, char* argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    unsigned split_count;
    if (not StringUtil::ToUnsigned(argv[3], &split_count))
        logger->error("bad split count: \"" + std::string(argv[3]) + "\"!");

    const std::string output_prefix(argv[2]);

    std::vector<std::unique_ptr<MarcWriter>> marc_writers;
    for (size_t i(0); i < split_count; ++i) {
        const std::string output_filename(output_prefix + "_" + std::to_string(i) + ".mrc");
        marc_writers.emplace_back(MarcWriter::Factory(output_filename, MarcWriter::BINARY));
    }
    Split(marc_reader.get(), marc_writers);
}
