/** \file marc_split.cc
 *  \brief Splits a MARC 21 file in equally sized files.
 *
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
    std::cerr << "usage: " << ::progname << " marc_input marc_output_name splits\n";
    std::exit(EXIT_FAILURE);
}

void Split(File * const input, std::vector<File*> & outputs) {
    unsigned index = 0;
    while (MarcRecord record = MarcReader::Read(input)) {
        MarcWriter::Write(record, outputs[index % outputs.size()]);
        ++index ;
    }
    std::cout << "~" << (index / outputs.size()) << " records per file.\n";
}

int main(int argc, char* argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string input_filename(argv[1]);
    File input(input_filename, "r");
    if (not input)
        Error("can't open \"" + input_filename + "\" for reading!");

    unsigned splits;
    if (not StringUtil::ToUnsigned(argv[3], &splits))
        Error("bad split: \"" + std::string(argv[3]) + "\"!");

    const std::string output_prefix(argv[2]);

    std::vector<File*> outputs;
    for (size_t i(0); i < splits; ++i) {
        const std::string output_filename(output_prefix + "_" + std::to_string(i) + ".mrc");
        outputs.emplace_back(new File(output_filename, "w"));
    }
    Split(&input, outputs);
}