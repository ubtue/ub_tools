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

#include <iostream>
#include <stdexcept>
#include <xxhash.h>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include "FileUtil.h"
#include "MARC.h"

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

// a comparator for XXH128_hash_t, which is needed for using XXH128_hash_t as the key in a boost::bimap. We compare the low64 part first,
// and if they are equal, we compare the high64 part.
struct XXH128_hash_t_cmp {
    bool operator()(const XXH128_hash_t &hash1, const XXH128_hash_t &hash2) const noexcept {
        if (hash1.low64 != hash2.low64)
            return hash1.low64 < hash2.low64;
        else
            return hash1.high64 < hash2.high64;
    }
};

// a bimap that maps control number to the hash of the record content, which can be used for quick comparison of record contents based on
// control numbers. We use boost::bimap here because we need to search by both control number and hash value.
using StringHashBimap = boost::bimap<boost::bimaps::set_of<std::string>, boost::bimaps::set_of<XXH128_hash_t, XXH128_hash_t_cmp>>;


// construct the list of IDs that are only in collection 1 but not in collection 2, and the list of IDs that are only in collection 2 but
// not in collection 1 (including records with the same ID but different content), based on the control number to record hash map for both
// collections.
void ConstructListOfDifferentIDs(const StringHashBimap &control_number_to_record_hash_map1,
                                 const StringHashBimap &control_number_to_record_hash_map2,
                                 std::unordered_set<std::string> *ids_in_collection1_only,
                                 std::unordered_set<std::string> *ids_in_collection2_only_or_different_content) {
    // search for control numbers that are only in collection1 but not in collection2
    for (const auto &entry : control_number_to_record_hash_map1.left) {
        const std::string &control_number = entry.first;
        const auto it_in_collection2 = control_number_to_record_hash_map2.left.find(control_number);
        if (it_in_collection2 == control_number_to_record_hash_map2.left.end())
            ids_in_collection1_only->emplace(control_number);
    }

    // search for control numbers that are only in collection2 but not in collection1, which includes both the control numbers that are only
    // in collection2 and the control numbers that are in both collections but have different content (if the control number is in both
    // collections but have different content, they will have different hash values, so they will be treated as if they are only in
    // collection2).
    for (const auto &entry : control_number_to_record_hash_map2.right) {
        const XXH128_hash_t &hash_value = entry.first;
        const auto it_in_collection1 = control_number_to_record_hash_map1.right.find(hash_value);
        if (it_in_collection1 == control_number_to_record_hash_map1.right.end()) {
            ids_in_collection2_only_or_different_content->emplace(control_number_to_record_hash_map2.right.at(hash_value));
        }
    }
}

// build a map from control number to the hash using XXH3_128bits of the record content for all records in the collection, which can be used
// for quick comparison of record contents based on control numbers.
void BuildBimapStringHashMap(MARC::Reader * const reader, StringHashBimap * const control_number_to_record_hash_map) {
    while (const MARC::Record record = reader->read()) {
        const std::string record_in_string(record.toString(MARC::Record::RecordFormat::MARC21_BINARY));
        control_number_to_record_hash_map->insert(
            StringHashBimap::value_type(record.getControlNumber(), XXH3_128bits(record_in_string.data(), record_in_string.size())));
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
                 const std::unordered_set<std::string> &ids_in_collection2_only_or_different_content, const std::string &collection1_name,
                 const std::string &collection2_name) {
    std::cout << ids_in_collection1_only.size() << " control number(s) are only in \"" << collection1_name << "\" but not in \""
              << collection2_name << "\".\n";
    for (const auto &control_number : ids_in_collection1_only)
        std::cout << '\t' << control_number << '\n';
    std::cout << ids_in_collection2_only_or_different_content.size() << " control number(s) are only in \"" << collection2_name
              << "\" but not in \"" << collection1_name << "\".\n";
    for (const auto &control_number : ids_in_collection2_only_or_different_content)
        std::cout << '\t' << control_number << '\n';
}


} // namespace

int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 5)
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
    StringHashBimap control_number_to_record_hash_map1;
    StringHashBimap control_number_to_record_hash_map2;

    std::unordered_set<std::string> ids_in_collection1_only, ids_in_collection2_only_or_different_content;

    // Note: CollectRecordOffsets also returns the number of records in the collection, but we don't need that here.
    BuildBimapStringHashMap(marc_reader1.get(), &control_number_to_record_hash_map1);
    BuildBimapStringHashMap(marc_reader2.get(), &control_number_to_record_hash_map2);


    // construct the list of different IDs.
    ConstructListOfDifferentIDs(control_number_to_record_hash_map1, control_number_to_record_hash_map2, &ids_in_collection1_only,
                                &ids_in_collection2_only_or_different_content);

    // writing the ids that are only in collection 1 to a text file, and writing the records with IDs that are only in collection 2 or have
    // different contents to a Marc file.
    WriteListOfIdToTextFile(filename_output1, ids_in_collection1_only);
    GetRecordAndWriteToMarcFile(filename_output2, ids_in_collection2_only_or_different_content, marc_reader2.get());

    // print the report to the standard output when needed.
    if (verbose)
        PrintReport(ids_in_collection1_only, ids_in_collection2_only_or_different_content, collection1_name, collection2_name);

    return EXIT_SUCCESS;
}
