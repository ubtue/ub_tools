/** \brief Utility for displaying all non-standar tags in a MARC collection.
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

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MARC::Reader * const marc_reader) {
    unsigned record_count(0);
    std::unordered_set<std::string> non_standard_tags;
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        for (const auto &field : record) {
            const auto &tag(field.getTag());
            if (not MARC::IsStandardTag(tag))
                non_standard_tags.emplace(tag.toString());
        }
    }

    std::cout << "Data set contains " << record_count << " MARC record(s) w/ the following " << non_standard_tags.size()
              << " non-standard tags:\n";

    std::vector<std::string> sorted_non_standard_tag;
    sorted_non_standard_tag.reserve(non_standard_tags.size());

    for (const auto &non_standard_tag : non_standard_tags)
        sorted_non_standard_tag.emplace_back(non_standard_tag);

    std::sort(sorted_non_standard_tag.begin(), sorted_non_standard_tag.end());
    for (const auto &tag : sorted_non_standard_tag)
        std::cout << tag << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get());

    return EXIT_SUCCESS;
}
