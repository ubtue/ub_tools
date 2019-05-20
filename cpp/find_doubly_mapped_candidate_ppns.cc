/** \brief Utility for finding potentially doubly-mapped PPN's.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MapUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const old_bsz_to_new_k10plus_ppns_map,
                    std::unordered_set<std::string> * const new_k10plus_ppns)
{
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("035")) {
            new_k10plus_ppns->emplace(record.getControlNumber());
            const auto subfield_a(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::StartsWith(subfield_a, "(DE-576)")) {
                (*old_bsz_to_new_k10plus_ppns_map)[subfield_a.substr(__builtin_strlen( "(DE-576)"))] = record.getControlNumber();
                continue;
            }
        }
    }

    LOG_INFO("Found " + std::to_string(old_bsz_to_new_k10plus_ppns_map->size()) + " mappings of old BSZ PPN's to new K10+ PPN's.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("title_records authority_records found_candidates_map");

    std::unordered_map<std::string, std::string> old_bsz_to_new_k10plus_ppns_map;
    std::unordered_set<std::string> new_k10plus_ppns;

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get(), &old_bsz_to_new_k10plus_ppns_map, &new_k10plus_ppns);

    auto marc_reader2(MARC::Reader::Factory(argv[2]));
    ProcessRecords(marc_reader2.get(), &old_bsz_to_new_k10plus_ppns_map, &new_k10plus_ppns);

    std::unordered_map<std::string, std::string> k10plus_to_k10plus_map;
    for (const auto &bsz_and_k10plus_ppns : old_bsz_to_new_k10plus_ppns_map) {
        const auto k10plus_ppn(new_k10plus_ppns.find(bsz_and_k10plus_ppns.first));
        if (k10plus_ppn != new_k10plus_ppns.cend())
            k10plus_to_k10plus_map[*k10plus_ppn] = bsz_and_k10plus_ppns.second;
    }
    LOG_INFO("Found " + std::to_string(k10plus_to_k10plus_map.size()) + " doubly mapped candidates.");

    MapUtil::SerialiseMap(argv[3], k10plus_to_k10plus_map);

    return EXIT_SUCCESS;
}
