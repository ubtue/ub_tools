/** \file    add_is_superior_work_for_subsystems.cc
 *  \brief   Determine if a superior work has attached inferior works based on subsystems.
 *  \author  Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 */

/*
    Copyright (C) 2020, Library of the University of Tübingen

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

/*  We offer a list of tags and subfields where the primary data resides along
    with a list of tags and subfields where the synonym data is found and
    a list of unused fields in the title data where the synonyms can be stored
*/

#include <set>
#include <unordered_map>
#include <cstdlib>
#include "MARC.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


void CollectSubsystemInfo(MARC::Reader * const marc_reader,
                          std::unordered_map<std::string, std::set<std::string>> * const superior_ppns_to_subsystem_types)
{
    const std::string INSTALLATION_TYPE(VuFind::GetTueFindFlavourOrDie() == "ixtheo" ? "IXT" : "KRI");

    unsigned record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        for (const auto &superior_ppn : record.getParentControlNumbers()) {
            auto superior_ppn_and_subsystem_types(superior_ppns_to_subsystem_types->find(superior_ppn));
            if (superior_ppn_and_subsystem_types == superior_ppns_to_subsystem_types->end()) {
                const std::set<std::string> new_set{ INSTALLATION_TYPE };
                superior_ppn_and_subsystem_types = superior_ppns_to_subsystem_types->emplace(superior_ppn, new_set).first;
            }
            if (INSTALLATION_TYPE == "KRI")
                continue;

            /*if (record.hasSubfieldWithValue("SUB", 'a', "BIB"))*/
            if (record.hasTag("BIB")) // remove after migration
                superior_ppn_and_subsystem_types->second.emplace("BIB");
            /*if (record.hasSubfieldWithValue("SUB", 'a', "CAN"))*/
            if (record.hasTag("CAN")) // remove after migration
                superior_ppn_and_subsystem_types->second.emplace("CAN");
            /*if (record.hasSubfieldWithValue("SUB", 'a', "REL"))*/
            if (record.hasTag("REL")) // remove after migration
                superior_ppn_and_subsystem_types->second.emplace("REL");
        }
    }

    LOG_INFO("Read " + std::to_string(record_count) + " record(s).");
}


void PatchSPRFields(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::unordered_map<std::string, std::set<std::string>> &superior_ppns_to_subsystem_types)
{
    unsigned augmented_count(0);
    std::map<std::string, unsigned> subsystems_to_counts_map;
    while (MARC::Record record = marc_reader->read()) {
        auto spr_field(record.findTag("SPR"));
        if (spr_field != record.end()) {
            const auto ppn(record.getControlNumber());
            const auto superior_ppn_and_subsystem_types(superior_ppns_to_subsystem_types.find(ppn));
            if (superior_ppn_and_subsystem_types != superior_ppns_to_subsystem_types.cend()) {
                for (const auto &subsystem_type : superior_ppn_and_subsystem_types->second) {
                    auto subsystem_and_count(subsystems_to_counts_map.find(subsystem_type));
                    if (unlikely(subsystem_and_count == subsystems_to_counts_map.end()))
                        subsystems_to_counts_map.emplace(subsystem_type, 1);
                    else
                        ++(subsystem_and_count->second);

                    spr_field->appendSubfield('t', subsystem_type);
                }
                ++augmented_count;
            }
        }

        marc_writer->write(record);
    }

    std::string subsystem_stats;
    for (const auto &[subsystem, count] : subsystems_to_counts_map) {
        if (not subsystem_stats.empty())
            subsystem_stats += ", ";
        subsystem_stats += subsystem + ":" + std::to_string(count);
    }

    LOG_INFO("Augmented " + std::to_string(augmented_count) + " record(s) w/ SPR-fields w/ subsystem information. ("
             + subsystem_stats + ")");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("MARC input filename must not equal MARC output filename!");

    auto marc_reader(MARC::Reader::Factory(marc_input_filename));
    auto marc_writer(MARC::Writer::Factory(marc_output_filename));

    std::unordered_map<std::string, std::set<std::string>> superior_ppns_to_subsystem_types;
    CollectSubsystemInfo(marc_reader.get(), &superior_ppns_to_subsystem_types);
    marc_reader->rewind();
    PatchSPRFields(marc_reader.get(), marc_writer.get(), superior_ppns_to_subsystem_types);

    return EXIT_SUCCESS;
}
