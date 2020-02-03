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
    ::Usage("[--patch] types marc_filename (untagged_ppn_list | marc_output)\n"
            "where \"types\" must be a list of CHURCHLAW, BIBLESTUDIES or RELSTUDIES using the vertical bar as a separator.\n"
            "Please note that if \"--patch\" has been specified, the last argument is the output MARC file o/w it is a list "
            "of untagged PPNs.");
}


typedef bool (*RecordTypeOfInterestPredicate)(const MARC::Record &record);


bool IsBibleStudiesRecord(const MARC::Record &record) {
    return record.findTag("BIB") != record.end();
}


bool IsChurchLawRecord(const MARC::Record &record) {
    return record.findTag("CAN") != record.end();
}


bool IsRelStudiesRecord(const MARC::Record &record) {
    return record.findTag("REL") != record.end();
}


enum RecordType { BIBLESTUDIES, CHURCHLAW, RELSTUDIES };


std::map<RecordType, RecordTypeOfInterestPredicate> record_type_to_predicate_map{
    { BIBLESTUDIES, IsBibleStudiesRecord },
    { CHURCHLAW,    IsChurchLawRecord    },
    { RELSTUDIES,   IsRelStudiesRecord   },
};


std::map<RecordType, MARC::Tag> record_type_to_tag_map{
    { BIBLESTUDIES, "BIB" },
    { CHURCHLAW,    "CAN" },
    { RELSTUDIES,   "REL" },
};


void FindUntaggedPPNs(MARC::Reader * const marc_reader, File * const list_file,
                      std::map<RecordType, std::unordered_set<std::string>> * const record_types_to_unpatched_ppns_map)
{
    std::unordered_map<std::string, std::set<std::string>> referee_to_referenced_ppns_map;
    while (const auto record = marc_reader->read()) {
        bool need_references(false);
        for (const auto &record_type_and_unpatched_ppns : *record_types_to_unpatched_ppns_map) {
            if (record_type_to_predicate_map[record_type_and_unpatched_ppns.first](record)) {
                need_references = true;
                break;
            }
        }
        if (not need_references)
            continue;

        std::set<std::string> referenced_ppns;

        const auto parent_ppn(MARC::GetParentPPN(record));
        if (not parent_ppn.empty())
            referenced_ppns.emplace(parent_ppn);

        const auto cross_link_ppns(MARC::ExtractPrintAndOnlineCrossLinkPPNs(record));
        for (const auto &cross_link_ppn : cross_link_ppns)
            referenced_ppns.emplace(cross_link_ppn);

        if (not referenced_ppns.empty()) {
            for (auto &record_type_and_unpatched_ppns : *record_types_to_unpatched_ppns_map) {
                record_type_and_unpatched_ppns.second.insert(referenced_ppns.cbegin(), referenced_ppns.cend());
                if (list_file != nullptr) {
                    for (const auto &referenced_ppn : referenced_ppns)
                        (*list_file) << record_type_to_tag_map[record_type_and_unpatched_ppns.first].c_str() << ' '
                                     << record.getControlNumber() << referenced_ppn << '\n';
                }
            }
        }
    }

    size_t untagged_references_count(0);
    for (const auto &record_type_and_unpatched_ppns : *record_types_to_unpatched_ppns_map)
        untagged_references_count += record_type_and_unpatched_ppns.second.size();
    LOG_INFO("Found " + std::to_string(untagged_references_count) + " referenced but untagged record(s).");
}


void PatchRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                  const std::map<RecordType, std::unordered_set<std::string>> &record_types_to_unpatched_ppns_map)
{
    unsigned patched_count(0);
    while (auto record = marc_reader->read()) {
        bool added_at_least_one_field(false);
        for (const auto &record_type_and_ppns : record_types_to_unpatched_ppns_map) {
            if (record_type_and_ppns.second.find(record.getControlNumber()) != record_type_and_ppns.second.cend()) {
                const auto &tag(record_type_to_tag_map[record_type_and_ppns.first]);
                record.insertField(MARC::Tag(tag), std::vector<MARC::Subfield>{ { 'a', "1" }, { 'c', "1" } });
                added_at_least_one_field = true;
            }
        }
        if (added_at_least_one_field)
            ++patched_count;

        marc_writer->write(record);
    }

    LOG_INFO("Successfully patched " + std::to_string(patched_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4 and argc != 5)
        Usage();

    bool patch(false);
    if (argc == 5) {
        if (std::strcmp(argv[1], "--patch") != 0)
            Usage();
        patch = true;
        --argc, ++argv;
    }

    std::vector<std::string> record_type_strings;
    StringUtil::Split(std::string(argv[1]), '|', &record_type_strings);
    std::set<RecordType> record_types;
    for (const auto &record_type_string : record_type_strings) {
        if (record_type_string == "CHURCHLAW")
            record_types.emplace(CHURCHLAW);
        else if (record_type_string == "RELSTUDIES")
            record_types.emplace(RELSTUDIES);
        else if (record_type_string == "BIBLESTUDIES")
            record_types.emplace(BIBLESTUDIES);
        else
            LOG_ERROR("\"" + record_type_string + "\" is not a valid type!");
    }
    if (record_types.empty())
        LOG_ERROR("You must specify at least one record type!");

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto list_file(patch ? nullptr : FileUtil::OpenOutputFileOrDie(argv[3]));

    std::map<RecordType, std::unordered_set<std::string>> record_types_to_unpatched_ppns_map;
    for (const auto &record_type : record_types)
        record_types_to_unpatched_ppns_map[record_type] = std::unordered_set<std::string>();

    FindUntaggedPPNs(marc_reader.get(), list_file.get(), &record_types_to_unpatched_ppns_map);

    if (patch) {
        marc_reader->rewind();
        const auto marc_writer(MARC::Writer::Factory(argv[3]));
        PatchRecords(marc_reader.get(), marc_writer.get(), record_types_to_unpatched_ppns_map);
    }

    return EXIT_SUCCESS;
}
