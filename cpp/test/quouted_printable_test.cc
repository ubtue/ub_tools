/** \file   quouted_printable_test.cc
 *  \brief  A test harness for TextUtil::EncodeQuotedPrintable and TextUtil::DecodeQuotedPrintable.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include <cstring>
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " (--encode|--decode) text\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();
    
    try {
        if (std::strcmp("--encode", argv[1]) == 0)
            std::cout << TextUtil::EncodeQuotedPrintable(argv[2]) << '\n';
        else if (std::strcmp("--decode", argv[1]) == 0)
            std::cout << TextUtil::DecodeQuotedPrintable(argv[2]) << '\n';
        else
            Usage();
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
