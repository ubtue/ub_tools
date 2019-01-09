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
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "NGram.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("language_blob language_model\n"
                "The \"language_blob\" should be a file containing example text w/o markup in whatever language.\n"
                "\"language_model\" should be named after the language followed by \".lm\".\n");

    std::ifstream input(argv[1]);
    if (not input)
        LOG_ERROR("failed to open \"" + std::string(argv[1]) + "\" for reading!");

    NGram::NGramCounts ngram_counts;
    NGram::SortedNGramCounts sorted_ngram_counts;
    NGram::CreateLanguageModel(input, &ngram_counts, &sorted_ngram_counts);

    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    for (const auto &ngram_and_frequency : sorted_ngram_counts)
        *output << ngram_and_frequency.first << '\t' << ngram_and_frequency.second << '\n';

    return EXIT_SUCCESS;
}
