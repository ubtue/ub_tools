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
            logger->error("line with a confusing structure in \"" + input->getPath() + "\", line #"
                          + std::to_string(line_no) + "!");

        language_mapping->emplace_back(lhs, rhs);
    }
}


/** A hashing functor for pairs of strings which uses only the first string of a pair to generate
    the hash value. */
struct StringPairHash {
    inline size_t operator()(const std::pair<std::string, std::string> &string_pair) const {
        return std::hash<std::string>()(string_pair.first);
    }
};


/** An equality functor for pairs of strings which uses only the first string of a pair to determine equality. */
struct StringPairEqual {
    inline bool operator()(const std::pair<std::string, std::string> &lhs_string_pair,
                           const std::pair<std::string, std::string> &rhs_string_pair) const
    {
        return lhs_string_pair.first == rhs_string_pair.first;
    }
};


using StringPairSet = std::unordered_set<std::pair<std::string, std::string>, StringPairHash, StringPairEqual>;


void ReadIniFileAndCollectEntries(File * const input, StringPairSet * const lhs_entries) {
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
            logger->error("line with a confusing structure in \"" + input->getPath() + "\", line #"
                          + std::to_string(line_no) + "!");

        lhs_entries->insert(std::make_pair(lhs, rhs));
    }
}


// Reports keys that exist in the other language file but not in the reference file.
void ReportMissingEntriesInTheReferenceFile(
    const std::vector<std::pair<std::string, std::string>> &reference_language_mapping,
    const StringPairSet &lhs_entries)
{
    std::unordered_set<std::string> reference_keys;
    for (const auto &ref_pair : reference_language_mapping)
        reference_keys.insert(ref_pair.first);

    unsigned missing_count(0);
    for (const auto &lhs_entry : lhs_entries) {
        if (reference_keys.find(lhs_entry.first) == reference_keys.cend()) {
            std::cout << lhs_entry.first << " is a missing key in the reference file.\n";
            ++missing_count;
        }
    }
    std::cout << "Found " << missing_count << " missing entries in the reference file.\n";
}


void InsertMissingTranslations(File * const output,
                               const std::vector<std::pair<std::string, std::string>> &reference_language_mapping,
                               const StringPairSet &lhs_entries)
{
    unsigned total_count(0), missing_count(0), comment_count(0);

    std::set<std::string> already_seen;
    for (const auto &ref_pair : reference_language_mapping) {
        ++total_count;
        const auto set_entry(lhs_entries.find(ref_pair));

        if (already_seen.find(ref_pair.first) != already_seen.end())
            continue;
        already_seen.insert(ref_pair.first);

        if (set_entry != lhs_entries.cend()) {
            *output << set_entry->first << " = " << '"' << set_entry->second << "\"\n";
        } else {
            *output << ref_pair.first << " = " << "\"\"";
            if (ref_pair.first != ref_pair.second) {
                *output << "  //** " << ref_pair.second;
            }
            ++comment_count;
            *output << '\n';
            ++missing_count;
        }
    }

    std::cout << "Processed " << total_count << " entries.\n";
    std::cout << "Found " << missing_count << " missing entries.\n";
    std::cout << "Inserted " << comment_count << " comments.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string reference_filename(argv[1]);
    const std::string other_language_filename(argv[2]);
    const std::string output_filename(argv[3]);

    if (reference_filename == other_language_filename)
        logger->error("other language file name must differ from the reference file name!");
    if (reference_filename == output_filename)
        logger->error("the reference file name must differ from the output file name!");
    if (other_language_filename == output_filename)
        logger->error("other language file name must differ from the output file name!");

    File reference_file(reference_filename, "r");
    if (not reference_file)
        logger->error("can't open \"" + reference_filename + "\" for reading!");
    File other_language_file(other_language_filename, "r");
    if (not other_language_file)
        logger->error("can't open \"" + other_language_filename + "\" for reading!");
    File output(output_filename, "w");
    if (not output)
        logger->error("can't open \"" + output_filename + "\" for writing!");

    try {
        std::vector<std::pair<std::string, std::string>> reference_language_mapping;
        ReadIniFile(&reference_file, &reference_language_mapping);

        StringPairSet lhs_entries;
        ReadIniFileAndCollectEntries(&other_language_file, &lhs_entries);

        ReportMissingEntriesInTheReferenceFile(reference_language_mapping, lhs_entries);
        InsertMissingTranslations(&output, reference_language_mapping, lhs_entries);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
