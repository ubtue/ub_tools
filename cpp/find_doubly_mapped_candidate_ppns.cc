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
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


struct PPNAndRecordType {
    std::string ppn_;
    MARC::Record::RecordType record_type_;
public:
    PPNAndRecordType(const std::string &ppn, const MARC::Record::RecordType record_type): ppn_(ppn), record_type_(record_type) { }
    PPNAndRecordType(const PPNAndRecordType &other) = default;
    PPNAndRecordType() = default;
    inline const PPNAndRecordType &operator=(const PPNAndRecordType &rhs) {
        ppn_ = rhs.ppn_;
        record_type_ = rhs.record_type_;
        return *this;
    }
};


void ProcessRecords(MARC::Reader * const marc_reader,
                    std::unordered_map<std::string, PPNAndRecordType> * const old_bsz_to_new_k10plus_ppns_map,
                    std::unordered_set<std::string> * const new_k10plus_ppns)
{
    unsigned identity_count(0), old_to_new_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("035")) {
            new_k10plus_ppns->emplace(record.getControlNumber());
            const auto subfield_a(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::StartsWith(subfield_a, "(DE-576)")) {
                const std::string old_bsz_ppn(subfield_a.substr(__builtin_strlen( "(DE-576)")));
                if (unlikely(old_bsz_ppn == record.getControlNumber()))
                    ++identity_count;
                else {
                    (*old_bsz_to_new_k10plus_ppns_map)[old_bsz_ppn] = PPNAndRecordType(record.getControlNumber(), record.getRecordType());
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

    std::unordered_map<std::string, PPNAndRecordType> old_bsz_to_new_k10plus_ppns_map;
    std::unordered_set<std::string> new_k10plus_ppns;

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get(), &old_bsz_to_new_k10plus_ppns_map, &new_k10plus_ppns);

    auto marc_reader2(MARC::Reader::Factory(argv[2]));
    ProcessRecords(marc_reader2.get(), &old_bsz_to_new_k10plus_ppns_map, &new_k10plus_ppns);

    const auto map_file(FileUtil::OpenOutputFileOrDie(argv[3]));

    std::unordered_map<std::string, std::string> k10plus_to_k10plus_map;
    for (const auto &bsz_and_k10plus_ppns : old_bsz_to_new_k10plus_ppns_map) {
        // Is the replaced PPN an old BSZ PPN?
        unsigned replacement_count(0);
        std::string final_k10plus_ppn(bsz_and_k10plus_ppns.first);
        const std::string correct_substitution(bsz_and_k10plus_ppns.second.ppn_);
        for (;;) {
            auto bsz_and_k10plus_ppn2(old_bsz_to_new_k10plus_ppns_map.find(final_k10plus_ppn));
            if (bsz_and_k10plus_ppn2 == old_bsz_to_new_k10plus_ppns_map.cend())
                break;
            final_k10plus_ppn = bsz_and_k10plus_ppn2->second.ppn_;
            ++replacement_count;
        }
        if (replacement_count > 1)
            (*map_file) << (bsz_and_k10plus_ppns.second.record_type_ == MARC::Record::RecordType::AUTHORITY ? "authority:" : "title:")
                        << k10plus_to_k10plus_map[final_k10plus_ppn] << "->" << correct_substitution << '\n';
    }
    LOG_INFO("Found " + std::to_string(k10plus_to_k10plus_map.size()) + " doubly mapped candidates.");

    return EXIT_SUCCESS;
}
