/** \brief Utility for counting references to GND numbers.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <unordered_map>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--control-number-list=list_filename] gnd_number_list marc_data counts\n"
              << "       If a control-number-list filename has been specified only references of records\n"
              << "       matching entries in that file will be counted.\n\n";
    std::exit(EXIT_FAILURE);
}


void LoadGNDNumbers(File * const input, std::unordered_map<std::string, unsigned> * const gnd_numbers_and_counts) {
    while (not input->eof()) {
        std::string line;
        if (input->getline(&line) > 0)
            gnd_numbers_and_counts->emplace(line, 0);
    }

    std::cout << "Loaded " << gnd_numbers_and_counts->size() << " GND numbers.\n";
}


const std::vector<std::string> GND_REFERENCE_FIELDS{ "100", "600", "689", "700" };


void ProcessRecords(MARC::Reader * const marc_reader, const std::unordered_set<std::string> &filter_set,
                    std::unordered_map<std::string, unsigned> * const gnd_numbers_and_counts)
{
    unsigned matched_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        if (not filter_set.empty()) {
            if (filter_set.find(record.getControlNumber()) == filter_set.cend())
                continue;
        }

        for (const auto &gnd_reference_field : GND_REFERENCE_FIELDS) {
            for (const auto &field : record.getTagRange(gnd_reference_field)) {
                const MARC::Subfields subfields(field.getSubfields());
                for (const auto &subfield0 : subfields.extractSubfields('0')) {
                    if (subfield0.length() <= __builtin_strlen("(DE-588)") or not StringUtil::StartsWith(subfield0, "(DE-588)"))
                        continue;

                    const std::string gnd_number(subfield0.substr(__builtin_strlen("(DE-588)")));
                    const auto gnd_number_and_count(gnd_numbers_and_counts->find(gnd_number));
                    if (gnd_number_and_count != gnd_numbers_and_counts->end()) {
                        ++gnd_number_and_count->second;
                        ++matched_count;
                    }
                }
            }
        }
    }

    std::cerr << "Found " << matched_count << " reference(s) to " << gnd_numbers_and_counts->size()
              << " matching GND number(s).\n";
}


void WriteCounts(const std::unordered_map<std::string, unsigned> &gnd_numbers_and_counts, File * const output) {
    for (const auto &gnd_number_and_count : gnd_numbers_and_counts) {
        if (gnd_number_and_count.second > 0)
            (*output) << gnd_number_and_count.first << '|' << gnd_number_and_count.second << '\n';
    }
}


void LoadFilterSet(const std::string &input_filename, std::unordered_set<std::string> * const filter_set) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(input_filename));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::TrimWhite(&line);
        if (likely(not line.empty()))
            filter_set->emplace(line);
    }

    logger->info("loaded " + std::to_string(filter_set->size()) + " control numbers from \"" + input_filename
                 + "\".");
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4 and argc != 5)
        Usage();

    std::unordered_set<std::string> filter_set;
    if (argc == 5) {
        if (not StringUtil::StartsWith(argv[1], "--control-number-list="))
            Usage();
        LoadFilterSet(argv[1] + __builtin_strlen("--control-number-list="), &filter_set);
        --argc, ++argv;
    }

    try {
        std::unique_ptr<File> gnd_numbers_and_counts_file(FileUtil::OpenInputFileOrDie(argv[1]));
        std::unordered_map<std::string, unsigned> gnd_numbers_and_counts;
        LoadGNDNumbers(gnd_numbers_and_counts_file.get(), &gnd_numbers_and_counts);

        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
        ProcessRecords(marc_reader.get(), filter_set, &gnd_numbers_and_counts);

        std::unique_ptr<File> counts_file(FileUtil::OpenOutputFileOrDie(argv[3]));
        WriteCounts(gnd_numbers_and_counts, counts_file.get());
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
