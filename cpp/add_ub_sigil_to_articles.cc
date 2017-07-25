/** \file
 *  \author Johannes Riedl
 *
 *  New implementation to derive information for articles about being available in Tübingen
 *  from superior works and augment LOK data appropriately
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

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

#include <algorithm>
#include <iostream>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "TextUtil.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"


static unsigned extracted_count(0);
static unsigned modified_count(0);
static std::unordered_set<std::string> de21_superior_ppns;
static const RegexMatcher * const tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));
static const RegexMatcher * const superior_ppn_matcher(RegexMatcher::RegexMatcherFactory(".DE-576.(.*)"));


void Usage() {
    std::cerr << "Usage: " << ::progname << " spr_augmented_marc_input marc_output\n";
    std::cerr << "  Adds DE-21 sigils, as appropriate, to article entries found in the\n";
    std::cerr << "  master_marc_input and writes this augmented file as marc_output.\n\n";
    std::cerr << "  Notice that this program requires the SPR tag for superior works\n";
    std::cerr << "  to be set for appropriate results";
    std::exit(EXIT_FAILURE);
}


void ProcessSuperiorRecord(const MarcRecord &record) {
    // We are done if this is not a superior work
    if (record.getFieldData("SPR").empty())
        return;

    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record.findAllLocalDataBlocks(&local_block_boundaries);
    if (local_data_count == 0)
        return;

    for (const auto &block_start_and_end : local_block_boundaries) {
        std::vector<size_t> field_indices;
        record.findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);
    
        for (size_t field_index : field_indices) {
            const std::string field_data(record.getFieldData(field_index));
            const Subfields subfields(field_data);
            std::string sigil;
            if (subfields.extractSubfieldWithPattern('a', *tue_sigil_matcher, &sigil)) {
                de21_superior_ppns.insert(record.getControlNumber());
                ++extracted_count;
            }
        }
    }
}


void LoadDE21PPNs(MarcReader * const marc_reader) {
    while (const MarcRecord record = marc_reader->read())
         ProcessSuperiorRecord(record);
}


void CollectSuperiorPPNs(const MarcRecord &record, std::unordered_set<std::string> * const superior_ppn_set) {
    // Determine superior PPNs from 800w:810w:830w:773w:776w
    const std::vector<std::string> tags({ "800", "810", "830", "773", "776" });
    for (const auto &tag : tags) {
        std::vector<std::string> superior_ppn_vector;
        record.extractSubfields(tag , "w", &superior_ppn_vector);

        // Remove superfluous prefixes
        for (auto &superior_ppn : superior_ppn_vector) {
             std::string err_msg;
             if (not superior_ppn_matcher->matched(superior_ppn, &err_msg)) {
                 if (not err_msg.empty())
                     Error("Error with regex für superior works " + err_msg);
                 continue;
             }
             superior_ppn = (*superior_ppn_matcher)[1];
        }
        std::copy(superior_ppn_vector.begin(), superior_ppn_vector.end(), std::inserter(*superior_ppn_set, superior_ppn_set->end()));
    }
}


void InsertDE21ToLOK852(MarcRecord * const record) {
     record->insertField("LOK", "  ""\x1F""0852""\x1F""aDE-21");
     ++modified_count;
}


bool AlreadyHasLOK852DE21(const MarcRecord &record) {
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record.findAllLocalDataBlocks(&local_block_boundaries);
    if (local_data_count == 0)
        return false;

    for (const auto &block_start_and_end : local_block_boundaries) {
        std::vector<size_t> field_indices;
        record.findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);
        for (size_t field_index : field_indices) {
            const std::string field_data(record.getFieldData(field_index));
            const Subfields subfields(field_data);
            std::string sigil;
            if (subfields.extractSubfieldWithPattern('a', *tue_sigil_matcher, &sigil)) {
                return true;
            }
        }
    }
    return false;
}


void ProcessRecord(MarcRecord * const record, MarcWriter * const marc_writer) {
    const Leader &leader(record->getLeader());
    if (not leader.isArticle()) {
        marc_writer->write(*record);
        return;
    }

    if (AlreadyHasLOK852DE21(*record)) {
        marc_writer->write(*record);
        return;
    }
    std::unordered_set<std::string> superior_ppn_set;
    CollectSuperiorPPNs(*record, &superior_ppn_set);
    // Do we have superior PPN that has DE-21
    for (const auto &superior_ppn : superior_ppn_set) {
        if (de21_superior_ppns.find(superior_ppn) != de21_superior_ppns.end()) {
            InsertDE21ToLOK852(record);
            marc_writer->write(*record);
            return;
        }
    }
}


void AugmentRecords(MarcReader * const marc_reader, MarcWriter * const marc_writer) {
    marc_reader->rewind();
    while (MarcRecord record = marc_reader->read())
        ProcessRecord(&record, marc_writer);
    std::cerr << "Extracted " << extracted_count << " superior PPNs with DE-21 and modified " << modified_count << " records\n"; 
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));
    const std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2]));

    try {
        LoadDE21PPNs(marc_reader.get());
        AugmentRecords(marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
