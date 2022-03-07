/** \file    generate_simple_math_puzzle.cc
 *  \brief   Geneates a puzzle only using addition, subtraction, multiplication and small integers.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019 Library of the University of Tübingen

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

#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <StringUtil.h>


const int MAX_NUMBER(6);


// \return a number from the range [0, MAX_NUMBER].
int GetNumber() {
    return 1 + (std::rand() % MAX_NUMBER);
}


typedef std::function<int(const int, const int, const int)> PuzzleFunc;


const std::vector<std::pair<std::string, PuzzleFunc>> TEMPLATES{
    { "%n + %n × %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 + n2 * n3; } },
    { "%n - %n × %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 - n2 * n3; } },
    { "%n × %n + %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 * n2 + n3; } },
    { "%n × %n - %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 * n2 - n3; } },
    { "%n + %n + %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 + n2 + n3; } },
    { "%n - %n + %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 - n2 + n3; } },
    { "%n + %n - %n = ?", [](const int n1, const int n2, const int n3) -> int { return n1 + n2 - n3; } },
};


std::string GeneratePuzzle() {
    const unsigned puzzle_type(std::rand() % TEMPLATES.size());

    std::vector<int> values;
    std::string puzzle(TEMPLATES[puzzle_type].first + "\n");

    const int n1(GetNumber());
    StringUtil::ReplaceString("%n", StringUtil::ToString(n1), &puzzle, /* global = */ false);

    const int n2(GetNumber());
    StringUtil::ReplaceString("%n", StringUtil::ToString(n2), &puzzle, /* global = */ false);

    const int n3(GetNumber());
    StringUtil::ReplaceString("%n", StringUtil::ToString(n3), &puzzle, /* global = */ false);

    puzzle += StringUtil::ToString(TEMPLATES[puzzle_type].second(n1, n2, n3));

    return puzzle;
}


int Main(int /*argc*/, char * /*argv*/[]) {
    std::srand(time(NULL));
    std::cout << GeneratePuzzle() << "\n";
    return EXIT_SUCCESS;
}
