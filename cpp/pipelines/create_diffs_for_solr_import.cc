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
#include <thread>
#include <xxhash.h>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include "FileUtil.h"
#include "MARC.h"

namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] previous_marc_collection current_marc_collection list_of_ids_to_delete list_of_record_to_import\n"
              << "\t[--extract]\tprint more info.\n"
              << "\t-previous_marc_collection\tfirst MARC collection.\n"
              << "\t-current_marc_collection\tsecond MARC collection.\n"
              << "\t-list_of_ids_to_delete\twill be a text file that lists all the IDs from the previous collection that are not found in "
                 "the current collection.\n"
              << "\t-list_of_record_to_import\twill be a Marc file with all records that are found in the current collection but not in "
                 "the previous collection, as "
                 "well "
                 "as records that have the same ID in both collections but different content.\n";
    std::exit(EXIT_FAILURE);
}

// a comparator for XXH128_hash_t, which is needed for using XXH128_hash_t as the key in a boost::bimap. We compare the low64 part first,
// and if they are equal, we compare the high64 part.
struct XXH128_hash_t_cmp {
    bool operator()(const XXH128_hash_t &hash1, const XXH128_hash_t &hash2) const noexcept {
        if (hash1.high64 < hash2.high64)
            return true;
        if (hash1.high64 > hash2.high64)
            return false;
        return hash1.low64 < hash2.low64;
    }
};

// a bimap that maps control number to the hash of the record content, which can be used for quick comparison of record contents based on
// control numbers. We use boost::bimap here because we need to search by both control number and hash value.
using PPNRecordHashBimap = boost::bimap<boost::bimaps::set_of<std::string>, boost::bimaps::set_of<XXH128_hash_t, XXH128_hash_t_cmp>>;


void CollectIdsToBeDeleted(const PPNRecordHashBimap &previous_ppn_to_record_hash_map,
                           const PPNRecordHashBimap &current_ppn_to_record_hash_map,
                           std::unordered_set<std::string> *ids_need_to_be_deleted) {
    for (const auto &[ppn, record_hash] : previous_ppn_to_record_hash_map.left) {
        if (current_ppn_to_record_hash_map.left.find(ppn) == current_ppn_to_record_hash_map.left.end())
            ids_need_to_be_deleted->emplace(ppn);
    }
}

void CollectIdsToBeImported(const PPNRecordHashBimap &previous_ppn_to_record_hash_map,
                            const PPNRecordHashBimap &current_ppn_to_record_hash_map,
                            std::unordered_set<std::string> *ids_need_to_be_imported) {
    for (const auto &[ppn, record_hash] : current_ppn_to_record_hash_map.right) {
        if (previous_ppn_to_record_hash_map.right.find(record_hash) == previous_ppn_to_record_hash_map.right.end())
            ids_need_to_be_imported->emplace(ppn);
    }
}

// build a map from control number to the hash using XXH3_128bits of the record content for all records in the collection, which can be
// used for quick comparison of record contents based on control numbers.
void BuildBimapStringHashMap(const std::string &marc_file_name, PPNRecordHashBimap * const control_number_to_record_hash_map) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory(marc_file_name));
    while (const MARC::Record record = reader->read()) {
        const std::string record_in_string(record.toString(MARC::Record::RecordFormat::MARC21_BINARY));
        control_number_to_record_hash_map->insert(
            PPNRecordHashBimap::value_type(record.getControlNumber(), XXH3_128bits(record_in_string.data(), record_in_string.size())));
    }
}

// A generic function of writing a list of IDs to a text file, which can be used for both the list of IDs that are only in collection 1
// and the list of IDs that are only in collection 2.
void WriteListOfIdToTextFile(const std::string &filename, const std::unordered_set<std::string> &ids) {
    const auto output_file(FileUtil::OpenOutputFileOrDie(filename));
    for (const auto &id : ids)
        (*output_file) << id << '\n';
}

// A generic function of writing records with given IDs to a Marc file. The records will be read from the given reader, which can be
// either the reader for collection 1 or the reader for collection 2.
void GetRecordAndWriteToMarcFile(const std::string &filename, const std::unordered_set<std::string> &ids, MARC::Reader * const reader) {
    auto marc_writer(MARC::Writer::Factory(filename));
    for (reader->rewind(); const MARC::Record record = reader->read();) {
        if (ids.find(record.getControlNumber()) != ids.cend())
            marc_writer->write(record);
    }
}

// Print the report to the standard output when needed.
void PrintReport(const std::unordered_set<std::string> &ids_need_to_be_deleted,
                 const std::unordered_set<std::string> &ids_need_to_be_imported, const std::string &previous_marc_file_name,
                 const std::string &current_marc_file_name) {
    std::cout << ids_need_to_be_deleted.size() << " control number(s) are only in \"" << previous_marc_file_name << "\" but not in \""
              << current_marc_file_name << "\".\n";
    for (const auto &control_number : ids_need_to_be_deleted)
        std::cout << '\t' << control_number << '\n';
    std::cout << ids_need_to_be_imported.size() << " control number(s) are only in \"" << current_marc_file_name << "\" but not in \""
              << previous_marc_file_name << "\".\n";
    for (const auto &control_number : ids_need_to_be_imported)
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
    const std::string previous_marc_file_name(argv[1]);
    const std::string current_marc_file_name(argv[2]);
    const std::string list_of_ids_to_delete(argv[3]);
    const std::string list_of_record_to_import(argv[4]);


    std::unique_ptr<MARC::Reader> marc_reader_of_current_file(MARC::Reader::Factory(current_marc_file_name));

    // Variables for collecting control numbers and their corresponding file offsets for both collections.
    PPNRecordHashBimap previous_ppn_to_record_hash_map;
    PPNRecordHashBimap current_ppn_to_record_hash_map;

    std::unordered_set<std::string> ids_need_to_be_deleted, ids_need_to_be_imported;

    // build the bimap for both collections and collect the IDs that need to be deleted and imported in parallel using multiple threads,
    // which can speed up the process significantly when the collections are large. We can do this because building the bimap and collecting
    // the IDs are independent tasks that do not need to be done sequentially. We can also use multiple threads to build the bimap for each
    // collection in parallel, which can further speed up the process. We can use std::thread for this purpose, which is a simple and
    // efficient way to create and manage threads in C++. We can also use std::async and std::future for a more high-level approach, but
    // std::thread is sufficient for our needs here. We can also use a thread pool if we want to limit the number of threads and reuse them
    // for multiple tasks, but since we only have a few tasks here, we can just create and join the threads directly without the need for a
    // thread pool.
    std::thread thread_build_previous_bimap(BuildBimapStringHashMap, previous_marc_file_name, &previous_ppn_to_record_hash_map);
    std::thread thread_build_current_bimap(BuildBimapStringHashMap, current_marc_file_name, &current_ppn_to_record_hash_map);
    thread_build_previous_bimap.join();
    thread_build_current_bimap.join();

    // collect the IDs that need to be deleted and imported in parallel using multiple threads, which can speed up the process significantly
    // when the collections are large. We can do this because collecting the IDs that need to be deleted and imported are independent tasks
    // that do not need to be done sequentially. We can also use multiple threads to collect the IDs that need to be deleted and imported in
    // parallel, which can further speed up the process. We can use std::thread for this purpose, which is a simple and efficient way to
    // create and manage threads in C++. We can also use std::async and std::future for a more high-level approach, but std::thread is
    // sufficient for our needs here. We can also use a thread pool if we want to limit the number of threads and reuse them for multiple
    // tasks, but since we only have a few tasks here, we can just create and join the threads directly without the need for a thread pool.
    std::thread thread_collect_ids_to_be_deleted(CollectIdsToBeDeleted, previous_ppn_to_record_hash_map, current_ppn_to_record_hash_map,
                                                 &ids_need_to_be_deleted);
    std::thread thread_collect_ids_to_be_imported(CollectIdsToBeImported, previous_ppn_to_record_hash_map, current_ppn_to_record_hash_map,
                                                  &ids_need_to_be_imported);
    thread_collect_ids_to_be_deleted.join();
    thread_collect_ids_to_be_imported.join();

    // write the IDs in ids_need_to_be_deleted to the list_of_ids_to_delete file.
    WriteListOfIdToTextFile(list_of_ids_to_delete, ids_need_to_be_deleted);
    // write the records with the IDs in ids_need_to_be_imported to the list_of_record_to_import file. The records will be read from the
    // current collection, which is represented by marc_reader_of_current_file. We can do this in a single pass through the current
    // collection, which can be efficient even when the collection is large, because we only need to check if the control number of each
    // record is in ids_need_to_be_imported, which can be done in constant time using an unordered_set. We can also do this in parallel
    // using multiple threads, but since we only need to read the current collection once, it may not be worth the overhead of creating and
    // managing multiple threads for this task. We can just do it in a single thread for simplicity and efficiency.
    GetRecordAndWriteToMarcFile(list_of_record_to_import, ids_need_to_be_imported, marc_reader_of_current_file.get());


    // print the report to the standard output when needed.
    if (verbose)
        PrintReport(ids_need_to_be_deleted, ids_need_to_be_imported, previous_marc_file_name, current_marc_file_name);

    return EXIT_SUCCESS;
}
