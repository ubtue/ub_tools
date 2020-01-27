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


void FindUntaggedPPNs(MARC::Reader * const marc_reader, File * const list_file,
                      const RecordTypeOfInterestPredicate is_record_type_of_interest)
{
    std::unordered_set<std::string> tagged_ppns;
    std::unordered_map<std::string, std::set<std::string>> referee_to_referenced_ppns_map;
    while (const auto record = marc_reader->read()) {
        if (not (*is_record_type_of_interest)(record))
            continue;

        tagged_ppns.emplace(record.getControlNumber());
        const auto parent_ppn(MARC::GetParentPPN(record));
        if (not parent_ppn.empty()) {
            auto referee_and_referenced_ppns(referee_to_referenced_ppns_map.find(record.getControlNumber()));
            if (referee_and_referenced_ppns != referee_to_referenced_ppns_map.end())
                referee_and_referenced_ppns->second.emplace(parent_ppn);
            else
                referee_to_referenced_ppns_map[record.getControlNumber()] = std::set<std::string>{ parent_ppn };
        }

        const auto cross_link_ppns(MARC::ExtractPrintAndOnlineCrossLinkPPNs(record));
        for (const auto &cross_link_ppn : cross_link_ppns) {
            auto referee_and_referenced_ppns(referee_to_referenced_ppns_map.find(record.getControlNumber()));
            if (referee_and_referenced_ppns != referee_to_referenced_ppns_map.end())
                referee_and_referenced_ppns->second.emplace(cross_link_ppn);
            else
                referee_to_referenced_ppns_map[record.getControlNumber()] = std::set<std::string>{ cross_link_ppn };
        }
    }

    unsigned untagged_count(0);
    for (const auto &referee_and_referenced_ppns : referee_to_referenced_ppns_map) {
        for (const auto &referenced_ppn : referee_and_referenced_ppns.second) {
            if (tagged_ppns.find(referenced_ppn) == tagged_ppns.cend()) {
                ++untagged_count;
                (*list_file) << referee_and_referenced_ppns.first << " -> " << referenced_ppn << '\n';
            }
        }
    }

    LOG_INFO("Found " + std::to_string(untagged_count) + " referenced but untagged record(s).");
}


    enum RecordType { UNKNOWN, BIBLESTUDIES, CHURCHLAW, RELSTUDIES };


std::map<RecordType, RecordTypeOfInterestPredicate> record_type_to_predicate_map{
    { BIBLESTUDIES, IsBibleStudiesRecord },
    { CHURCHLAW,    IsChurchLawRecord    },
    { RELSTUDIES,   IsRelStudiesRecord   },
};


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        ::Usage("type marc_filename untagged_ppn_list\n"
                "where \"type\" must be one of CHURCHLAW, BIBLESTUDIES or RELSTUDIES.\n");

    RecordType record_type(UNKNOWN);
    if (std::strcmp(argv[1], "CHURCHLAW") == 0)
        record_type = CHURCHLAW;
    else if (std::strcmp(argv[1], "RELSTUDIES") == 0)
        record_type = RELSTUDIES;
    else if (std::strcmp(argv[1], "BIBLESTUDIES") == 0)
        record_type = BIBLESTUDIES;
    else
        LOG_ERROR(std::string(argv[1]) + " is not a valid type!");

    const auto marc_reader(MARC::Reader::Factory(argv[2]));
    const auto list_file(FileUtil::OpenOutputFileOrDie(argv[3]));
    FindUntaggedPPNs(marc_reader.get(), list_file.get(), record_type_to_predicate_map[record_type]);

    return EXIT_SUCCESS;
}
