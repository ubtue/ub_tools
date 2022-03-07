/** Test harness for MiscUtil::ExpandTemplate().
 */
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>
#include "FileUtil.h"
#include "StringUtil.h"
#include "Template.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " template_filename var1_and_values [var2_and_values ... varN_and_values]\n"
              << "       Variable names and values have to be separated by colons, arrays of array have to have their subarray\n"
              << "       values separated by semicolons. In order to include colons or semicolons in the values you need to backslash-\n"
              << "       escape them.  Literal backslashes can included by doubling them.\n"
              << "       You can specify names for empty arrays by using \"--empty-array\" followed by an array name anywhere"
              << "       in the argument list.\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessSingleArgument(const char * const arg, Template::Map * const names_to_values_map) {
    const char *ch(arg);
    std::string variable_name;
    while (*ch != ':') {
        if (unlikely(*ch == '\0'))
            LOG_ERROR("failed to find the colon separating the variable name from one or more values!");
        variable_name += *ch++;
    }
    ++ch; // Skip over colon.

    std::vector<std::vector<std::string>> columns;
    std::vector<std::string> column;
    bool escaped(false);
    std::string current_value;
    while (*ch != '\0') {
        if (escaped) {
            current_value += *ch;
            escaped = false;
        } else if (*ch == '\\')
            escaped = true;
        else {
            if (*ch == ':') {
                column.emplace_back(current_value);
                current_value.clear();
            } else if (*ch == ';') {
                if (columns.empty() or columns[0].size() == column.size())
                    columns.emplace_back(column);
                else
                    LOG_ERROR("a column of \"" + variable_name + "\" has " + std::to_string(column.size())
                              + " entries while the 1st column has " + std::to_string(columns[0].size()) + " entries!");
                column.clear();
            } else
                current_value += *ch;
        }

        ++ch;
    }
    column.emplace_back(current_value);

    if (unlikely(columns.empty() and column.empty()))
        LOG_ERROR("missing values for \"" + variable_name + "\"!");

    if (columns.empty()) {
        if (column.size() == 1)
            names_to_values_map->insertScalar(variable_name, column[0]);
        else
            names_to_values_map->insertArray(variable_name, column);
    } else {
        if (column.size() != columns[0].size())
            LOG_ERROR("last column of \"" + variable_name + "\" has " + std::to_string(column.size()) + " entries while the 1st column has "
                      + std::to_string(columns[0].size()) + " entries!");
        columns.emplace_back(column);

        std::vector<std::shared_ptr<Template::Value>> array_of_arrays;
        for (const auto &single_column : columns)
            array_of_arrays.emplace_back(new Template::ArrayValue(variable_name, single_column));
        names_to_values_map->insertArray(variable_name, array_of_arrays);
    }
}


void ExtractNamesAndValues(const int argc, char *argv[], Template::Map * const names_to_values_map) {
    names_to_values_map->clear();

    for (int arg_no(2); arg_no < argc; ++arg_no) {
        if (std::strcmp(argv[arg_no], "--empty-array") == 0) {
            ++arg_no;
            if (arg_no == argc)
                LOG_ERROR("missing empty array name at end of argument list");
            names_to_values_map->insertArray(argv[arg_no], std::vector<std::string>());
        } else
            ProcessSingleArgument(argv[arg_no], names_to_values_map);
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
