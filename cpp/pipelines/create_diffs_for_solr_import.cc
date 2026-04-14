/** \brief Utility for two collections of MARC records.
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2026 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <xxhash.h>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"

namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_collection1 marc_collection2 filename_output1 filename_output2\n"
              << "\t[--extract]\tprint more info.\n"
              << "\t-marc_collection1\tfirst MARC collection.\n"
              << "\t-marc_collection2\tsecond MARC collection.\n"
              << "\t-filename_ouput1\twill be a text file that lists all the IDs from collection1 that are not found in collection2.\n"
              << "\t-filename_output2\twill be a Marc file with all records that are found in collection2 but not in collection1, as well "
                 "as records that have the same ID in both collections but different content.\n";
    std::exit(EXIT_FAILURE);
}

inline void InitSortedControlNumbersList(const std::unordered_map<std::string, std::string> &control_number_to_offset_map,
                                         std::vector<std::string> * const sorted_control_numbers) {
    sorted_control_numbers->clear();
    sorted_control_numbers->reserve(control_number_to_offset_map.size());
    for (const auto &control_number_and_offset : control_number_to_offset_map)
        sorted_control_numbers->emplace_back(control_number_and_offset.first);
    std::sort(sorted_control_numbers->begin(), sorted_control_numbers->end());
}

void ConstructListOfDifferentIDs(const std::unordered_map<std::string, std::string> &control_number_to_record_hash_map1,
                                 const std::unordered_map<std::string, std::string> &control_number_to_record_hash_map2,
                                 std::unordered_set<std::string> *ids_in_collection1_only,
                                 std::unordered_set<std::string> *ids_in_collection2_only,
                                 std::unordered_set<std::string> *same_ids_with_different_contents) {
    std::vector<std::string> sorted_control_numbers1;
    InitSortedControlNumbersList(control_number_to_record_hash_map1, &sorted_control_numbers1);

    std::vector<std::string> sorted_control_numbers2;
    InitSortedControlNumbersList(control_number_to_record_hash_map2, &sorted_control_numbers2);

    auto control_number1(sorted_control_numbers1.cbegin());
    auto control_number2(sorted_control_numbers2.cbegin());
    while (control_number1 != sorted_control_numbers1.cend() and control_number2 != sorted_control_numbers2.cend()) {
        if (*control_number1 == *control_number2) {
            if (control_number_to_record_hash_map1.at(*control_number1) != control_number_to_record_hash_map2.at(*control_number2)) {
                same_ids_with_different_contents->emplace(*control_number1);
            }
            ++control_number1;
            ++control_number2;
        } else if (*control_number1 < *control_number2) {
            ids_in_collection1_only->emplace(*control_number1);
            ++control_number1;
        } else {
            ids_in_collection2_only->emplace(*control_number2);
            ++control_number2;
        }
    }
}

// build a map from control number to the hash using XXH3_128bits of the record content for all records in the collection, which can be used
// for quick comparison of record contents based on control numbers.
void BuildControlNumberToHashMap(MARC::Reader * const reader,
                                 std::unordered_map<std::string, std::string> * const control_number_to_record_hash_map) {
    while (const MARC::Record record = reader->read()) {
        const std::string record_in_string(record.toString(MARC::Record::RecordFormat::MARC21_BINARY));
        // const uint64_t hash_low = XXH3_128bits(record_in_string.data(), record_in_string.size()).low64;
        // (*control_number_to_record_hash_map)[record.getControlNumber()] = std::to_string(hash_low);
        (*control_number_to_record_hash_map)[record.getControlNumber()] = std::to_string(std::hash<std::string>{}(record_in_string));
    }
}

// A generic function of writing a list of IDs to a text file, which can be used for both the list of IDs that are only in collection 1 and
// the list of IDs that are only in collection 2.
void WriteListOfIdToTextFile(const std::string &filename, const std::unordered_set<std::string> &ids) {
    const auto output_file(FileUtil::OpenOutputFileOrDie(filename));
    for (const auto &id : ids)
        (*output_file) << id << '\n';
}

// A generic function of writing records with given IDs to a Marc file. The records will be read from the given reader, which can be either
// the reader for collection 1 or the reader for collection 2.
void GetRecordAndWriteToMarcFile(const std::string &filename, const std::unordered_set<std::string> &ids, MARC::Reader * const reader) {
    auto marc_writer(MARC::Writer::Factory(filename));
    for (reader->rewind(); const MARC::Record record = reader->read();) {
        if (ids.find(record.getControlNumber()) != ids.cend())
            marc_writer->write(record);
    }
}

// Print the report to the standard output when needed.
void PrintReport(const std::unordered_set<std::string> &ids_in_collection1_only,
                 const std::unordered_set<std::string> &ids_in_collection2_only,
                 const std::unordered_set<std::string> &same_ids_with_different_contents, const std::string &collection1_name,
                 const std::string &collection2_name) {
    std::cout << ids_in_collection1_only.size() << " control number(s) are only in \"" << collection1_name << "\" but not in \""
              << collection2_name << "\".\n";
    for (const auto &control_number : ids_in_collection1_only)
        std::cout << '\t' << control_number << '\n';
    std::cout << ids_in_collection2_only.size() << " control number(s) are only in \"" << collection2_name << "\" but not in \""
              << collection1_name << "\".\n";
    for (const auto &control_number : ids_in_collection2_only)
        std::cout << '\t' << control_number << '\n';
    std::cout << same_ids_with_different_contents.size() << " control number(s) are in both collections but have different contents.\n";
    for (const auto &control_number : same_ids_with_different_contents)
        std::cout << '\t' << control_number << '\n';
}


} // namespace

int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc < 5)
        Usage();

    // build the parameters
    const std::string collection1_name(argv[1]);
    const std::string collection2_name(argv[2]);
    const std::string filename_output1(argv[3]);
    const std::string filename_output2(argv[4]);

    // We need to create the readers before collecting the control numbers and their corresponding file offsets, because
    // CollectRecordOffsets needs to read the records in the collections.
    std::unique_ptr<MARC::Reader> marc_reader1(MARC::Reader::Factory(collection1_name));
    std::unique_ptr<MARC::Reader> marc_reader2(MARC::Reader::Factory(collection2_name));

    // Variables for collecting control numbers and their corresponding file offsets for both collections.
    std::unordered_map<std::string, std::string> control_number_to_record_hash_map1;
    std::unordered_map<std::string, std::string> control_number_to_record_hash_map2;

    std::unordered_set<std::string> ids_in_collection1_only, ids_in_collection2_only, same_ids_with_different_contents;

    // Note: CollectRecordOffsets also returns the number of records in the collection, but we don't need that here.
    BuildControlNumberToHashMap(marc_reader1.get(), &control_number_to_record_hash_map1);
    BuildControlNumberToHashMap(marc_reader2.get(), &control_number_to_record_hash_map2);


    // construct the list of different IDs.
    ConstructListOfDifferentIDs(control_number_to_record_hash_map1, control_number_to_record_hash_map2, &ids_in_collection1_only,
                                &ids_in_collection2_only, &same_ids_with_different_contents);

    // construct the list of IDs need to be get from collection 2 but not in collection 1, which includes both the IDs that are only in
    // collection 2 and the IDs that are in both collections but have different contents.
    std::unordered_set<std::string> ids_in_collection2_only_or_different_content(ids_in_collection2_only);
    ids_in_collection2_only_or_different_content.insert(same_ids_with_different_contents.cbegin(), same_ids_with_different_contents.cend());

    // writing the ids that are only in collection 1 to a text file, and writing the records with IDs that are only in collection 2 or have
    // different contents to a Marc file.
    WriteListOfIdToTextFile(filename_output1, ids_in_collection1_only);
    GetRecordAndWriteToMarcFile(filename_output2, ids_in_collection2_only_or_different_content, marc_reader2.get());

    // print the report to the standard output when needed.
    if (verbose)
        PrintReport(ids_in_collection1_only, ids_in_collection2_only, same_ids_with_different_contents, collection1_name, collection2_name);

    return EXIT_SUCCESS;
}
