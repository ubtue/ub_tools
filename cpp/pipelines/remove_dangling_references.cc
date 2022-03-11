/** \brief Remove references to records that we don't have in our collection.
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_input marc_output missing_log");
}


void CollectAllPPNs(MARC::Reader * const reader, std::unordered_map<std::string, bool /*mark to remove record*/> * const all_ppns) {
    int next_year = std::stoi(TimeUtil::GetCurrentYear()) + 1;
    while (const auto record = reader->read()) {
        const std::string _008_field(record.getFirstFieldContents("008"));
        try {
            const int year = std::stoi(_008_field.substr(7, 4));
            all_ppns->emplace(record.getControlNumber(),
                              year > (next_year + 1)); // exclude records that will be pusblished in 2 years or later
        } catch (...) {
            all_ppns->emplace(record.getControlNumber(), false);
        }
    }
}


void EliminateDanglingCrossReferences(MARC::Reader * const reader, MARC::Writer * const writer, File * const log_file,
                                      const std::unordered_map<std::string, bool> &all_ppns) {
    unsigned modified_count(0);
    unsigned removed_count(0);
    while (auto record = reader->read()) {
        std::vector<size_t> field_indices_to_be_deleted;
        const auto first_field(record.begin());
        for (auto field(first_field); field != record.end(); ++field) {
            if (field->isCrossLinkField()) {
                for (const auto &subfield : field->getSubfields()) {
                    if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)")) {
                        const auto bsz_ppn(subfield.value_.substr(__builtin_strlen("(DE-627)")));
                        if (unlikely(all_ppns.find(bsz_ppn) == all_ppns.cend()) or all_ppns.find(bsz_ppn)->second == true) {
                            field_indices_to_be_deleted.emplace_back(field - first_field);
                            (*log_file) << record.getControlNumber() << ": " << field->getTag().toString() << " -> " << bsz_ppn << '\n';
                        }
                    }
                }
            }
        }

        if (not field_indices_to_be_deleted.empty()) {
            record.deleteFields(field_indices_to_be_deleted);
            ++modified_count;
        }

        if (all_ppns.find(record.getControlNumber())->second == false)
            writer->write(record);
        else
            ++removed_count;
    }
    LOG_INFO("Dropped dangling links from " + std::to_string(modified_count) + " record(s).");
    LOG_INFO("Dropped " + std::to_string(removed_count) + " record(s) due to publishing date in the future.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    const auto log_file(FileUtil::OpenOutputFileOrDie(argv[3]));

    std::unordered_map<std::string, bool> all_ppns;
    CollectAllPPNs(marc_reader.get(), &all_ppns);

    marc_reader->rewind();
    EliminateDanglingCrossReferences(marc_reader.get(), marc_writer.get(), log_file.get(), all_ppns);

    return EXIT_SUCCESS;
}
