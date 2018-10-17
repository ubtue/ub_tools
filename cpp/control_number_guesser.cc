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
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << "(--lookup-author=author|--lookup-title=title|--lookup-year=year)]\n";
    std::exit(EXIT_FAILURE);
}


void LookupAuthor(const ControlNumberGuesser &control_number_guesser, const std::string &author) {
    std::set<std::string> control_numbers;
    control_number_guesser.lookupAuthor(author, &control_numbers);

    for (const auto &control_number : control_numbers)
        std::cout << control_number << '\n';
}


void LookupTitle(const ControlNumberGuesser &control_number_guesser, const std::string &title) {
    std::set<std::string> control_numbers;
    control_number_guesser.lookupTitle(title, &control_numbers);

    for (const auto &control_number : control_numbers)
        std::cout << control_number << '\n';
}


void LookupYear(const ControlNumberGuesser &control_number_guesser, const std::string &year) {
    std::unordered_set<std::string> control_numbers;
    control_number_guesser.lookupYear(year, &control_numbers);

    for (const auto &control_number : control_numbers)
        std::cout << control_number << '\n';
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    const ControlNumberGuesser control_number_guesser(ControlNumberGuesser::DO_NOT_CLEAR_DATABASES);
    if (StringUtil::StartsWith(argv[1], "--lookup-author="))
        LookupAuthor(control_number_guesser, argv[1] + __builtin_strlen("--lookup-author="));
    else if (StringUtil::StartsWith(argv[1], "--lookup-title="))
        LookupTitle(control_number_guesser, argv[1] + __builtin_strlen("--lookup-title="));
    else if (StringUtil::StartsWith(argv[1], "--lookup-year="))
        LookupYear(control_number_guesser, argv[1] + __builtin_strlen("--lookup-year="));
    else
        Usage();

    return EXIT_SUCCESS;
}
