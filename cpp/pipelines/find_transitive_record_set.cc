/** \file    find_transitive_record_set.cc
 *  \brief   Finds untagged records that belong in the same category and are directly or indirectly linked to via PPN's.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020 Library of the University of Tübingen

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

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_input marc_output dangling_references\n"
            "propagates tagging of CHURCHLAW, BIBLESTUDIES or RELSTUDIES records via up- and crosslinks.");
}


enum RecordType { BIBLESTUDIES, CHURCHLAW, RELSTUDIES };


typedef bool (*RecordTypeOfInterestPredicate)(const MARC::Record &record);


inline bool IsBibleStudiesRecord(const MARC::Record &record) {
    //return record.hasSubfieldWithValue("SUB", 'a', "BIB");
    return record.findTag("BIB") != record.end(); // remove after migration
}


inline bool IsChurchLawRecord(const MARC::Record &record) {
    //return record.hasSubfieldWithValue("SUB", 'a', "CAN");
    return record.findTag("CAN") != record.end(); // remove after migration
}


inline bool IsRelStudiesRecord(const MARC::Record &record) {
    //return record.hasSubfieldWithValue("SUB", 'a', "REL");
    return record.findTag("REL") != record.end(); // remove after migration
}


std::set<RecordType> GetRecordTypes(const MARC::Record &record) {
    std::set<RecordType> record_types;
    if (IsBibleStudiesRecord(record))
        record_types.emplace(BIBLESTUDIES);
    if (IsChurchLawRecord(record))
        record_types.emplace(CHURCHLAW);
    if (IsRelStudiesRecord(record))
        record_types.emplace(RELSTUDIES);

    return record_types;
}


std::set<std::string> GetReferencedPPNs(const MARC::Record &record) {
    std::set<std::string> referenced_ppns(MARC::ExtractCrossLinkPPNs(record));

    const auto parent_ppn(record.getParentControlNumber());
    if (not parent_ppn.empty())
        referenced_ppns.emplace(parent_ppn);

    return referenced_ppns;
}


struct NodeInfo {
    std::set<std::string> referenced_ppns_;
    std::set<RecordType> types_;
public:
    NodeInfo() = default;
    NodeInfo(const NodeInfo &) = default;
    NodeInfo(std::set<std::string> &&referenced_ppns, std::set<RecordType> &&types)
        : referenced_ppns_(referenced_ppns), types_(types) { }
};


void GenerateGraph(MARC::Reader * const marc_reader, std::unordered_map<std::string, NodeInfo> * const ppns_to_node_infos) {
    while (const auto record = marc_reader->read())
        (*ppns_to_node_infos)[record.getControlNumber()] = NodeInfo(GetReferencedPPNs(record), GetRecordTypes(record));
}


std::map<RecordType, RecordTypeOfInterestPredicate> record_type_to_predicate_map{
    { BIBLESTUDIES, IsBibleStudiesRecord },
    { CHURCHLAW,    IsChurchLawRecord    },
    { RELSTUDIES,   IsRelStudiesRecord   },
};


std::map<RecordType, MARC::Tag> record_type_to_tag_map{ // remove after migration
    { BIBLESTUDIES, "BIB" },
    { CHURCHLAW,    "CAN" },
    { RELSTUDIES,   "REL" },
};
std::map<RecordType, std::string> record_type_to_subfield_map{
    { BIBLESTUDIES, "BIB" },
    { CHURCHLAW,    "CAN" },
    { RELSTUDIES,   "REL" },
};

unsigned PropagateTypes(File * const dangling_references_file, std::unordered_map<std::string, NodeInfo> * const ppns_to_node_infos,
                        unsigned * const dangling_references_count)
{
    unsigned newly_tagged_count(0);

    for (auto &ppn_and_node_info : *ppns_to_node_infos) {
        if (ppn_and_node_info.second.types_.empty())
            continue; // Nothing to propagate.

        for (auto &referenced_ppn : ppn_and_node_info.second.referenced_ppns_) {
            auto referenced_ppn_and_node_info(ppns_to_node_infos->find(referenced_ppn));
            if (referenced_ppn_and_node_info == ppns_to_node_infos->end()) {
                (*dangling_references_file) << ppn_and_node_info.first << " -> " << referenced_ppn << '\n';
                ++*dangling_references_count;
                continue;
            }

            bool propagated_at_least_one_type(false);
            for (const auto type : ppn_and_node_info.second.types_) {
                if (referenced_ppn_and_node_info->second.types_.find(type) == referenced_ppn_and_node_info->second.types_.end()) {
                    referenced_ppn_and_node_info->second.types_.emplace(type);
                    propagated_at_least_one_type = true;
                }
            }
            if (propagated_at_least_one_type)
                ++newly_tagged_count;
        }
    }


    return newly_tagged_count;
}


void PatchRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                  const std::unordered_map<std::string, NodeInfo> &ppns_to_node_infos)
{
    unsigned patched_count(0);
    while (auto record = marc_reader->read()) {
        const auto ppn_and_node_info(ppns_to_node_infos.find(record.getControlNumber()));
        if (unlikely(ppn_and_node_info == ppns_to_node_infos.cend()))
            LOG_ERROR("PPN not found! This should *never* happen!");

        const auto existing_types(GetRecordTypes(record));
        bool added_at_least_one_new_type(false);
        for (const auto type : ppn_and_node_info->second.types_) {
            if (existing_types.find(type) == existing_types.cend()) {
                const auto &tag(record_type_to_tag_map[type]); // remove after migration
                /*const auto &subfield(record_type_to_subfield_map[type]);*/
                record.insertField(MARC::Tag(tag), std::vector<MARC::Subfield>{ { 'a', "1" }, { 'c', "1" } }); // remove after migration
                /*record.addSubfieldCreateFieldUnique("SUB", 'a', subfield);*/
                added_at_least_one_new_type = true;
            }
        }
        if (added_at_least_one_new_type)
            ++patched_count;

        marc_writer->write(record);
    }

    LOG_INFO("Successfully patched " + std::to_string(patched_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unordered_map<std::string, NodeInfo> ppns_to_node_infos;
    GenerateGraph(marc_reader.get(), &ppns_to_node_infos);

    const auto dangling_references_file(FileUtil::OpenOutputFileOrDie(argv[3]));
    unsigned total_tagged_count(0), dangling_references_count(0);
    for (;;) {
        const unsigned newly_tagged_count(PropagateTypes(dangling_references_file.get(), &ppns_to_node_infos, &dangling_references_count));
        if (newly_tagged_count == 0)
            break;
        total_tagged_count += newly_tagged_count;
        LOG_INFO("tagged " + std::to_string(newly_tagged_count) + " additional record(s).");
    }
    LOG_INFO("Tagged " + std::to_string(total_tagged_count) + " record(s) and found " + std::to_string(dangling_references_count)
             + " dangling references.");

    marc_reader->rewind();
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    PatchRecords(marc_reader.get(), marc_writer.get(), ppns_to_node_infos);

    return EXIT_SUCCESS;
}
