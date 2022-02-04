/** \brief Utility for creating ngram language models.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "NGram.h"
#include "StringUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--topmost-use-count=N] language_blob language_model\n"
            "The default for N is " + std::to_string(NGram::DEFAULT_TOPMOST_USE_COUNT) + ".\n"
            "The \"language_blob\" should be a file containing example text w/o markup in whatever language.\n"
            "\"language_model\" should be named after the language followed by \".lm\".\n");
}


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    unsigned topmost_use_count(NGram::DEFAULT_TOPMOST_USE_COUNT);
    if (StringUtil::StartsWith(argv[1], "--topmost-use-count=")) {
        topmost_use_count = StringUtil::ToUnsigned(argv[1] + __builtin_strlen("--topmost-use-count="));
        --argc, ++argv;
    }

    if (argc != 3)
        Usage();

    std::ifstream input(argv[1]);
    if (not input)
        LOG_ERROR("failed to open \"" + std::string(argv[1]) + "\" for reading!");

    NGram::CreateAndWriteLanguageModel(input, argv[2], NGram::DEFAULT_NGRAM_NUMBER_THRESHOLD, topmost_use_count);

    return EXIT_SUCCESS;
}
