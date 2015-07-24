/** \file    add_issn_to_articles.cc
 *  \brief   A tool for adding missing ISSN ID's to articles entries, field 773x, in MARC data.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << "[--verbose] master_marc_input additional_marc_input marc_output\n";
    std::cerr << "  Adds host/parent/journal ISSNs to article entries found in the master_marc_input and writes\n";
    std::cerr << "  this augmented file as marc_output.  The ISSNs are extracted from serial/journal entries\n";
    std::cerr << "  found in both, master_marc_input, and, additional_marc_input.\n";
    std::exit(EXIT_FAILURE);
}


bool IsPossibleISSN(const std::string &issn_candidate) {
    static RegexMatcher *matcher(NULL);
    std::string err_msg;
    if (unlikely(matcher == NULL)) {
        matcher = RegexMatcher::RegexMatcherFactory("\\d{4}\\-\\d{3}[\\dX]", &err_msg);
        if (matcher == NULL)
            Error(err_msg);
    }

    const bool is_possible_issn(matcher->matched(issn_candidate, &err_msg));
    if (unlikely(not err_msg.empty()))
        Error(err_msg);

    return is_possible_issn;
}


void PopulateParentIdToISSNMap(const bool verbose, FILE * const input,
                               std::unordered_map<std::string, std::string> * const parent_id_to_issn_map)
{
    if (verbose)
        std::cout << "Starting extraction of ISSNs.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), extracted_issn_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++count;

        std::unique_ptr<Leader> leader(raw_leader);
        if (not leader->isSerial())
            continue;

        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

        auto const entry_iterator(DirectoryEntry::FindField("022", dir_entries));
        if (entry_iterator == dir_entries.end())
            continue;

        const Subfields subfields(field_data[entry_iterator - dir_entries.begin()]);
        auto begin_end = subfields.getIterators('a'); // ISSN
        std::string issn;
        if (begin_end.first != begin_end.second) {
            (*parent_id_to_issn_map)[field_data[0]] = begin_end.first->second;
            ++extracted_issn_count;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Extracted " << extracted_issn_count << " ISSNs.\n";
    }
}


void AddMissingISSNsToArticleEntries(const bool verbose, FILE * const input, FILE * const output,
                                     const std::unordered_map<std::string, std::string> &parent_id_to_issn_map)
{
    if (verbose)
        std::cout << "Starting augmentation of article entries.\n";

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    unsigned count(0), modified_count(0), missing_host_record_ctrl_num_count(0), missing_issn_count(0);
    std::string err_msg;
    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
        ++count;
        std::unique_ptr<Leader> leader(raw_leader);
        if (not leader->isArticle()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        if (dir_entries[0].getTag() != "001")
            Error("First field is not \"001\"!");

        auto entry_iterator(DirectoryEntry::FindField("773", dir_entries));
        if (entry_iterator == dir_entries.end()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        const size_t index_773(entry_iterator - dir_entries.begin());
        Subfields subfields(field_data[index_773]);
        if (subfields.hasSubfield('x')) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            continue;
        }

        auto begin_end = subfields.getIterators('w'); // Record control number of Host Item Entry.
        if (begin_end.first == begin_end.second) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            ++missing_host_record_ctrl_num_count;
            continue;
        }

        std::string host_id(begin_end.first->second);
        if (StringUtil::StartsWith(host_id, "(DE-576)"))
            host_id = host_id.substr(8);
        auto const parent_issn_iter(parent_id_to_issn_map.find(host_id));
        if (parent_issn_iter == parent_id_to_issn_map.end()) {
            MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
            ++missing_issn_count;
            continue;
        }

        subfields.addSubfield('x', parent_issn_iter->second);
        const size_t old_773_field_length(field_data[index_773].size());
        field_data[index_773] = subfields.toString();
        const size_t new_773_field_length(field_data[index_773].size());

        //
        // Patch up all directory entries starting with the one with index "index_773".
        //

        const size_t offset(new_773_field_length - old_773_field_length);
        dir_entries[index_773].setFieldLength(dir_entries[index_773].getFieldLength() + offset);
        for (auto dir_entry_iter(dir_entries.begin() + index_773); dir_entry_iter != dir_entries.end();
             ++dir_entry_iter)
            dir_entry_iter->setFieldOffset(dir_entry_iter->getFieldOffset() + offset);

        MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
        ++modified_count;
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Modified " << modified_count << " article record(s).\n";
        std::cerr << missing_host_record_ctrl_num_count << " articles had missing host record control number(s).\n";
        std::cerr << "For " << missing_issn_count << " articles no host ISSN was found.\n";
    }
}


int main(int argc, char **argv) {
    progname = argv[0];

    if ((argc != 4 and argc != 5) or (argc == 5 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 5);

    const std::string marc_input_filename(argv[argc == 4 ? 1 : 2]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rm");
    if (marc_input == NULL)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_aux_input_filename(argv[argc == 4 ? 2 : 3]);
    FILE *marc_aux_input = std::fopen(marc_aux_input_filename.c_str(), "rm");
    if (marc_input == NULL)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[argc == 4 ? 3 : 4]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");

    if (unlikely(marc_aux_input_filename == marc_output_filename))
        Error("Auxiallary input file name equals output file name!");

    std::unordered_map<std::string, std::string> parent_id_to_issn_map;
    PopulateParentIdToISSNMap(verbose, marc_input, &parent_id_to_issn_map);
    PopulateParentIdToISSNMap(verbose, marc_aux_input, &parent_id_to_issn_map);

    std::rewind(marc_input);
    AddMissingISSNsToArticleEntries(verbose, marc_input, marc_output, parent_id_to_issn_map);

    std::fclose(marc_input);
    std::fclose(marc_aux_input);
    std::fclose(marc_output);
}
