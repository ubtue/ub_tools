/** \brief Importer for full text documents.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "FullTextImport.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] fulltext_file1 [fulltext_file2 .. fulltext_fileN]\n";
    std::exit(EXIT_FAILURE);
}


bool ImportDocument(ControlNumberGuesser &control_number_guesser, const std::string &filename) {
    const auto input(FileUtil::OpenInputFileOrDie(filename));
    FullTextImport::FullTextData full_text_data;
    FullTextImport::ReadExtractedTextFromDisk(input.get(), &full_text_data);

    std::string ppn;
    if (not FullTextImport::CorrelateFullTextData(control_number_guesser, full_text_data, &ppn))
        return false;

    return true;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc < 2)
        Usage();

    ControlNumberGuesser control_number_guesser;

    unsigned total_count(0), failure_count(0);
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        ++total_count;
        if (not ImportDocument(control_number_guesser, argv[arg_no]))
            ++failure_count;
    }

    LOG_INFO("Failed to import " + std::to_string(failure_count) + " documents of " + std::to_string(total_count) + ".");

    return EXIT_SUCCESS;
}
