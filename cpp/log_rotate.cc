/** \file   log_rotate.cc
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
#include "FileUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const unsigned DEFAULT_MAX_ROTATIONS(5);


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--max-rotations=max_rotations] regex\n"
              << "       where the default for \"max_rotations\" is " << DEFAULT_MAX_ROTATIONS
              << " and \"regex\" must be a PCRE.\n\n";
    std::exit(EXIT_FAILURE);
}


inline bool HasNumericExtension(const std::string &filename) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\\.[0-9]+$"));
    return matcher->matched(filename);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();
    
    unsigned max_rotations(DEFAULT_MAX_ROTATIONS);
    std::string file_regex;
    if (argc == 3) {
        if (not StringUtil::StartsWith(argv[1], "--max-rotations="))
            Usage();
        if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--max-rotations="), &max_rotations))
            Error("\"" + std::string(argv[1] + std::strlen("--max-rotations="))
                  + "\" is not a valid maximum rotation count!");
        file_regex = argv[2];
    } else
        file_regex = argv[1];
    
    try {
        FileUtil::Directory directory(file_regex);
        for (const auto &entry : directory) {
            if (not HasNumericExtension(entry.getName()))
                MiscUtil::LogRotate(entry.getName(), max_rotations);
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
