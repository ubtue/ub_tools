/** \file dsv_reader.cc
 *  \brief Test program for the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " dsv_input\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        DSVReader reader(argv[1]);
        std::vector<std::string> values;
        while (reader.readLine(&values)) {
            unsigned counter(0);
            for (const auto &value : values)
                std::cout << ++counter << ": " << value << ' ';
            std::cout << '\n';
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
