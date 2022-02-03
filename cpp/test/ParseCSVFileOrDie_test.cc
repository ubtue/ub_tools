/** \brief Test program for interfacing to the Sqlite3 tables.
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
#include <string>
#include <cstdlib>
#include "TextUtil.h"
#include "util.h"


[[noreturn]] static void Usage() {
    std::cerr << "Usage: " << ::progname << "CSV_filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        std::vector<std::vector<std::string>> lines;
        TextUtil::ParseCSVFileOrDie(argv[1], &lines);
        std::cerr << "Read " << lines.size() << " logical lines.\n";
        unsigned row(0);
        for (const auto &line : lines) {
            ++row;
            unsigned column(0);
            for (const auto &value : line) {
                ++column;
                if (column > 1)
                    std::cout << ", ";
                std::cout << row << ':' << column << ":'" << value << '\'';
            }
            std::cout << '\n';
        }
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
