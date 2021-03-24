/** \brief Utility for comparing keywords with gnd database.
 *  \author Hjordis Lindeboom
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("gnd_input.mrc keyword_input keyword_matches_output keyword_without_matches_output"
               "       Searches for keyword matches in the \"gnd_input\" MARC file."
               "       Returns a \"keyword_matches_output\" file with matching keywords and their PPN,"
               "       as well as \"keywords_without_matches\" file containing keywords where no matches were found.\n");
}


void ExtractSubfieldsForTag(const MARC::Record &record, const std::string &field_tag, const std::string &subfield_codes, std::vector<std::string> * const subfields) {
    const auto gnd_fields(record.findTag(field_tag));
    if (gnd_fields == record.end())
        return;
    const auto marc_subfields(gnd_fields->getSubfields());
    for (const char subfield_code : subfield_codes) {
        const std::vector<std::string> subfield_values(marc_subfields.extractSubfields(subfield_code));
        if (subfield_values.empty()) {
            if (subfield_code == 'a')
                LOG_WARNING("Entry has no Subfield 'a' for PPN " + record.getControlNumber());
            continue;
        }
        std::string subfield_value(subfield_values[0]);
        if (subfield_code == 'x')
            subfield_value = '(' + subfield_value + ')';
        subfields->emplace_back(subfield_value);
    }
}


void AddMainSubfieldAndCombinationsToGndKeywords(const MARC::Record &record, std::unordered_map<std::string, std::string> * const keywords_to_gnd_numbers_map, const std::string &field_tag, const std::string &subfield_codes, std::vector<std::string> * const subfields) {
    subfields->clear();
    // adding extra entry for every 'a' (main) subfield of every tag for a higher matching chance
    if (subfield_codes.find('a') != std::string::npos) {
        std::string subfield_value_a(record.getFirstSubfieldValue(field_tag, 'a'));
        if (not subfield_value_a.empty())
            keywords_to_gnd_numbers_map->emplace(std::make_pair(subfield_value_a, record.getControlNumber()));
    }
    ExtractSubfieldsForTag(record, field_tag, subfield_codes, subfields);
    if (subfields->size() > 1) {
        const std::string key(StringUtil::Join(*subfields, " "));
        keywords_to_gnd_numbers_map->emplace(std::make_pair(key , record.getControlNumber()));
    }
}


void ReadInGndKeywords(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const keywords_to_gnd_numbers_map)
{
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        std::vector<std::string> subfields;
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "150", "agx", &subfields);
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "100", "atg", &subfields);
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "110", "agx", &subfields);
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "111", "agx", &subfields);
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "130", "agx", &subfields);
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "151", "agx", &subfields);
    }
    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
}


void FindEquivalentKeywords(std::unordered_map<std::string, std::string> const &keywords_to_gnd_numbers_map, std::unordered_set<std::string> const &keywords_to_compare, const std::string matches_output_file, const std::string no_matches_output_file) {
    std::unordered_map<std::string, std::string> keyword_to_ppn_map;
    std::unordered_set<std::string> keywords_without_match;
    for (const auto &keyword : keywords_to_compare) {
        const auto gnd_number(keywords_to_gnd_numbers_map.find(keyword));
        if (gnd_number == keywords_to_gnd_numbers_map.end())
            keywords_without_match.insert(keyword);
        else
            keyword_to_ppn_map.insert(*gnd_number);
    }
    LOG_INFO("Found " + std::to_string(keyword_to_ppn_map.size()) + " keyword matches.\n");
    const double percentage((static_cast<double>(keyword_to_ppn_map.size()) / static_cast<double>(keywords_to_compare.size())) * 100);
    LOG_INFO("Which makes up for " + std::to_string(percentage) + "%\n");
    LOG_INFO("Couldn't find a match for " + std::to_string(keywords_without_match.size()) + " keyword(s).\n");
    std::ofstream output_file(matches_output_file);
    for (const auto &[key, value] : keyword_to_ppn_map)
        output_file << TextUtil::CSVEscape(key) << ',' << TextUtil::CSVEscape(value) << '\n';
    std::ofstream out_file(no_matches_output_file);
    for (const auto &word : keywords_without_match)
        out_file << TextUtil::CSVEscape(word) << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 5)
        Usage();

    const std::string filename(argv[2]);
    const std::string match_output(argv[3]);
    const std::string no_match_output(argv[4]);

    std::unordered_set<std::string> keywords_to_compare;
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(filename, &lines);
    for (const auto &keywords: lines) {
        for (const auto &keyword: keywords) {
            keywords_to_compare.insert(keyword);
        }
    }

    const std::string input_filename(argv[1]);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename));
    std::unordered_map<std::string, std::string> keywords_to_gnd_numbers_map;
    ReadInGndKeywords(marc_reader.get(), &keywords_to_gnd_numbers_map);
    FindEquivalentKeywords(keywords_to_gnd_numbers_map, keywords_to_compare, match_output, no_match_output);
    return EXIT_SUCCESS;
}
