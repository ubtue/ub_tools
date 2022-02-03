/** \file krimdok_flag_pda_records.cc
 *  \brief A tool for adding a PDA field to KrimDok records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de)
 *
 *  \copyright 2016-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " no_of_years marc_input_file marc_output_file\n";
    std::exit(EXIT_FAILURE);
}


bool IsMatchingRecord(const MARC::Record &record, const std::vector<MARC::Record::iterator> &local_block_starts,
                      std::vector<std::string> &matching_subfield_a_values) {
    for (const auto local_block_start : local_block_starts) {
        auto _852_fields(record.findFieldsInLocalBlock("852", local_block_start));
        if (_852_fields.empty())
            continue;

        for (const auto &_852_field : _852_fields) {
            std::vector<std::string> subfield_a_values(_852_field.getSubfields().extractSubfields('a'));
            for (const auto &subfield_a_value : subfield_a_values) {
                for (const auto &matching_subfield_a_value : matching_subfield_a_values) {
                    if (subfield_a_value == matching_subfield_a_value)
                        return true;
                }
            }
        }
    }
    return false;
}


bool IsMPIRecord(const MARC::Record &record, const std::vector<MARC::Record::iterator> &local_block_starts) {
    static std::vector<std::string> subfield_a_values{ "DE-Frei85" };
    return IsMatchingRecord(record, local_block_starts, subfield_a_values);
}


bool IsUBOrIFKRecord(const MARC::Record &record, const std::vector<MARC::Record::iterator> &local_block_starts) {
    static std::vector<std::string> subfield_a_values{ "DE-21", "DE-21-110" };
    return IsMatchingRecord(record, local_block_starts, subfield_a_values);
}


void FindNonMPIInstitutions(const MARC::Record &record, const std::vector<MARC::Record::iterator> &local_block_starts,
                            std::vector<std::string> * const non_mpi_institutions) {
    non_mpi_institutions->clear();

    for (const auto &local_block_start : local_block_starts) {
        const auto _852_fields(record.findFieldsInLocalBlock("852", local_block_start));
        if (_852_fields.empty())
            return;

        for (const auto &_852_field : _852_fields) {
            std::vector<std::string> subfield_a_values(_852_field.getSubfields().extractSubfields('a'));
            for (const auto &subfield_a_value : subfield_a_values)
                if (subfield_a_value != "DE-Frei85")
                    non_mpi_institutions->emplace_back(subfield_a_value);
        }
    }
}


void AddPDAFieldToRecords(const std::string &cutoff_year, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned pda_field_added_count(0);
    while (MARC::Record record = marc_reader->read()) {
        if (not record.isMonograph()) {
            marc_writer->write(record);
            continue;
        }

        auto local_block_starts(record.findStartOfAllLocalDataBlocks());
        if (IsMPIRecord(record, local_block_starts) and not IsUBOrIFKRecord(record, local_block_starts)) {
            const std::string publication_year(record.getMostRecentPublicationYear());
            if (publication_year >= cutoff_year) {
                std::vector<std::string> non_mpi_institutions;
                FindNonMPIInstitutions(record, local_block_starts, &non_mpi_institutions);
                if (non_mpi_institutions.empty()) {
                    ++pda_field_added_count;
                    record.insertField("PDA", { { 'a', "yes" } });
                    marc_writer->write(record);
                    continue;
                }
            }
        }

        marc_writer->write(record);
    }

    LOG_INFO("Added a PDA field to " + std::to_string(pda_field_added_count) + " record(s).");
}


std::string GetCutoffYear(const unsigned no_of_years) {
    const unsigned current_year(StringUtil::ToUnsigned(TimeUtil::GetCurrentYear(TimeUtil::LOCAL)));
    return std::to_string(current_year - no_of_years);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const unsigned no_of_years(StringUtil::ToUnsigned(argv[1]));
    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2]));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[3]));
    AddPDAFieldToRecords(GetCutoffYear(no_of_years), marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
