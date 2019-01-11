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
#include "BinaryIO.h"
#include "FileUtil.h"
#include "NGram.h"
#include "TextUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--debug] language_blob language_model\n"
            "The \"language_blob\" should be a file containing example text w/o markup in whatever language.\n"
            "\"language_model\" should be named after the language followed by \".lm\".\n");
}


int Main(int argc, char *argv[]) {
    if (argc != 3 and argc != 4)
        Usage();

    bool debug(false);
    if (argc == 4) {
        if (std::strcmp(argv[1], "--debug") != 0)
            Usage();
        --argc, ++argv;
        debug = true;
    }

    std::ifstream input(argv[1]);
    if (not input)
        LOG_ERROR("failed to open \"" + std::string(argv[1]) + "\" for reading!");

    NGram::NGramCounts ngram_counts;
    NGram::SortedNGramCounts sorted_ngram_counts;
    NGram::CreateLanguageModel(input, &ngram_counts, &sorted_ngram_counts);

    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    BinaryIO::WriteOrDie(*output, sorted_ngram_counts.size());
    for (const auto &ngram_and_rank : sorted_ngram_counts) {
        if (debug)
            std::cout << '"' << TextUtil::WCharToUTF8StringOrDie(ngram_and_rank.first) << "\" = " << ngram_and_rank.second << '\n';
        BinaryIO::WriteOrDie(*output, ngram_and_rank.first);
        BinaryIO::WriteOrDie(*output, ngram_and_rank.second);
    }

    return EXIT_SUCCESS;
}
