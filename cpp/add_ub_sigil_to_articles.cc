/** \file    add_ub_sigil_to_articles.cc
 *  \brief   A tool for adding the DE-21 sigil to articles entries if the containing work has the DE-21 entry.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include "FileUtil.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "MarcXmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] master_marc_input marc_output\n";
    std::cerr << "  Adds DE-21 sigils, as appropriate, to article entries found in the\n";
    std::cerr << "  master_marc_input and writes this augmented file as marc_output.\n\n";
    std::exit(EXIT_FAILURE);
}


// Returns true if we're dealing with a DE-21 block and we managed to extract some inventory information.
bool ExtractInventoryInfoFromDE21Block(const size_t block_start_index, const size_t block_end_index,
                                       const MarcRecord &record, std::string * const inventory_info)
{
    inventory_info->clear();
    
    bool is_de21_block(false);
    for (size_t index(block_start_index); index < block_end_index; ++index) {
        const Subfields subfields(record.getSubfields(index));
        const std::string subfield0(subfields.getFirstSubfieldValue('0'));
        if (subfield0 == "852  ") {
            if (subfields.getFirstSubfieldValue('a') == "DE-21")
                is_de21_block = true;
        } else if (subfield0 == "86630")
            *inventory_info = subfields.getFirstSubfieldValue('a');
    }

    return is_de21_block and not inventory_info->empty();
}


void CollectParentIDs(const bool verbose, MarcReader * const marc_reader,
                      std::unordered_map<std::string, std::string> * const parent_ids_and_inventory_info)
{
    if (verbose)
        std::cout << "Starting extraction of parent IDs.\n";

    unsigned count(0);
    std::string err_msg;
    while (const MarcRecord record = marc_reader->read()) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isSerial() and not leader.isMonograph())
            continue;

        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        record.findAllLocalDataBlocks(&local_block_boundaries);
        for (const auto &local_block_boundary : local_block_boundaries) {
            std::string inventory_info;
            if (ExtractInventoryInfoFromDE21Block(local_block_boundary.first, local_block_boundary.second, record,
                                                  &inventory_info))
                (*parent_ids_and_inventory_info)[record.getControlNumber()] = inventory_info;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Found " << parent_ids_and_inventory_info->size() << " relevant parent ID's.\n";
    }
}


bool IssueInInventory(const std::string &/*inventory_info*/, const MarcRecord &/*issue_record*/) {
    return true;
}


void AddMissingSigilsToArticleEntries(
    const bool verbose, MarcReader * const marc_reader, MarcWriter * const marc_writer,
    const std::unordered_map<std::string, std::string> &parent_ids_and_inventory_info)
{
    if (verbose)
        std::cout << "Starting augmentation of article entries.\n";

    unsigned count(0), ids_added(0), missing_host_record_ctrl_num_count(0),
             missing_parent_id_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;

        const Leader &leader(record.getLeader());
        if (not leader.isArticle()) {
            marc_writer->write(record);
            continue;
        }

        const size_t index_773 = record.getFieldIndex("773");
        if (index_773 == MarcRecord::FIELD_NOT_FOUND) {
            marc_writer->write(record);
            continue;
        }

        Subfields subfields(record.getSubfields(index_773));

        auto begin_end = subfields.getIterators('w'); // Record control number of Host Item Entry.
        if (begin_end.first == begin_end.second) {
            marc_writer->write(record);
            ++missing_host_record_ctrl_num_count;
            continue;
        }

        std::string host_id;
        for (auto _773w_iter(begin_end.first); _773w_iter != begin_end.second; ++_773w_iter) {
            if (StringUtil::StartsWith(_773w_iter->value_, "(DE-576)")) {
                host_id = _773w_iter->value_.substr(8);
                break;
            }
        }

        auto const parent_id_iter(parent_ids_and_inventory_info.find(host_id));
        if (parent_id_iter == parent_ids_and_inventory_info.end()) {
            marc_writer->write(record);
            ++missing_parent_id_count;
            continue;
        }

        if (IssueInInventory(parent_id_iter->second, record)) {
            record.insertField("LOK", "  ""\x1F""0852""\x1F""aDE-21");
            ++ids_added;
        }
        marc_writer->write(record);
    }

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Added ID's to " << ids_added << " article record(s).\n";
        std::cerr << missing_host_record_ctrl_num_count << " articles had missing host record control number(s).\n";
        std::cerr << "For " << missing_parent_id_count << " articles no host ISBN nor ISSN was found.\n";
    }
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if ((argc != 3 and argc != 4)
        or (argc == 4 and std::strcmp(argv[1], "-v") != 0 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 4);

    const std::string marc_input_filename(argv[argc == 3 ? 1 : 2]);
    const std::string marc_output_filename(argv[argc == 3 ? 2 : 3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Master input file name equals output file name!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    try {
        std::unordered_map<std::string, std::string> parent_ids_and_inventory_info;
        CollectParentIDs(verbose, marc_reader.get(), &parent_ids_and_inventory_info);

        marc_reader->rewind();
        AddMissingSigilsToArticleEntries(verbose, marc_reader.get(), marc_writer.get(),
                                         parent_ids_and_inventory_info);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
