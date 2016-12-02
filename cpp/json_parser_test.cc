/** \brief Test harness for our JSON parser.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <sstream>
#include <cstdlib>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " json_input_filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    try {
        const std::string input_filename(argv[1]);
        std::string document;
        if (not FileUtil::ReadString(input_filename, &document))
            Error("failed to read \"" + input_filename + "\"!");

        std::stringstream input(document, std::ios_base::in);
        boost::property_tree::ptree property_tree;
        boost::property_tree::json_parser::read_json(input, property_tree);

        for (const auto &array_entry : property_tree.get_child("response.docs.")) {
            const auto &id(array_entry.second.get<std::string>("id"));
            const auto &title(array_entry.second.get<std::string>("title"));
            std::cout << id << " : " << title << '\n';
        }
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
