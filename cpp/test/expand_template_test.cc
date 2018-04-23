/** Test harness for MiscUtil::ExpandTemplate().
 */
#include <iostream>
#include <map>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include "FileUtil.h"
#include "StringUtil.h"
#include "Template.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname
              << " template_filename var1_and_values [var2_and_values ... varN_and_values]\n"
              << "       Variable names and values have to be separated by colons, arrays of array have to have their arrays\n"
              << "       arrays separated by semicolons.\n\n";
    std::exit(EXIT_FAILURE);
}


void ExtractNamesAndValues(const int argc, char *argv[], Template::Map * const names_to_values_map) {
    names_to_values_map->clear();

    for (int arg_no(2); arg_no < argc; ++arg_no) {
        std::string arg(argv[arg_no]);
        const auto first_colon_pos(arg.find(':'));
        if (first_colon_pos == std::string::npos)
            LOG_ERROR("missing variable name: \"" + arg + "\"!");
        const std::string name(arg.substr(0, first_colon_pos));
        arg = arg.substr(first_colon_pos + 1);
        std::vector<std::string> values;
        const auto first_semicolon_pos(arg.find(';'));
        if (first_semicolon_pos != std::string::npos) { // We have an array of arrays.
            std::vector<std::string> arrays;
            StringUtil::Split(arg, ';', &arrays);
            std::vector<std::shared_ptr<Template::Value>> array_of_arrays;
            for (unsigned i(0); i < arrays.size(); ++i) {
                StringUtil::Split(arrays[i], ':', &values);
                if (values.empty())
                    LOG_ERROR("subarray " + std::to_string(i) + " of \"" + name + "\" is missing at least one value!");
                std::shared_ptr<Template::ArrayValue> subarray(new Template::ArrayValue(name + "[" + std::to_string(i) + "]"));
                for (const auto &value : values)
                    subarray->appendValue(value);
                array_of_arrays.emplace_back(subarray);
            }
            names_to_values_map->insertArray(name, array_of_arrays);
        } else {
            StringUtil::Split(arg, ':', &values);
            if (values.empty())
                LOG_ERROR(name + " is missing at least one value!");
            if (values.size() == 1)
                names_to_values_map->insertScalar(name, values.front());
            else
                names_to_values_map->insertArray(name, values);
        }
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 3)
        Usage();

    try {
        const std::string template_filename(argv[1]);
        std::string template_string;
        if (not FileUtil::ReadString(template_filename, &template_string))
            logger->error("failed to read the template from \"" + template_filename + "\" for reading!");
        Template::Map names_to_values_map;
        ExtractNamesAndValues(argc, argv, &names_to_values_map);
        std::istringstream input(template_string);
        Template::ExpandTemplate(input, std::cout, names_to_values_map);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
