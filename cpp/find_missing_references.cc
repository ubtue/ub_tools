/** \brief  Utility for finding referenced PPN's that we should have, but that are missing.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input missing_references");

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(nullptr);
    const auto missing_references_log(FileUtil::OpenOutputFileOrDie(argv[2]));

    std::unordered_set<std::string> all_ppns;
    while (const auto record = marc_reader->read())
        all_ppns.emplace(record.getControlNumber());
    marc_reader->rewind();

    std::unordered_map<std::string, std::set<std::string>> missing_ppns_to_referers_map;
    while (const auto record = marc_reader->read()) {
        for (const auto _787_field : record.getTagRange("787")) {
            if (_787_field.getFirstSubfieldWithCode('i') != "Rezension von")
                continue;

            for (const auto &subfield : _787_field.getSubfields()) {
                if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)")) {
                    const auto referenced_ppn(subfield.value_.substr(__builtin_strlen("(DE-627)")));
                    if (all_ppns.find(referenced_ppn) == all_ppns.end()) {
                        auto missing_ppn_and_referers(missing_ppns_to_referers_map.find(referenced_ppn));
                        if (missing_ppn_and_referers != missing_ppns_to_referers_map.end())
                            missing_ppn_and_referers->second.emplace(record.getControlNumber());
                        else
                            missing_ppns_to_referers_map.emplace(referenced_ppn, std::set<std::string>{ record.getControlNumber() });
                    }
                    break;
                }
            }
        }
    }

    for (const auto &[missing_ppn, referers] : missing_ppns_to_referers_map)
        (*missing_references_log) << missing_ppn << " <- " << StringUtil::Join(referers, ", ") << '\n';

    LOG_INFO("Found " + std::to_string(missing_ppns_to_referers_map.size()) + " missing reference(s).");

    return EXIT_FAILURE;
}
