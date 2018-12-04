/** \brief Tool for submitting full-text files to an ElsticSearch server.
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
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "FullTextImport.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] [--normalise-only] input1 [input2 .. inputN]\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc == 0)
        Usage();

    ControlNumberGuesser control_number_guesser;

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        const std::string input_filename(argv[arg_no]);
        const auto input_file(FileUtil::OpenInputFileOrDie(input_filename));

        FullTextImport::FullTextData full_text_data;
        FullTextImport::ReadExtractedTextFromDisk(input_file.get(), &full_text_data);

        const auto guessed_control_numbers(control_number_guesser.getGuessedControlNumbers(full_text_data.title_, full_text_data.authors_,
                                                                                           full_text_data.year_, full_text_data.doi_,
                                                                                           full_text_data.issn_, full_text_data.isbn_));
        if (guessed_control_numbers.empty())
            LOG_WARNING("failed to associate \"" + input_filename + "\" with any control number!");
        else if (guessed_control_numbers.size() > 1)
            LOG_WARNING("we're in a pickle!");
        else
            LOG_INFO("whoohoo!");
    }

    return EXIT_SUCCESS;
}
