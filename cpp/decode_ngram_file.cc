/** \brief Utility for converting binary ngram language model files to a human-readable format.
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

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "NGram.h"
#include "TextUtil.h"
#include "util.h"


namespace {


void DecodeFile(const std::string &language) {
    NGram::NGramCounts ngram_counts;
    NGram::LoadLanguageModel(language, &ngram_counts);

    const NGram::SortedNGramCounts sorted_ngram_counts(ngram_counts, NGram::SortedNGramCounts::DESCENDING_ORDER);
    for (const auto &ngram_and_frequency : sorted_ngram_counts)
        std::cout << TextUtil::WCharToUTF8StringOrDie(ngram_and_frequency.first) << ": " << ngram_and_frequency.second << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage(" language");

    DecodeFile(argv[1]);
    return EXIT_SUCCESS;
}
