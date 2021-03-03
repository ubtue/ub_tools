/** \file mysql_diff_schemas.cc
 *  \brief A tool for listing the differences between two schemas.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <cstdlib>
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


std::string ExtractBackQuotedString(const std::string &s) {
    auto ch(s.cbegin());
    if (unlikely(*ch != '`'))
        LOG_ERROR("\"" + s + "\" does not start with a backtick!");
    ++ch;

    std::string extracted_string;
    while (ch != s.cend() and *ch != '`')
        extracted_string += *ch++;
    if (unlikely(ch == s.cend()))
        LOG_ERROR("\"" + s + "\" does not end with a backtick!");

    return extracted_string;
}


inline bool SchemaLineIsLessThan(const std::string &line1, const std::string &line2) {
    if ((*line1.c_str() == '`') != (*line2.c_str() == '`'))
        return *line1.c_str() == '`';

    return line1 < line2;
}


void LoadSchema(const std::string &filename,
                std::map<std::string, std::vector<std::string>> * const table_or_view_name_to_schema_map)
{
    std::string current_table_or_view;
    std::vector<std::string> current_schema;
    for (auto line : FileUtil::ReadLines(filename)) {
        StringUtil::Trim(&line);
        const bool line_starts_with_create_table(StringUtil::StartsWith(line, "CREATE TABLE "));
        const bool line_starts_with_create_view(line_starts_with_create_table ? false
                                                : StringUtil::StartsWith(line, "CREATE VIEW "));
        if (line_starts_with_create_table or line_starts_with_create_view) {
            if (not current_table_or_view.empty()) {
                std::sort(current_schema.begin(), current_schema.end(), SchemaLineIsLessThan);
                (*table_or_view_name_to_schema_map)[current_table_or_view] = current_schema;
            }
            current_table_or_view = ExtractBackQuotedString(line_starts_with_create_table
                                                            ? line.substr(__builtin_strlen("CREATE TABLE "))
                                                            : line.substr(__builtin_strlen("CREATE VIEW ")));
            current_schema.clear();
        } else {
            if (line[line.length() - 1] == ',')
                line = line.substr(0, line.length() - 1);
            current_schema.emplace_back(line);
        }
    }
    if (not current_schema.empty()) {
        std::sort(current_schema.begin(), current_schema.end(), SchemaLineIsLessThan);
        (*table_or_view_name_to_schema_map)[current_table_or_view] = current_schema;
    }
}


std::vector<std::string> FindLinesStartingWithPrefix(const std::vector<std::string> &lines, const std::string &prefix) {
    std::vector<std::string> matching_lines;
    for (const auto &line : lines) {
        if (StringUtil::StartsWith(line, prefix))
            matching_lines.emplace_back(line);
    }
    return matching_lines;
}


// Reports differences for lines that start w/ "prefix" for tables w/ the identical names.
void CompareTables(const std::string &prefix,
                   const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map1,
                   const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map2)
{
    for (const auto &table_name1_and_schema1 : table_or_view_name_to_schema_map1) {
        const auto &table_name1(table_name1_and_schema1.first);
        const auto table_name2_and_schema2(table_or_view_name_to_schema_map2.find(table_name1));
        if (table_name2_and_schema2 == table_or_view_name_to_schema_map2.cend())
            continue;
        const auto &schema2(table_name2_and_schema2->second);

        const auto matching_lines_in_table1(FindLinesStartingWithPrefix(table_name1_and_schema1.second, prefix));
        for (const auto &matching_line_in_table1 : matching_lines_in_table1) {
            const auto matching_line_in_schema2(std::find(schema2.cbegin(), schema2.cend(), matching_line_in_table1));
            if (matching_line_in_schema2 == schema2.cend())
                std::cout << matching_line_in_table1 << " is missing in 2nd schema for table " << table_name1 << '\n';
        }
        for (const auto &line_in_table2 : schema2) {
            if (StringUtil::StartsWith(line_in_table2, prefix)
                and std::find(matching_lines_in_table1.cbegin(), matching_lines_in_table1.cend(), line_in_table2)
                    == matching_lines_in_table1.cend())
                std::cout << line_in_table2 << " is missing in 1st schema for table " << table_name1 << '\n';
        }
    }
}


class StartsWith {
    std::string prefix_;
public:
    explicit StartsWith(const std::string &prefix): prefix_(prefix) { }
    bool operator ()(const std::string &line) const { return StringUtil::StartsWith(line, prefix_); }
};


class StartsWithColumnName: public StartsWith {
public:
    explicit StartsWithColumnName(const std::string &reference_column_name): StartsWith("`" + reference_column_name + "`") { }
};


void CompareTableOptions(const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map1,
                         const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map2)
{
    for (const auto &table_name1_and_schema1 : table_or_view_name_to_schema_map1) {
        const auto &table_name1(table_name1_and_schema1.first);
        const auto table_name2_and_schema2(table_or_view_name_to_schema_map2.find(table_name1));
        if (table_name2_and_schema2 == table_or_view_name_to_schema_map2.cend())
            continue;

        const auto &schema1(table_name1_and_schema1.second);
        const auto table_options1(std::find_if(schema1.cbegin(), schema1.cend(), StartsWith(") ")));
        if (unlikely(table_options1 == schema1.cend()))
            LOG_ERROR("No table options line for table \"" + table_name1 + "\" found in 1st schema!");

        const auto &schema2(table_name2_and_schema2->second);
        const auto table_options2(std::find_if(schema2.cbegin(), schema2.cend(), StartsWith(") ")));
        if (unlikely(table_options2 == schema2.cend()))
            LOG_ERROR("No table options line for table \"" + table_name1 + "\" found in 2nd schema!");

        static RegexMatcher * const auto_increment_matcher(RegexMatcher::RegexMatcherFactoryOrDie("\\s*AUTO_INCREMENT=\\d+"));
        const std::string cleaned_table_options1(auto_increment_matcher->replaceAll(table_options1->substr(2), ""));
        const std::string cleaned_table_options2(auto_increment_matcher->replaceAll(table_options2->substr(2), ""));

        if (cleaned_table_options1 != cleaned_table_options2)
            std::cerr << "Table options differ for " << table_name1 << ": " << cleaned_table_options1 << " -> "
                      << cleaned_table_options2 << '\n';
    }
}


void ReportUnknownLines(const std::string &schema,
                        const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map)
{
    static const std::vector<std::string> KNOWN_LINE_PREFIXES{ "KEY", "PRIMARY KEY", "UNIQUE KEY", "CONSTRAINT", ") ", "`" };

    for (const auto &table_name_and_schema : table_or_view_name_to_schema_map) {
        for (const auto &line : table_name_and_schema.second) {
            bool found_a_known_prefix(false);
            for (const auto &known_prefix : KNOWN_LINE_PREFIXES) {
                if (StringUtil::StartsWith(line, known_prefix)) {
                    found_a_known_prefix = true;
                    break;
                }
            }

            if (not found_a_known_prefix)
                LOG_ERROR("Unknown line type in " + schema + ", table " + table_name_and_schema.first + ": " + line);
        }
    }
}


void DiffSchemas(const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map1,
                 const std::map<std::string, std::vector<std::string>> &table_or_view_name_to_schema_map2)
{
    std::set<std::string> already_processed_table_or_view_names;
    for (const auto &table_or_view_name1_and_schema1 : table_or_view_name_to_schema_map1) {
        const auto &table_or_view_name1(table_or_view_name1_and_schema1.first);
        already_processed_table_or_view_names.emplace(table_or_view_name1);
        const auto table_or_view_name2_and_schema2(table_or_view_name_to_schema_map2.find(table_or_view_name1));
        if (table_or_view_name2_and_schema2 == table_or_view_name_to_schema_map2.cend()) {
            std::cout << "Table or view was deleted: " << table_or_view_name1 << '\n';
            continue;
        }

        const auto &schema1(table_or_view_name1_and_schema1.second);
        const auto &schema2(table_or_view_name2_and_schema2->second);

        // Compare column definitions first:
        const auto last_column_def1(std::find_if(schema1.crbegin(), schema1.crend(),
                                                 [](const std::string &line){ return not line.empty() and line[0] == '`'; }));
        const auto last_column_def2(std::find_if(schema2.crbegin(), schema2.crend(),
                                                 [](const std::string &line){ return not line.empty() and line[0] == '`'; }));

        std::set<std::string> already_processed_column_names;
        for (auto column_def1(schema1.cbegin()); column_def1 != last_column_def1.base(); ++column_def1) {
            const auto column_name1(ExtractBackQuotedString(*column_def1));
            already_processed_column_names.emplace(column_name1);
            const auto column_def2(std::find_if(schema2.cbegin(), last_column_def2.base(), StartsWithColumnName(column_name1)));
            if (column_def2 == last_column_def2.base())
                std::cout << "Column does not exist in 1st schema: " << table_or_view_name1 << '.' << column_name1 << '\n';
            else if (*column_def1 != *column_def2)
                std::cout << "Column definition differs between the 1st and 2nd schemas (" << table_or_view_name1 << "): "
                          << *column_def1 << " -> " << *column_def2 << '\n';
        }

        for (auto column_def2(schema2.cbegin()); column_def2 != last_column_def2.base(); ++column_def2) {
            const auto column_name2(ExtractBackQuotedString(*column_def2));
            if (already_processed_column_names.find(column_name2) == already_processed_column_names.cend())
                std::cout << "Column exists only in 2nd schema: " << table_or_view_name1 << '.' << column_name2 << '\n';
        }
    }

    std::set<std::string> schema2_tables;
    for (const auto &table_or_view_name2_and_schema2 : table_or_view_name_to_schema_map2) {
        const auto &table_or_view_name2(table_or_view_name2_and_schema2.first);
        schema2_tables.emplace(table_or_view_name2);
        if (already_processed_table_or_view_names.find(table_or_view_name2) == already_processed_table_or_view_names.cend())
            std::cout << "Table or view exists only in 1st schema: " << table_or_view_name2 << '\n';
    }
    for (const auto &table_or_view_name1_and_schema1 : table_or_view_name_to_schema_map1) {
        const auto &table_or_view_name1(table_or_view_name1_and_schema1.first);
        if (schema2_tables.find(table_or_view_name1) == schema2_tables.cend())
            std::cout << "Table or view exist only in 2st schema: " << table_or_view_name1 << '\n';
    }

    CompareTables("KEY", table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);
    CompareTables("PRIMARY KEY", table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);
    CompareTables("UNIQUE KEY", table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);
    CompareTables("CONSTRAINT", table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);
    CompareTableOptions(table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);

    ReportUnknownLines("schema1", table_or_view_name_to_schema_map1);
    ReportUnknownLines("schema2", table_or_view_name_to_schema_map2);
}


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("schema1 schema2\n"
                "Please note that this tool may not work particularly well if you do not use output from mysql_list_tables");

    std::map<std::string, std::vector<std::string>> table_or_view_name_to_schema_map1;
    LoadSchema(argv[1], &table_or_view_name_to_schema_map1);

    std::map<std::string, std::vector<std::string>> table_or_view_name_to_schema_map2;
    LoadSchema(argv[2], &table_or_view_name_to_schema_map2);

    DiffSchemas(table_or_view_name_to_schema_map1, table_or_view_name_to_schema_map2);

    return EXIT_SUCCESS;
}
