/** \brief A test harness for the bible reference parser.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include <cstring>
#include "BibleUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " [--verbose] bible_reference_candidate [expected_pair1 expected_pair2 "
              << "... expected_pairN]\n";
    std::cerr << "       When the expected pairs, where start and end have to be separated with a colon, are\n";
    std::cerr << "       provided, the program returns a non-zero exit code if not all pairs have been matched!\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc < 2)
        Usage();

    const std::string book_code("01");
    std::set<std::pair<std::string, std::string>> start_end;
    if (not BibleUtil::ParseBibleReference(argv[1], book_code, &start_end)) {
        if (argc == 2)
            std::cerr << "Bad bible reference: " << argv[1] << '\n';
        std::exit(EXIT_FAILURE);
    }

    std::set<std::string> parsed_pairs;
    for (const auto &pair : start_end) {
        const std::string parsed_pair(pair.first + ":" + pair.second);
        if (argc == 2)
            std::cout << parsed_pair << '\n';
        else
            parsed_pairs.insert(parsed_pair);
    }

    if (argc == 2)
        return EXIT_SUCCESS;

    unsigned matched_count(0);
    for (int arg_no(2); arg_no < argc; ++arg_no) {
        if (parsed_pairs.find(argv[arg_no]) == parsed_pairs.end()) {
            if (verbose)
                std::cerr << argv[arg_no] << " did not match any of " << StringUtil::Join(parsed_pairs, ",") << "!\n";
            return EXIT_FAILURE;
        }
        ++matched_count;
    }

    return (matched_count == parsed_pairs.size()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
