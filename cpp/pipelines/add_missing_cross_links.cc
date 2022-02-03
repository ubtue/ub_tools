/** \brief Utility for adding back links to links found in 7?? fields $w subfields.
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

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


struct RecordInfo {
    std::vector<std::pair<std::string, std::string>> ppns_and_titles_;

public:
    RecordInfo() = default;
    RecordInfo(const RecordInfo &other) = default;
    RecordInfo(const std::string &ppn, const std::string &title): ppns_and_titles_{ { ppn, title } } { }
    void addPPNAndTitle(const std::string &ppn, const std::string &title) { ppns_and_titles_.emplace_back(ppn, title); }
    RecordInfo &operator=(const RecordInfo &rhs) = default;
};


std::vector<std::string> GetReferencedPPNs(const MARC::Record &record) {
    std::vector<std::string> referenced_ppns;

    for (const auto &field : record) {
        // We only look at fields whose tags start with a 7.
        if (field.getTag().c_str()[0] != '7' or field.getTag().toString() == "773")
            continue;

        const MARC::Subfields subfields(field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)"))
                referenced_ppns.emplace_back(subfield.value_.substr(__builtin_strlen("(DE-627)")));
        }
    }

    return referenced_ppns;
}


void ProcessRecords(MARC::Reader * const marc_reader, std::unordered_map<std::string, RecordInfo> * const ppn_to_description_map) {
    unsigned record_count(0), cross_link_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        const auto referenced_ppns(GetReferencedPPNs(record));
        for (const auto &referenced_ppn : referenced_ppns) {
            auto iter(ppn_to_description_map->find(referenced_ppn));
            if (iter == ppn_to_description_map->end())
                ppn_to_description_map->emplace(referenced_ppn, RecordInfo(record.getControlNumber(), record.getMainTitle()));
            else
                iter->second.addPPNAndTitle(record.getControlNumber(), record.getMainTitle());
            ++cross_link_count;
        }
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " MARC record(s).");
    LOG_INFO("Found " + std::to_string(cross_link_count) + " cross references to " + std::to_string(ppn_to_description_map->size())
             + " records.");
}


void AddMissingBackLinks(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                         const std::unordered_map<std::string, RecordInfo> &ppn_to_description_map) {
    unsigned added_count(0);
    while (MARC::Record record = marc_reader->read()) {
        const auto ppn_and_record(ppn_to_description_map.find(record.getControlNumber()));
        if (ppn_and_record != ppn_to_description_map.cend()) {
            const auto referenced_ppns(GetReferencedPPNs(record));
            for (const auto &ppn_and_title : ppn_and_record->second.ppns_and_titles_) {
                if (std::find_if(referenced_ppns.cbegin(), referenced_ppns.cend(),
                                 [ppn_and_record](const std::string &referenced_ppn) { return ppn_and_record->first == referenced_ppn; })
                    != referenced_ppns.cend())
                {
                    record.insertField("799", { { 'a', ppn_and_title.second }, { 'w', "(DE-627)" + ppn_and_title.first } });
                    ++added_count;
                }
            }
        }

        marc_writer->write(record);
    }

    LOG_INFO("Added " + std::to_string(added_count) + " missing back links.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unordered_map<std::string, RecordInfo> ppn_to_description_map;
    ProcessRecords(marc_reader.get(), &ppn_to_description_map);
    marc_reader->rewind();

    auto marc_writer(MARC::Writer::Factory(argv[2]));
    AddMissingBackLinks(marc_reader.get(), marc_writer.get(), ppn_to_description_map);

    return EXIT_SUCCESS;
}
