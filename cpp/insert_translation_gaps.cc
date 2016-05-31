/** \file insert_translation_gaps.cc
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
#include <unordered_set>
#include <utility>
#include <vector>
#include "Compiler.h"
#include "File.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " reference_file other_language_file output_file\n";
    std::cerr << "       The reference file is typically en.ini.\n";
    std::exit(EXIT_FAILURE);
}


// Returns the cleaned-up parts of the line before and after the equal sign.
bool SplitLine(const std::string &line, std::string * const lhs, std::string * const rhs) {
    const size_t first_equal_pos(line.find('='));
    if (unlikely(first_equal_pos == std::string::npos))
        return false;

    *lhs = line.substr(0, first_equal_pos);
    if (unlikely(StringUtil::Trim(lhs).empty()))
        return false;

    *rhs = line.substr(first_equal_pos + 1);
    if (unlikely(StringUtil::Trim(rhs).empty()))
        return false;

    // Strip enclosing double quotes?
    if ((*rhs)[0] == '"') {
        if (unlikely(StringUtil::Trim(rhs, '"').empty()))
            return false;
    }

    return true;
}


void ReadIniFile(File * const input, std::vector<std::pair<std::string, std::string>> * const language_mapping) {
    language_mapping->clear();

    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;

        std::string line;
        input->getline(&line);
        if (unlikely(StringUtil::StartsWith(line, ";")))
            continue; // a comment line
        StringUtil::Trim(&line);
        if (unlikely(line.empty()))
            continue;

        std::string lhs, rhs;
        if (unlikely(not SplitLine(line, &lhs, &rhs)))
            Error("line with more than one equal sign in \"" + input->getPath() + "\", line #"
                  + std::to_string(line_no) + "!");

        language_mapping->emplace_back(lhs, rhs);
    }
}


void ReadIniFileAndCollectEntries(File * const input, std::unordered_set<std::string> * const lhs_entries) {
    lhs_entries->clear();

    unsigned line_no(0);
    while (not input->eof()) {
        ++line_no;

        std::string line;
        input->getline(&line);
        if (unlikely(StringUtil::StartsWith(line, ";")))
            continue; // a comment line
        StringUtil::Trim(&line);
        if (unlikely(line.empty()))
            continue;

        std::string lhs, rhs;
        if (unlikely(not SplitLine(line, &lhs, &rhs)))
            Error("line with more than one equal sign in \"" + input->getPath() + "\", line #"
                  + std::to_string(line_no) + "!");

        lhs_entries->insert(lhs);
    }
}


void InsertMissingTranslations(File * const output,
                               const std::vector<std::pair<std::string, std::string>> &reference_language_mapping,
                               const std::unordered_set<std::string> &lhs_entries)
{
    unsigned total_count(0), missing_count(0);
    for (const auto &ref_pair : reference_language_mapping) {
        ++total_count;
        if (lhs_entries.find(ref_pair.first) != lhs_entries.cend())
            *output << ref_pair.first << " = " << '"' << ref_pair.second << "\"\n";
        else {
            *output << ref_pair.first << " = " << "\"\"\n";
            ++missing_count;
        }
    }

    std::cout << "Processed " << total_count << " entries.\n";
    std::cout << "Found " << missing_count << " missing entries.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string reference_filename(argv[1]);
    const std::string other_language_filename(argv[2]);
    const std::string output_filename(argv[3]);

    if (reference_filename == other_language_filename)
        Error("other language file name must differ from the reference file name!");
    if (reference_filename == output_filename)
        Error("the reference file name must differ from the output file name!");
    if (other_language_filename == output_filename)
        Error("other language file name must differ from the output file name!");

    File reference_file(reference_filename, "r");
    if (not reference_file)
        Error("can't open \"" + reference_filename + "\" for reading!");
    File other_language_file(other_language_filename, "r");
    if (not other_language_file)
        Error("can't open \"" + other_language_filename + "\" for reading!");
    File output(output_filename, "w");
    if (not output)
        Error("can't open \"" + output_filename + "\" for writing!");

    try {
        std::vector<std::pair<std::string, std::string>> reference_language_mapping;
        ReadIniFile(&reference_file, &reference_language_mapping);

        std::unordered_set<std::string> lhs_entries;
        ReadIniFileAndCollectEntries(&other_language_file, &lhs_entries);

        InsertMissingTranslations(&output, reference_language_mapping, lhs_entries);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
