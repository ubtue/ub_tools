/** \brief Tool for detecting possible dups based on the same title and, at least one common author.
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
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] possible_matches_list\n";
    std::exit(EXIT_FAILURE);
}


const std::string IXTHEO_PREFIX("https://ixtheo.de/Record/");

    
void FindDups(File * const matches_list_output,
              const std::unordered_map<std::string, std::set<std::string>> &title_to_control_numbers_map,
              const  std::unordered_map<std::string, std::set<std::string>> &control_number_to_authors_map)
{
    unsigned dup_count(0);
    for (const auto &title_and_control_numbers : title_to_control_numbers_map) {
        if (title_and_control_numbers.second.size() < 2)
            continue;

        std::map<std::string, std::set<std::string>> author_to_control_numbers_map;
        for (const auto &control_number : title_and_control_numbers.second) {
            const auto control_number_and_authors(control_number_to_authors_map.find(control_number));
            if (control_number_and_authors == control_number_to_authors_map.cend())
                continue;

            for (const auto &author : control_number_and_authors->second) {
                auto author_and_control_numbers(author_to_control_numbers_map.find(author));
                if (author_and_control_numbers == author_to_control_numbers_map.end())
                    author_to_control_numbers_map[author] = std::set<std::string>{ control_number };
                else
                    author_and_control_numbers->second.emplace(control_number);
            }
        }

        // Output those cases where we found multiple control numbers for the same author for a single title:
        for (const auto &author_and_control_numbers : author_to_control_numbers_map) {
            if (author_and_control_numbers.second.size() >= 2) {
                for (const auto &control_number : author_and_control_numbers.second)
                    (*matches_list_output) << IXTHEO_PREFIX << control_number << ' ';
                (*matches_list_output) << "\r\n";
            }
        }
    }

    LOG_INFO("found " + std::to_string(dup_count) + " possible multiples.");
}

    
} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    ControlNumberGuesser control_number_guesser(ControlNumberGuesser::DO_NOT_CLEAR_DATABASES);
    
    std::unordered_map<std::string, std::set<std::string>> title_to_control_numbers_map;
    std::string title;
    std::set<std::string> control_numbers;
    while (control_number_guesser.getNextTitle(&title, &control_numbers))
        title_to_control_numbers_map.emplace(title, control_numbers);
    LOG_INFO("loaded " + std::to_string(title_to_control_numbers_map.size()) + " mappings from titles to control numbers.");

    std::unordered_map<std::string, std::set<std::string>> control_number_to_authors_map;
    std::string author;
    while (control_number_guesser.getNextAuthor(&author, &control_numbers)) {
        for (const auto &control_number : control_numbers) {
            auto control_number_and_authors(control_number_to_authors_map.find(control_number));
            if (control_number_and_authors == control_number_to_authors_map.end())
                control_number_to_authors_map[control_number] = std::set<std::string>{ author };
            else
                control_number_and_authors->second.emplace(author);
        }
    }
    LOG_INFO("loaded " + std::to_string(control_number_to_authors_map.size()) + " mappings from control numbers to authors.");

    auto matches_list_output(FileUtil::OpenOutputFileOrDie(argv[1]));
    FindDups(matches_list_output.get(), title_to_control_numbers_map, control_number_to_authors_map);

    return EXIT_SUCCESS;
}
