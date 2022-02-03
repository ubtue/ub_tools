/** \brief Utility for looking up entries in one of our IniFiles.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <map>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "IniFile.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--suppress-newline|-n] path section entry [optional_default_value]");
}


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    const bool suppress_newline(std::strcmp(argv[1], "--suppress-newline") == 0 or std::strcmp(argv[1], "-n") == 0);
    if (suppress_newline)
        --argc, ++argv;

    if (argc != 4 and argc != 5)
        Usage();

    try {
        IniFile ini_file(argv[1]);

        const std::string section(argv[2]);
        const std::string entry(argv[3]);

        std::string value;
        if (ini_file.lookup(section, entry, &value))
            std::cout << value;
        else if (argc == 5)
            std::cout << argv[4];
        else
            LOG_ERROR("entry \"" + entry + "\" in section \"" + section + "\" not found!");
        if (not suppress_newline)
            std::cout << '\n';
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }

    return EXIT_SUCCESS;
}
