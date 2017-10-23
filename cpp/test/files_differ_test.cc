/** \file   files_differ_test.cc
 *  \brief  Test harness for FileUtil::FilesDiffer
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " path1 path2\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();
    
    try {
        return FileUtil::FilesDiffer(argv[1], argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
