/** \brief Generates a list of values from LOK $a where $0=689.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


void ExtractLocalKeywords(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const local_keywords) {
    unsigned total_count(0), matched_count(0), records_with_local_data_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++total_count;

        bool matched(false);
        for (const auto &local_field : record.getTagRange("LOK")) {
            if (local_field.getLocalTag() != "689" or local_field.getLocalIndicator1() != ' ' or local_field.getLocalIndicator2() != ' ')
                continue;

            const MARC::Subfields subfields(local_field.getSubfields());
            for (const auto &subfield : subfields) {
                if (subfield.code_ == 'a') {
                    local_keywords->insert(subfield.value_);
                    matched = true;
                }
            }
        }
        if (matched)
            ++matched_count;
    }

    LOG_INFO("Processed a total of " + std::to_string(total_count) + " record(s) of which " + std::to_string(records_with_local_data_count)
             + " had local data.");
    LOG_INFO("Found " + std::to_string(matched_count) + " record(s) w/ local keywords.");
}


void DisplayKeywords(const std::unordered_set<std::string> &local_keywords) {
    std::vector<std::string> sorted_keywords;
    sorted_keywords.reserve(local_keywords.size());

    std::copy(local_keywords.cbegin(), local_keywords.cend(), std::back_inserter(sorted_keywords));
    std::sort(sorted_keywords.begin(), sorted_keywords.end());

    for (const auto &keyword : sorted_keywords)
        std::cout << keyword << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::unordered_set<std::string> local_keywords;
    ExtractLocalKeywords(marc_reader.get(), &local_keywords);
    DisplayKeywords(local_keywords);

    return EXIT_SUCCESS;
}
