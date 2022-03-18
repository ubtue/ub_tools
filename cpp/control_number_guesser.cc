/** \file    control_number_guesser.cc
 *  \brief   A tool for filtering MARC-21 data sets based on patterns for control numbers.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include "ControlNumberGuesser.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << "--lookup-author=author|--lookup-title=title|--lookup-year=year|--lookup-doi=doi\n"
              << "       You can repeat the lookup operations any number of times, in which case you will get the\n"
              << "       intersection of the lookups.\n\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    const ControlNumberGuesser control_number_guesser;

    std::set<std::string> control_numbers;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        if (StringUtil::StartsWith(argv[arg_no], "--lookup-author=")) {
            std::set<std::string> control_numbers2;
            control_number_guesser.lookupAuthor(argv[arg_no] + __builtin_strlen("--lookup-author="), &control_numbers2);

            if (arg_no == 1)
                control_numbers.insert(control_numbers2.cbegin(), control_numbers2.cend());
            else {
                control_numbers2 = MiscUtil::Intersect(control_numbers2, control_numbers);
                control_numbers.clear();
                control_numbers.insert(control_numbers2.cbegin(), control_numbers2.cend());
            }
        } else if (StringUtil::StartsWith(argv[arg_no], "--lookup-title=")) {
            std::set<std::string> control_numbers2;
            control_number_guesser.lookupTitle(argv[arg_no] + __builtin_strlen("--lookup-title="), &control_numbers2);

            if (arg_no == 1)
                control_numbers.insert(control_numbers2.cbegin(), control_numbers2.cend());
            else {
                control_numbers2 = MiscUtil::Intersect(control_numbers2, control_numbers);
                control_numbers.clear();
                control_numbers.insert(control_numbers2.cbegin(), control_numbers2.cend());
            }
        } else if (StringUtil::StartsWith(argv[arg_no], "--lookup-year=")) {
            std::set<std::string> control_numbers2;
            control_number_guesser.lookupYear(argv[arg_no] + __builtin_strlen("--lookup-year="), &control_numbers2);

            if (arg_no == 1)
                control_numbers.insert(control_numbers2.cbegin(), control_numbers2.cend());
            else {
                const auto control_numbers3(MiscUtil::Intersect(control_numbers, control_numbers2));
                control_numbers.clear();
                control_numbers.insert(control_numbers3.cbegin(), control_numbers3.cend());
            }
        } else if (StringUtil::StartsWith(argv[arg_no], "--lookup-doi=")) {
            if (arg_no == 1)
                control_number_guesser.lookupDOI(argv[arg_no] + __builtin_strlen("--lookup-doi="), &control_numbers);
            else {
                std::set<std::string> control_numbers2;
                control_number_guesser.lookupDOI(argv[arg_no] + __builtin_strlen("--lookup-doi="), &control_numbers2);
                const auto control_numbers3(MiscUtil::Intersect(control_numbers2, control_numbers));
                control_numbers.clear();
                control_numbers.insert(control_numbers3.cbegin(), control_numbers3.cend());
            }
        } else
            Usage();
    }

    for (const auto &control_number : control_numbers)
        std::cout << control_number << '\n';

    return EXIT_SUCCESS;
}
