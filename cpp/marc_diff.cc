/** \brief Utility for two collections of MARC records.
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
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_collection1 marc_collection2\n";
    std::exit(EXIT_FAILURE);
}


unsigned LoadMap(MARC::Reader * const marc_reader, std::unordered_map<std::string, off_t> * const control_number_to_offset_map) {
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;
        (*control_number_to_offset_map)[record.getControlNumber()] = marc_reader->tell();
    }

    return record_count;
}


void EmitDetailedReport(const std::string &/*collection1_name*/, const std::string &/*collection2_name*/,
                        const std::unordered_map<std::string, off_t> &/*control_number_to_offset_map1*/,
                        const std::unordered_map<std::string, off_t> &/*control_number_to_offset_map2*/,
                        MARC::Reader * const /*reader1*/, MARC::Reader * const /*reader2*/)
{
}


inline void InitSortedControlNumbersList(const std::unordered_map<std::string, off_t> &control_number_to_offset_map,
                                         std::vector<std::string> * const sorted_control_numbers)
{
    sorted_control_numbers->clear();
    sorted_control_numbers->reserve(control_number_to_offset_map.size());
    for (const auto &control_number_and_offset : control_number_to_offset_map)
        sorted_control_numbers->emplace_back(control_number_and_offset.first);
    std::sort(sorted_control_numbers->begin(), sorted_control_numbers->end());
}


void EmitStandardReport(const std::string &collection1_name, const std::string &collection2_name,
                        const unsigned collection1_size, const unsigned collection2_size,
                        const std::unordered_map<std::string, off_t> &control_number_to_offset_map1,
                        const std::unordered_map<std::string, off_t> &control_number_to_offset_map2)
{
    std::vector<std::string> sorted_control_numbers1;
    InitSortedControlNumbersList(control_number_to_offset_map1, &sorted_control_numbers1);

    std::vector<std::string> sorted_control_numbers2;
    InitSortedControlNumbersList(control_number_to_offset_map2, &sorted_control_numbers2);

    unsigned in_map1_only_count(0), in_map2_only_count(0);

    auto control_number1(sorted_control_numbers1.cbegin());
    auto control_number2(sorted_control_numbers2.cbegin());
    while (control_number1 != sorted_control_numbers1.cend() and control_number2 != sorted_control_numbers2.cend()) {
        if (*control_number1 == *control_number2) {
            ++control_number1;
            ++control_number2;
        } else if (*control_number1 < *control_number2) {
            ++in_map1_only_count;
            ++control_number1;
        } else { // *control_number2 < *control_number1
            ++in_map2_only_count;
            ++control_number2;
        }
    }

    in_map1_only_count += sorted_control_numbers1.cend() - control_number1;
    in_map2_only_count += sorted_control_numbers2.cend() - control_number2;

    std::cout << '"' << collection1_name << "\" contains " << collection1_size << " record(s).\n";
    std::cout << '"' << collection2_name << "\" contains " << collection2_size << " record(s).\n";
    std::cout << in_map1_only_count << " record(s) are only in \"" << collection1_name << "\" but not in \"" << collection2_name
              << "\".\n";
    std::cout << in_map2_only_count << " record(s) are only in \"" << collection2_name << "\" but not in \"" << collection1_name
              << "\".\n";
    std::cout << (collection1_size - in_map1_only_count) << " are in both collections.\n";
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 3)
        Usage();

    try {
        const std::string collection1_name(argv[1]);
        const std::string collection2_name(argv[2]);
        std::unique_ptr<MARC::Reader> marc_reader1(MARC::Reader::Factory(collection1_name));
        std::unique_ptr<MARC::Reader> marc_reader2(MARC::Reader::Factory(collection2_name));

        std::unordered_map<std::string, off_t> control_number_to_offset_map1;
        const unsigned collection1_size(LoadMap(marc_reader1.get(), &control_number_to_offset_map1));

        std::unordered_map<std::string, off_t> control_number_to_offset_map2;
        const unsigned collection2_size(LoadMap(marc_reader2.get(), &control_number_to_offset_map2));

        if (verbose)
            EmitDetailedReport(collection1_name, collection2_name, control_number_to_offset_map1, control_number_to_offset_map2,
                               marc_reader1.get(), marc_reader2.get());

        EmitStandardReport(collection1_name, collection2_name, collection1_size, collection2_size, control_number_to_offset_map1,
                           control_number_to_offset_map2);
    } catch (const std::exception &e) {
        ERROR("Caught exception: " + std::string(e.what()));
    }
}
