/** \brief Generates a list of values from LOK $a where $0=689.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <vector>
#include <cstdlib>
//#include <cstring>
#include "FileUtil.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


void ExtractLocalKeywords(MarcReader * const marc_reader, std::unordered_set<std::string> * const local_keywords) {
    unsigned total_count(0), matched_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++total_count;

        std::vector<size_t> field_indices;
        if (not record.getFieldIndices("LOK", &field_indices))
            continue;

        bool matched(false);
        for (size_t field_index : field_indices) {
            const Subfields subfields(record.getFieldData(field_index));
            const std::string subfield0(subfields.getFirstSubfieldValue('0'));
            if (subfield0.empty() or subfield0 != "689")
                continue;
            const std::string subfield_a(subfields.getFirstSubfieldValue('a'));
            if (not subfield_a.empty()) {
                local_keywords->insert(subfield_a);
                matched = true;
            }
        }
        if (matched)
            ++matched_count;
    }

    std::cerr << "Processed a total of " << total_count << " record(s).\n";
    std::cerr << "Found " << matched_count << " record(s) w/ local keywords.\n";
}


void DisplayKeywords(const std::unordered_set<std::string> &local_keywords) {
    std::vector<std::string> sorted_keywords;
    sorted_keywords.reserve(local_keywords.size());

    std::copy(local_keywords.cbegin(), local_keywords.cend(), std::back_inserter(sorted_keywords));
    std::sort(sorted_keywords.begin(), sorted_keywords.end());

    for (const auto &keyword : sorted_keywords)
        std::cout << keyword << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        std::unordered_set<std::string> local_keywords;
        ExtractLocalKeywords(marc_reader.get(), &local_keywords);
        DisplayKeywords(local_keywords);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
