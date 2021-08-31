/** \brief Tool to determine the frequency for krim keywords
 *  \author Johannes Riedl
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"

namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " A-Z_output keyword_frequeny.csv\n";
    std::exit(EXIT_FAILURE);
}


void ParseFrequencyTable(const std::string &path, std::unordered_map<std::string, std::string> * const all_frequency_map) {
    std::vector<std::vector<std::string>> keywords_and_frequencies;
    TextUtil::ParseCSVFileOrDie(path, &keywords_and_frequencies);
    for (const auto &keyword_and_frequency : keywords_and_frequencies) {
         if (keyword_and_frequency.size() != 2)
            LOG_ERROR("Invalid keyword and frequency for entry " +
                       StringUtil::Join(keyword_and_frequency, ","));
         all_frequency_map->emplace(keyword_and_frequency[0], keyword_and_frequency[1]);
    }
}

} //unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    std::unordered_map<std::string, std::string> all_frequencies_map;
    ParseFrequencyTable(argv[2], &all_frequencies_map);
    std::vector<std::pair<std::string, unsigned>> local_keyword_frequencies;
    for (const auto &line : FileUtil::ReadLines(argv[1])) {
        std::vector<std::string> tokens;
        StringUtil::Split(line, ';', &tokens, true /* suppress empty */);
        unsigned frequency_with_variants(0);
        for (const auto &token : tokens) {
            if (not std::all_of(token.begin(),token.end(),isspace)) {
                if (all_frequencies_map.find(token) != all_frequencies_map.end())
                    frequency_with_variants += std::stoi(all_frequencies_map[token]);
            }
        }
        local_keyword_frequencies.emplace_back(std::make_pair(tokens[0], frequency_with_variants));

    }
    for (const auto &[token, frequency] : local_keyword_frequencies)
        std::cout << token << ";" << frequency << '\n';
    return EXIT_SUCCESS;
}
