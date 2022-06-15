/** \brief Utility for downloading data from CORE.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2022 Tübingen University Library.  All rights reserved.
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
#include "CORE.h"
#include "FileUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "query output_dir\n"
        "\tquery: The Query to use for CORE (like in the search field.)\n"
        "\toutput_dir: The directory to store the JSON result files (will be split due to API query limit restrictions).\n\n");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    // Parse args
    const std::string query(argv[1]);
    const std::string output_dir(argv[2]);

    // Setup CORE instance & parameters
    CORE core;
    CORE::SearchParams params;
    params.q_ = query;
    params.exclude_ = { "fullText" }; // for performance reasons
    params.limit_ = 100; // default 10, max 100
    params.entity_type_ = CORE::EntityType::WORK;

    // Perform download
    core.searchBatch(params, output_dir);

    return EXIT_SUCCESS;
}
