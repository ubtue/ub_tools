/** \brief Utility for guessing the language of some text.
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
#include "FileUtil.h"
#include "NGram.h"
#include "StringUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 2 and argc != 3)
        ::Usage("text | --file=filename [comma_separated_language_codes_list]");

    std::string text;
    if (StringUtil::StartsWith(argv[1], "--file="))
        FileUtil::ReadStringOrDie(argv[1] + __builtin_strlen("--file="), &text);
    else
        text = argv[1];

    std::set<std::string> language_codes_list;
    if (argc == 3)
        StringUtil::Split(std::string(argv[2]), ',', &language_codes_list, /* suppress_empty_components = */ true);

    std::vector<NGram::DetectedLanguage> top_languages;
    NGram::ClassifyLanguage(text, &top_languages, language_codes_list, NGram::DEFAULT_NGRAM_NUMBER_THRESHOLD);

    for (const auto &language : top_languages)
        std::cout << language.language_ << " (" << language.score_ << ")" << '\n';

    return EXIT_SUCCESS;
}
