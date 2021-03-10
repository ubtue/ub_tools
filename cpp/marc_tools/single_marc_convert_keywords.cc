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
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--quiet] [--limit max_no_of_records] [--output-individual-files] marc_input marc_output [CTLN_1 CTLN_2 .. CTLN_N]\n"
              << "       Autoconverts the MARC format of \"marc_input\" to \"marc_output\".\n"
              << "       Supported extensions are \"xml\", \"mrc\", \"marc\" and \"raw\".\n"
              << "       All extensions except for \"xml\" are assumed to imply MARC-21.\n"
              << "       If a control number list has been specified only those records will\n"
              << "       be extracted or converted.\n"
              << "       If --output-individual-files is specified marc_output must be a writable directory\n"
              << "       and files are named from the control numbers and written as XML\n\n";
    std::exit(EXIT_FAILURE);
}


void readInGndKeywords(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const gnd_keywords)
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

        const std::string controlNumber = record.getControlNumber();
        const std::string keywordSubfield = record.getFirstSubfieldValue("150", 'a');
        const std::string keywordSubfieldG = record.getFirstSubfieldValue("150", 'g');
        const std::string keywordSubfieldX = record.getFirstSubfieldValue("150", 'x');

       // if (not keywordSubfield.empty() or not keywordSubfieldG.empty() or not keywordSubfieldX.empty())
       //     std::cout << controlNumber << " a: " << keywordSubfield << " g:  " << keywordSubfieldG << " x: " << keywordSubfieldX << "\n";

    }

        LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
    
}

void findEquivalentKeywords(std::unordered_map<std::string, std::string> keywords_to_gnd, std::set<std::string> keywords_to_compare) {
    std::map<std::string, std::string> keyword_matches;
    std::set<std::string> keywords_without_match;
    std::unordered_map<std::string, std::string>::iterator lookup;
    for(const auto &keyword : keywords_to_compare) {
        lookup = keywords_to_gnd.find(keyword);
        if (lookup != keywords_to_gnd.end()) {
            keyword_matches.insert(*lookup);
            continue;
        }
        keywords_without_match.insert(keyword);
    }
    std::cout << "Found: " << keyword_matches.size() << " matches.\n";
    std::cout << "Found: " << keywords_without_match.size() << " entries that didn't match.\n";

    std::ofstream output_file;
    output_file.open ("keyword_matches.txt");
    for(const auto& [key, value]: keyword_matches) {
        output_file << "Keyword:  " << key << " PPN: " << value << "\n";
    }
    output_file.close();

    std::ofstream out_file;
    out_file.open ("keywords_without_match.txt");
    for (const auto &word : keywords_without_match) {
        out_file << word << "\n";
    }
    out_file.close();
}


} // unnamed namespace

void  readInKeywordsToCompare(const std::string &filename, std::set<std::string> * const keywords) {
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
    keywords->emplace(line);    
    //std::cout << line << "\n";
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 3)
        Usage();

    std::unordered_map<std::string, std::string> keywords_to_gnd;

    std::string filename = argv[2];

    std::set<std::string> keywords_to_compare;
    readInKeywordsToCompare(filename, &keywords_to_compare);
//    for(const auto &keyword : keywords_to_compare) {
//        std::cout << keyword << "\n ";
//    }

    const std::string input_filename(argv[1]);

    try {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename));
        readInGndKeywords(marc_reader.get(), &keywords_to_gnd);
//        for (const auto& [key, value]: keywords_to_gnd) {
//            std::cout << key << " has value " << value << "\n";
//        }
        findEquivalentKeywords(keywords_to_gnd, keywords_to_compare);
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }
}
