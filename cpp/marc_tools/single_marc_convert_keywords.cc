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
    const auto gnd_field(record.findTag(field_tag));
    if (gnd_field == record.end())
        return;
    const auto marc_subfields(gnd_field->getSubfields());
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


void AddMainSubfieldAndCombinationsToGndKeywords(const MARC::Record &record, std::unordered_map<std::string, std::string> * const keywords_to_gnd_numbers_map, const std::string &field_tag, const std::string &subfield_codes) {
    if (subfield_codes.find('a') != std::string::npos) {
        const std::string subfield_value_a(record.getFirstSubfieldValue(field_tag, 'a'));
        if (not subfield_value_a.empty()) {
            std::string gnd_code;
            if (not MARC::GetGNDCode(record, &gnd_code))
                LOG_WARNING("Unable to extract GND Code for " + record.getControlNumber());
            keywords_to_gnd_numbers_map->emplace(std::make_pair(subfield_value_a, record.getControlNumber() + ';' + gnd_code  + ';' + field_tag));
        }
    }
    std::vector<std::string> subfields;
    ExtractSubfieldsForTag(record, field_tag, subfield_codes, &subfields);
    if (subfields.size() > 1) {
        const std::string key(StringUtil::Join(subfields, " "));
        keywords_to_gnd_numbers_map->emplace(std::make_pair(key , record.getControlNumber()));
    }
}


void ReadInGndKeywords(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const keywords_to_gnd_numbers_map)
{
    unsigned record_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "150", "agx");
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "100", "atg");
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "110", "agx");
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "111", "agx");
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "130", "agx");
        AddMainSubfieldAndCombinationsToGndKeywords(record, keywords_to_gnd_numbers_map, "151", "agx");
    }
    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
}


void FindEquivalentKeywords(std::unordered_map<std::string, std::string> const &keywords_to_gnd_numbers_map, std::unordered_set<std::string> const &keywords_to_compare,  File * const matches_output_file, File * const no_matches_output_file) {
    std::unordered_map<std::string, std::string> keywords_to_ppns_map;
    std::unordered_set<std::string> keywords_without_match;
    for (const auto &keyword : keywords_to_compare) {
        const auto gnd_number(keywords_to_gnd_numbers_map.find(keyword));
        if (gnd_number == keywords_to_gnd_numbers_map.end())
            keywords_without_match.insert(keyword);
        else {
            keywords_to_ppns_map.insert(*gnd_number);
            LOG_INFO("Keyword '" + keyword + "' matched to PPN & Tag '" + gnd_number->second + "' \n");
        }
    }
    LOG_INFO("Found " + std::to_string(keywords_to_ppns_map.size()) + " keyword match(es).\n");
    const double percentage((static_cast<double>(keywords_to_ppns_map.size()) / static_cast<double>(keywords_to_compare.size())) * 100);
    LOG_INFO("Which makes up for " + std::to_string(percentage) + "%\n");
    LOG_INFO("Couldn't find a match for " + std::to_string(keywords_without_match.size()) + " keyword(s).\n");
    for (const auto &[key, value] : keywords_to_ppns_map)
        *matches_output_file << TextUtil::CSVEscape(key) << ',' << TextUtil::CSVEscape(value) << '\n';
    for (const auto &word : keywords_without_match)
        *no_matches_output_file << TextUtil::CSVEscape(word) << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 5)
        Usage();

    const std::string filename(argv[2]);

    std::unordered_set<std::string> keywords_to_compare;
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(filename, &lines);
    for (const auto &keywords: lines) {
        for (const auto &keyword: keywords) {
            keywords_to_compare.insert(keyword);
        }
    }

    const std::string input_filename(argv[1]);
    const auto match_output(FileUtil::OpenOutputFileOrDie(argv[3]));
    const auto no_match_output(FileUtil::OpenOutputFileOrDie(argv[4]));

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename));
    std::unordered_map<std::string, std::string> keywords_to_gnd_numbers_map;
    ReadInGndKeywords(marc_reader.get(), &keywords_to_gnd_numbers_map);
    FindEquivalentKeywords(keywords_to_gnd_numbers_map, keywords_to_compare, match_output.get(), no_match_output.get());
    return EXIT_SUCCESS;
}
