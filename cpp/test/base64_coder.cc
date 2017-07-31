/** \file    base64_coder.cc
    \brief   Test harness for TextUtil::{Base64Encode,Base64Decode}
    \author  Dr. Johannes Ruscheinski

    \copyright 2017, Library of the University of Tübingen

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
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << "(--encode|--decode) string\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();
    if (std::strcmp(argv[1], "--encode") == 0)
        std::cout << TextUtil::Base64Encode(argv[2]) << '\n';
    else     if (std::strcmp(argv[1], "--decode") == 0)
        std::cout << TextUtil::Base64Decode(argv[2]) << '\n';
    else
        Usage();
}

