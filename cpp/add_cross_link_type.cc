/** \brief Adds type of link target for 775 and 776 cross links.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void CollectRecordTypes(MARC::Reader * const reader, std::unordered_map<std::string, bool> * const ppn_to_is_electronic_map) {
    while (const auto record = reader->read())
        (*ppn_to_is_electronic_map)[record.getControlNumber()] = record.isElectronicResource();
}


void TagCrossLinks(MARC::Reader * const reader, MARC::Writer * const writer,
                   const std::unordered_map<std::string, bool> &ppn_to_is_electronic_map)
{
    unsigned link_target_is_same_type(0), link_target_is_different_type(0), danglink_link_count(0);
    while (auto record = reader->read()) {
        for (auto &field : record) {
            std::string partner_control_number;
            if (MARC::IsCrossLinkField(field, &partner_control_number)) {
                const auto ppn_and_is_electronic(ppn_to_is_electronic_map.find(partner_control_number));
                if (unlikely(ppn_and_is_electronic == ppn_to_is_electronic_map.cend())) {
                    LOG_WARNING("dangling cross link from \"" + record.getControlNumber() + "\" to \"" + partner_control_number + "\"!");
                    ++danglink_link_count;
                    continue;
                }

                if (ppn_and_is_electronic->second == record.isElectronicResource())
                    ++link_target_is_same_type;
                else
                    ++link_target_is_different_type;

                field.appendSubfield('x', ppn_and_is_electronic->second ? "electronic" : "non-electronic");
            }
        }

        writer->write(record);
    }

    LOG_INFO(std::to_string(link_target_is_different_type) + " cross links point to different types and "
             + std::to_string(link_target_is_same_type) + " cross links point to identical types.");
    LOG_WARNING(std::to_string(danglink_link_count) + " cross links were dangling!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::unordered_map<std::string, bool> ppn_to_is_electronic_map;
    CollectRecordTypes(marc_reader.get(), &ppn_to_is_electronic_map);
    marc_reader->rewind();

    TagCrossLinks(marc_reader.get(), marc_writer.get(), ppn_to_is_electronic_map);

    return EXIT_SUCCESS;
}
