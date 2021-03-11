/** \brief Utility for converting between MARC formats.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <set>
#include <iterator>
#include <utility>
#include <stdexcept>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("gnd_input keyword_input keyword_matches_output keyword_without_matches_output"
               "       Searches for keyword matches in the \"gnd_input\" file."
               "       Returns a \"keyword_matches_output\" file with matching keywords and their PPN,"
               "       as well as \"keywords_without_matches\" file containing keywords where no matches were found.\n");
}


void ReadInGndKeywords(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const gnd_keywords)
{
    unsigned record_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not record.getFirstSubfieldValue("150", 'a').empty())
            gnd_keywords->emplace(std::make_pair(record.getFirstSubfieldValue("150", 'a'), record.getControlNumber()));
        if (not record.getFirstSubfieldValue("150", 'g').empty())
            gnd_keywords->emplace(std::make_pair(record.getFirstSubfieldValue("150", 'g'), record.getControlNumber()));
        if (not record.getFirstSubfieldValue("150", 'x').empty())
            gnd_keywords->emplace(std::make_pair(record.getFirstSubfieldValue("150", 'x'), record.getControlNumber()));
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
    
}


void FindEquivalentKeywords(std::unordered_map<std::string, std::string> const *keywords_to_gnd, std::unordered_set<std::string> const *keywords_to_compare, const std::string matches_output_file, const std::string no_matches_output_file) {
    std::unordered_map<std::string, std::string> keyword_matches;
    std::unordered_set<std::string> keywords_without_match;
    for (const auto &keyword : *keywords_to_compare) {
        const auto lookup(keywords_to_gnd->find(keyword));
        if (lookup != keywords_to_gnd->end()) {
            keyword_matches.insert(*lookup);
            continue;
        }
        keywords_without_match.insert(keyword);
    }
    LOG_INFO("Found " + std::to_string(keyword_matches.size()) + " keyword matches.\n");
    double percentage = (static_cast<double>(keyword_matches.size())/static_cast<double>(keywords_to_compare->size())) * 100;
    LOG_INFO("Which makes up for " + std::to_string(percentage) + "%\n");
    LOG_INFO("Couldn't find a match for " + std::to_string(keywords_without_match.size()) + " keyword(s).\n");
    std::ofstream output_file(matches_output_file);
    for (const auto &[key, value] : keyword_matches) {
        output_file << "Keyword:  " << key << " PPN: " << value << "\n";
    }
    std::ofstream out_file(no_matches_output_file);
    for (const auto &word : keywords_without_match) {
        out_file << word << "\n";
    }
}


} // unnamed namespace


void  ReadInKeywordsToCompare(const std::string &filename, std::unordered_set<std::string> * const keywords) {
    std::ifstream input_file(filename.c_str());
    if (input_file.fail())
       throw std::runtime_error("in IniFile::processFile: can't open \"" + filename + "\"! ("
                                 + std::string(::strerror(errno)) + ")");

    // Read the file:
    while (not input_file.eof()) {
        std::string line;
        // read lines until newline character is not preceeded by a '\'
        bool continued_line;
        do {
            std::string buf;
            std::getline(input_file, buf);
            line += StringUtil::Trim(buf, " \t");
            if (line.empty())
                continue;

            continued_line = line[line.length() - 1] == '\\';
            if (continued_line)
                line = StringUtil::Trim(line.substr(0, line.length() - 1), " \t");
        } while (continued_line);
        // skip blank lines:
        if (line.length() == 0)
            continue;
    keywords->emplace(StringUtil::TrimWhite(line));    
    }
}


int Main(int argc, char *argv[]) {
    if (argc < 5)
        Usage();

    std::unordered_map<std::string, std::string> keywords_to_gnd;

    const std::string filename(argv[2]);
    const std::string match_output(argv[3]);
    const std::string no_match_output(argv[4]);

    std::unordered_set<std::string> keywords_to_compare;
    ReadInKeywordsToCompare(filename, &keywords_to_compare);

    const std::string input_filename(argv[1]);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename));
    ReadInGndKeywords(marc_reader.get(), &keywords_to_gnd);
    FindEquivalentKeywords(&keywords_to_gnd, &keywords_to_compare, match_output, no_match_output);
    return EXIT_SUCCESS;
}
