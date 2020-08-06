#include <fstream>
#include <iostream>
#include <cstdlib>
#include "Template.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc < 2)
        ::Usage("template_file [var1=value1 var2=value2 .. varN=valueN]");

    const std::string input_filename(argv[1]);
    std::ifstream input(input_filename);
    if (not input)
        LOG_ERROR("failed to open \"" + input_filename + "\" for reading!");

    Template::Map map;
    for (int arg_no(2); arg_no < argc; ++arg_no) {
        const std::string variable_name_and_value(argv[arg_no]);
        const auto first_colon_pos(variable_name_and_value.find('='));
        if (first_colon_pos == std::string::npos or first_colon_pos == 0)
            LOG_ERROR("bad name/value pair: \"" + variable_name_and_value + "\"!");
        map.insertScalar(variable_name_and_value.substr(0, first_colon_pos), variable_name_and_value.substr(first_colon_pos + 1));
    }

    Template::ExpandTemplate(input, std::cout, map);
    return EXIT_SUCCESS;
}
