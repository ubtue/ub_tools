/**
 * \brief The Utility for extracting issn information from https://portal.issn.org/
 * \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 * \copyright 2023 Tübingen University Library.  All rights reserved.
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
#include "IssnLookup.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage(
        "issn [--verbose]\n"
        "- issn: International Standard Serial Number\n"
        "- --verbose: print the issn info to standard output.\n");

    std::exit(EXIT_FAILURE);
}

int Main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3)
        Usage();

    nlohmann::json issn_info_json;
    bool processing_state(IssnLookup::GetISSNInfo(argv[1], issn_info_json));


    if (processing_state) {
        IssnLookup::ISSNInfo issn_info;
        IssnLookup::ExtractingData(&issn_info, issn_info_json, argv[1]);
        if (argc == 3 && (std::strcmp(argv[2], "--verbose") == 0))
            IssnLookup::PrettyPrintISSNInfo(issn_info);
    }

    return (processing_state ? EXIT_SUCCESS : EXIT_FAILURE);
}