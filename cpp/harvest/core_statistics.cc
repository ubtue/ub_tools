/** \brief Utility for generating statistics from downloaded files.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2022 Tübingen University Library.  All rights reserved.
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
#include <map>
#include "CORE.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "core_file\n"
        "\tcore_file: The Downloaded and merged file.\n\n");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    // Parse args and load file
    const std::string core_file(argv[1]);
    const auto works(CORE::GetWorksFromFile(core_file));

    unsigned count(0);
    unsigned articles(0);
    std::map<std::string, unsigned> languages;
    for (const auto &work : works) {
        ++count;
        if (work.isArticle())
            ++articles;
        const auto language_iter(languages.find(work.language_.code_));
        if (language_iter == languages.end())
            languages[work.language_.code_] = 1;
        else
            ++languages[work.language_.code_];

    }

    LOG_INFO("Statistics for " + core_file + ":");
    LOG_INFO(std::to_string(count) + " datasets (" + std::to_string(count) + " articles)");

    std::string languages_msg("languages: ");
    bool first(true);
    for (const auto &[language_code, language_count] : languages) {
        if (not first)
            languages_msg += ", ";
        languages_msg += "\"" + language_code + "\": " + std::to_string(language_count);
        first = false;
    }
    LOG_INFO(languages_msg);

    return EXIT_SUCCESS;
}
