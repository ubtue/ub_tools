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
                    std::unordered_map<std::string, MARC::Record::RecordType> * const ppn_to_record_type_map)
{
    unsigned identity_count(0), old_to_new_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("035")) {
            const auto subfield_a(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::StartsWith(subfield_a, "(DE-576)")) {
                const std::string old_bsz_ppn(subfield_a.substr(__builtin_strlen( "(DE-576)")));
                if (unlikely(old_bsz_ppn == record.getControlNumber()))
                    ++identity_count;
                else {
                    (*ppn_to_record_type_map)[record.getControlNumber()] = (*ppn_to_record_type_map)[old_bsz_ppn] = record.getRecordType();
                    (*old_bsz_to_new_k10plus_ppns_map)[old_bsz_ppn] = record.getControlNumber();
                    ++old_to_new_count;
                }
                continue;
            }
        }
    }

    LOG_INFO("Found " + std::to_string(identity_count) + " identity mappings.");
    LOG_INFO("Found " + std::to_string(old_to_new_count) + " mappings of old BSZ PPN's to new K10+ PPN's.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("title_records authority_records backpatch.map");

    std::unordered_map<std::string, std::string> old_bsz_to_new_k10plus_ppns_map;
    std::unordered_map<std::string, MARC::Record::RecordType> ppn_to_record_type_map;

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get(), &old_bsz_to_new_k10plus_ppns_map, &ppn_to_record_type_map);

    auto marc_reader2(MARC::Reader::Factory(argv[2]));
    ProcessRecords(marc_reader2.get(), &old_bsz_to_new_k10plus_ppns_map, &ppn_to_record_type_map);

    const auto map_file(FileUtil::OpenOutputFileOrDie((argv[3])));

    unsigned multiply_mapped_count(0);
    for (const auto &bsz_and_k10plus_ppns : old_bsz_to_new_k10plus_ppns_map) {
        // Is the replaced PPN an old BSZ PPN?
        std::vector<std::string> replacement_chain;
        std::string final_k10plus_ppn(bsz_and_k10plus_ppns.first);
        for (;;) {
            auto bsz_and_k10plus_ppn2(old_bsz_to_new_k10plus_ppns_map.find(final_k10plus_ppn));
            if (bsz_and_k10plus_ppn2 == old_bsz_to_new_k10plus_ppns_map.cend())
                break;
            final_k10plus_ppn = bsz_and_k10plus_ppn2->second;
            const std::string record_type(
                ppn_to_record_type_map[final_k10plus_ppn] == MARC::Record::RecordType::BIBLIOGRAPHIC ? "Bib" : "Auth");
            replacement_chain.emplace_back(final_k10plus_ppn + "(" + record_type + ")");
        }
        if (replacement_chain.size() > 1) {
            ++multiply_mapped_count;
            (*map_file) << StringUtil::Join(replacement_chain, " -> ") << '\n';
        }
    }
    LOG_INFO("Found " + std::to_string(multiply_mapped_count) + " multiply mapped candidates.");

    return EXIT_SUCCESS;
}
