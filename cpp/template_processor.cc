/** \file    template_processor.cc
 *  \brief   Expands a template and prints the result to stdout.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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

#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>
#include "Template.h"
#include "util.h"


void ProcessNameValuePair(const std::string &name_value_pair, Template::Map * const map) {
    const auto first_colon_pos(name_value_pair.find('='));
    if (first_colon_pos == std::string::npos or first_colon_pos == 0)
        LOG_ERROR("bad name/value pair: \"" + name_value_pair + "\"!");
    const auto variable_name(name_value_pair.substr(0, first_colon_pos));

    std::vector<std::string> array_values;
    std::string current_value;
    bool escaped(false);
    for (auto cp(name_value_pair.cbegin() + variable_name.length() + 1); cp != name_value_pair.cend(); ++cp) {
        if (escaped) {
            current_value += *cp;
            escaped = false;
        } else if (*cp == '\\')
            escaped = true;
        else if (*cp == ';') {
            array_values.push_back(current_value);
            current_value.clear();
        } else
            current_value += *cp;
    }
    if (escaped)
        LOG_ERROR("name/value pair w/ trailing escape: \"" + name_value_pair + "\"!");
    array_values.push_back(current_value);

    if (array_values.empty())
        LOG_ERROR("empty arrays are not supported! (\"" + name_value_pair + "\"!");
    else if (array_values.size() == 1)
        map->insertScalar(variable_name, array_values.front());
    else
        map->insertArray(variable_name, array_values);
}


int Main(int argc, char *argv[]) {
    if (argc < 2)
        ::Usage(
            "template_file [var1=value1 var2=value2 .. varN=valueN]\n"
            "For arrays use semicolons to separate individual values.  If a value has an embedded semicolon\n"
            "use a backslash to escape it.  Also use a backslash to escape an embedded backslash.\n"
            "NB: Empty values are explicitly permitted!");

    const std::string input_filename(argv[1]);
    std::ifstream input(input_filename);
    if (not input)
        LOG_ERROR("failed to open \"" + input_filename + "\" for reading!");

    Template::Map map;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        ProcessNameValuePair(argv[arg_no], &map);

    Template::ExpandTemplate(input, std::cout, map);
    return EXIT_SUCCESS;
}
