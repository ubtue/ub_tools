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
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] [--generate-dangling-log] marc_input marc_output\n"
              << "            If \"--generate-dangling-log\" has been specified an addition \"dangling.log\" file will be generated.\n\n";
    std::exit(EXIT_FAILURE);
}


void CollectRecordTypes(MARC::Reader * const reader, std::unordered_map<std::string, bool> * const ppn_to_is_electronic_map) {
    while (const auto record = reader->read())
        (*ppn_to_is_electronic_map)[record.getControlNumber()] = record.isElectronicResource();
}


void TagCrossLinks(const bool generate_dangling_log, MARC::Reader * const reader, MARC::Writer * const writer,
                   const std::unordered_map<std::string, bool> &ppn_to_is_electronic_map) {
    std::unique_ptr<File> dangling_log;
    if (generate_dangling_log)
        dangling_log = FileUtil::OpenOutputFileOrDie("dangling.log");

    unsigned link_target_is_same_type(0), link_target_is_different_type(0), danglink_link_count(0);
    while (auto record = reader->read()) {
        for (auto &field : record) {
            std::string partner_control_number;
            if (MARC::IsCrossLinkField(field, &partner_control_number)) {
                const auto ppn_and_is_electronic(ppn_to_is_electronic_map.find(partner_control_number));
                if (unlikely(ppn_and_is_electronic == ppn_to_is_electronic_map.cend())) {
                    LOG_WARNING("dangling cross link from \"" + record.getControlNumber() + "\" to \"" + partner_control_number + "\"!");
                    ++danglink_link_count;
                    field.appendSubfield('k', "dangling");
                    if (generate_dangling_log) {
                        const auto ddcs(record.getDDCs());
                        const auto rvks(record.getRVKs());
                        *dangling_log << record.getControlNumber() << ',' << partner_control_number
                                      << ",DDCs:" << StringUtil::Join(ddcs, ';') << ",RVKs:" << StringUtil::Join(rvks, ';') << ','
                                      << record.getLeader() << '\n';
                    }
                    continue;
                }

                if (ppn_and_is_electronic->second == record.isElectronicResource())
                    ++link_target_is_same_type;
                else
                    ++link_target_is_different_type;

                field.appendSubfield('k', ppn_and_is_electronic->second ? "Electronic" : "Non-Electronic");
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
    if (argc != 3 and argc != 4)
        Usage();

    bool generate_dangling_log(false);
    if (argc == 4) {
        if (std::strcmp(argv[1], "--generate-dangling-log") != 0)
            Usage();
        generate_dangling_log = true;
        --argc, ++argv;
    }

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::unordered_map<std::string, bool> ppn_to_is_electronic_map;
    CollectRecordTypes(marc_reader.get(), &ppn_to_is_electronic_map);
    marc_reader->rewind();

    TagCrossLinks(generate_dangling_log, marc_reader.get(), marc_writer.get(), ppn_to_is_electronic_map);

    return EXIT_SUCCESS;
}
