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
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] [--extract] marc_collection1 marc_collection2 [filename_output1] [filename_output2]\n"
              << "\t --extract\textracting the difference to filename_output1 and filename_output2\n"
              << "\t\t\t-filename_ouput1 contains all ids exist in collection1 but not in collection2.\n"
              << "\t\t\t-filename_output2 contains all records that not exist in collection1 but exist in collection2\n"
              << "\t\t\t\tor the record has the same id but different content."
              << "\n";
    std::exit(EXIT_FAILURE);
}


// Helper function for RecordsDiffer.
std::vector<std::string> ExtractRepeatedContents(MARC::Record::const_iterator &field, const MARC::Record::const_iterator &end) {
    const MARC::Tag repeated_tag(field->getTag());
    std::vector<std::string> contents;
    while (field != end and field->getTag() == repeated_tag) {
        contents.emplace_back(field->getContents());
        ++field;
    }

    return contents;
}


bool RecordsDiffer(const MARC::Record &record1, const MARC::Record &record2, std::string * const difference,
                   std::unordered_set<std::string> * const ppns_difference_content) {
    auto field1(record1.begin());
    auto field2(record2.begin());

    while (field1 != record1.end() and field2 != record2.end()) {
        if (unlikely(field1->getTag() != field2->getTag())) {
            *difference = field1->getTag().toString() + ", " + field2->getTag().toString();
            ppns_difference_content->emplace(record1.getControlNumber());
            return true;
        }

        // Handle repeated fields:
        const MARC::Tag common_tag(field1->getTag());
        if ((field1 + 1) == record1.end() or (field2 + 1) == record2.end() or (field1 + 1)->getTag() != common_tag
            or (field2 + 1)->getTag() != common_tag)
        {
            if (field1->getContents() != field2->getContents()) {
                *difference = common_tag.toString() + ", " + common_tag.toString();
                ppns_difference_content->emplace(record1.getControlNumber());
                return true;
            }
            ++field1, ++field2;
        } else { // We have a repeated field.
            std::vector<std::string> contents1(ExtractRepeatedContents(field1, record1.end()));
            std::vector<std::string> contents2(ExtractRepeatedContents(field2, record2.end()));
            if (contents1.size() != contents2.size()) {
                *difference = common_tag.toString() + ", " + common_tag.toString();
                return true;
            }

            // Compare the sorted contents of the two records for the current tag:
            std::sort(contents1.begin(), contents1.end());
            std::sort(contents2.begin(), contents2.end());
            auto content1(contents1.cbegin());
            auto content2(contents2.cbegin());
            while (content1 != contents1.cend()) {
                if (*content1 != *content2) {
                    *difference = common_tag.toString() + ", " + common_tag.toString();
                    ppns_difference_content->emplace(record1.getControlNumber());
                    return true;
                }
                ++content1, ++content2;
            }
        }
    }

    if (field1 != record1.end()) {
        *difference = field1->getTag().toString() + ", END";
        ppns_difference_content->emplace(record1.getControlNumber());
        return true;
    } else if (field2 != record2.end()) {
        *difference = "END, " + field2->getTag().toString();
        ppns_difference_content->emplace(record1.getControlNumber());
        return true;
    }

    return false;
}


void EmitDifferenceReport(const bool verbose, const std::unordered_map<std::string, off_t> &control_number_to_offset_map1,
                          const std::unordered_map<std::string, off_t> &control_number_to_offset_map2, MARC::Reader * const reader1,
                          MARC::Reader * const reader2, std::unordered_set<std::string> * const ppns_new_or_to_be_updated) {
    if (verbose)
        std::cout << "Records w/ identical control numbers but differing contents:\n";

    unsigned differ_count(0);
    for (const auto &control_number_and_offset1 : control_number_to_offset_map1) {
        const auto control_number_and_offset2(control_number_to_offset_map2.find(control_number_and_offset1.first));
        if (control_number_and_offset2 == control_number_to_offset_map2.cend())
            continue; // Control number is only in collection 1.

        if (unlikely(not reader1->seek(control_number_and_offset1.second)))
            LOG_ERROR("seek in collection 1 failed!");
        const MARC::Record record1(reader1->read());

        if (unlikely(not reader2->seek(control_number_and_offset2->second)))
            LOG_ERROR("seek in collection 2 failed!");
        const MARC::Record record2(reader2->read());

        std::string difference;
        if (RecordsDiffer(record1, record2, &difference, ppns_new_or_to_be_updated)) {
            ++differ_count;
            if (verbose)
                std::cout << '\t' << record1.getControlNumber() << " (fields: " << difference << ")\n";
        }
    }

    std::cout << differ_count << " record(s) have identical control numbers but different contents.\n";
}


inline void InitSortedControlNumbersList(const std::unordered_map<std::string, off_t> &control_number_to_offset_map,
                                         std::vector<std::string> * const sorted_control_numbers) {
    sorted_control_numbers->clear();
    sorted_control_numbers->reserve(control_number_to_offset_map.size());
    for (const auto &control_number_and_offset : control_number_to_offset_map)
        sorted_control_numbers->emplace_back(control_number_and_offset.first);
    std::sort(sorted_control_numbers->begin(), sorted_control_numbers->end());
}


void EmitStandardReport(const bool verbose, const std::string &collection1_name, const std::string &collection2_name,
                        const unsigned collection1_size, const unsigned collection2_size,
                        const std::unordered_map<std::string, off_t> &control_number_to_offset_map1,
                        const std::unordered_map<std::string, off_t> &control_number_to_offset_map2,
                        std::unordered_set<std::string> *in_map1_only, std::unordered_set<std::string> *in_map2_only) {
    std::vector<std::string> sorted_control_numbers1;
    InitSortedControlNumbersList(control_number_to_offset_map1, &sorted_control_numbers1);

    std::vector<std::string> sorted_control_numbers2;
    InitSortedControlNumbersList(control_number_to_offset_map2, &sorted_control_numbers2);

    // std::unordered_set<std::string> in_map1_only, in_map2_only;

    auto control_number1(sorted_control_numbers1.cbegin());
    auto control_number2(sorted_control_numbers2.cbegin());
    while (control_number1 != sorted_control_numbers1.cend() and control_number2 != sorted_control_numbers2.cend()) {
        if (*control_number1 == *control_number2) {
            ++control_number1;
            ++control_number2;
        } else if (*control_number1 < *control_number2) {
            in_map1_only->emplace(*control_number1);
            ++control_number1;
        } else { // *control_number2 < *control_number1
            in_map2_only->emplace(*control_number2);
            ++control_number2;
        }
    }

    const unsigned in_map1_only_count(in_map1_only->size() + (sorted_control_numbers1.cend() - control_number1));
    const unsigned in_map2_only_count(in_map2_only->size() + (sorted_control_numbers2.cend() - control_number2));

    std::cout << '"' << collection1_name << "\" contains " << collection1_size << " record(s).\n";
    std::cout << '"' << collection2_name << "\" contains " << collection2_size << " record(s).\n";
    std::cout << in_map1_only_count << " control number(s) are only in \"" << collection1_name << "\" but not in \"" << collection2_name
              << "\".\n";
    if (verbose) {
        for (const auto &control_number : *in_map1_only)
            std::cout << '\t' << control_number << '\n';
    }
    std::cout << in_map2_only_count << " control number(s) are only in \"" << collection2_name << "\" but not in \"" << collection1_name
              << "\".\n";
    if (verbose) {
        for (const auto &control_number : *in_map2_only)
            std::cout << '\t' << control_number << '\n';
    }
    std::cout << (collection1_size - in_map1_only_count) << " are in both collections.\n";
}

void WriteToOutput1(const std::string filename, const std::unordered_set<std::string> diff_in_collection1) {
    // open the output file
    const auto output_file(FileUtil::OpenOutputFileOrDie(filename));

    for (const auto &record_id : diff_in_collection1) {
        (*output_file) << record_id << '\n';
    }
}


void WriteToOutput2(const std::string &filename, const std::unordered_set<std::string> diff_in_collection2, MARC::Reader * const reader) {
    auto marc_writer(MARC::Writer::Factory(filename));
    for (reader->rewind(); const MARC::Record record = reader->read();) {
        if (diff_in_collection2.find(record.getControlNumber()) != diff_in_collection2.cend())
            marc_writer->write(record);
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    const bool extract(std::strcmp(argv[1], "--extract") == 0);
    std::string filename_output1, filename_output2;

    if (extract) {
        if (argc != 6)
            Usage();

        --argc, ++argv;
        filename_output1 = argv[3];
        filename_output2 = argv[4];
    } else if (argc != 3)
        Usage();

    const std::string collection1_name(argv[1]);
    const std::string collection2_name(argv[2]);
    std::unique_ptr<MARC::Reader> marc_reader1(MARC::Reader::Factory(collection1_name));
    std::unique_ptr<MARC::Reader> marc_reader2(MARC::Reader::Factory(collection2_name));

    std::unordered_map<std::string, off_t> control_number_to_offset_map1;
    const size_t collection1_size(MARC::CollectRecordOffsets(marc_reader1.get(), &control_number_to_offset_map1));
    const std::string marc_output_filename = "test.mrc";
    std::unordered_set<std::string> ppns_have_to_delete, ppns_new_or_to_be_updated;
    auto marc_writer(MARC::Writer::Factory(marc_output_filename));

    std::unordered_map<std::string, off_t> control_number_to_offset_map2;
    const unsigned collection2_size(MARC::CollectRecordOffsets(marc_reader2.get(), &control_number_to_offset_map2));

    EmitDifferenceReport(verbose, control_number_to_offset_map1, control_number_to_offset_map2, marc_reader1.get(), marc_reader2.get(),
                         &ppns_new_or_to_be_updated);

    EmitStandardReport(verbose, collection1_name, collection2_name, collection1_size, collection2_size, control_number_to_offset_map1,
                       control_number_to_offset_map2, &ppns_have_to_delete, &ppns_new_or_to_be_updated);

    if (extract) {
        WriteToOutput1(filename_output1, ppns_have_to_delete);
        WriteToOutput2(filename_output2, ppns_new_or_to_be_updated, marc_reader2.get());
    }

    return EXIT_SUCCESS;
}
