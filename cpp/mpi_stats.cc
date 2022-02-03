/** \file mpi_stats.cc
 *  \brief A tool for generating some stats.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_title_file");
}


bool IsMatchingRecord(const MARC::Record &record, const std::vector<MARC::Record::const_iterator> &local_block_starts,
                      std::vector<std::string> &matching_subfield_a_values) {
    for (const auto &local_block_start : local_block_starts) {
        auto field(local_block_start);
        auto last_local_tag(field->getLocalTag());
        while (likely(field != record.end()) and field->getLocalTag() >= last_local_tag) {
            last_local_tag = field->getLocalTag();
            if (last_local_tag == "852") {
                const auto subfield_a_value(field->getFirstSubfieldWithCode('a'));
                for (const auto &matching_subfield_a_value : matching_subfield_a_values) {
                    if (matching_subfield_a_value == subfield_a_value)
                        return true;
                }
            }
            ++field;
        }
    }

    return false;
}


bool IsMPIRecord(const MARC::Record &record, const std::vector<MARC::Record::const_iterator> &local_block_starts) {
    static std::vector<std::string> subfield_a_values{ "DE-Frei85" };
    return IsMatchingRecord(record, local_block_starts, subfield_a_values);
}


bool IsUBOrIFKRecord(const MARC::Record &record, const std::vector<MARC::Record::const_iterator> &local_block_starts) {
    static std::vector<std::string> subfield_a_values{ "DE-21", "DE-21-110" };
    return IsMatchingRecord(record, local_block_starts, subfield_a_values);
}


void FindNonMPIInstitutions(const MARC::Record &record, const std::vector<MARC::Record::const_iterator> &local_block_starts,
                            std::vector<std::string> * const non_mpi_institutions) {
    non_mpi_institutions->clear();

    for (const auto &local_block_start : local_block_starts) {
        auto field(local_block_start);
        auto last_local_tag(field->getLocalTag());
        while (likely(field != record.end()) and field->getLocalTag() >= last_local_tag) {
            last_local_tag = field->getLocalTag();
            if (last_local_tag == "852") {
                const auto subfields(field->getSubfields());
                for (const auto &subfield : subfields) {
                    if (subfield.code_ == 'a' and subfield.value_ != "DE-Frei85")
                        non_mpi_institutions->emplace_back(subfield.value_);
                }
            }
            ++field;
        }
    }
}


void GenerateStats(MARC::Reader * const marc_reader) {
    unsigned recent_mpi_only_count(0), has_additional_non_mpi_institutions(0);
    while (const MARC::Record record = marc_reader->read()) {
        if (not record.isMonograph())
            continue;

        const auto local_block_starts(record.findStartOfAllLocalDataBlocks());
        if (IsMPIRecord(record, local_block_starts) and not IsUBOrIFKRecord(record, local_block_starts)) {
            const std::string publication_year(record.getMostRecentPublicationYear());
            if (publication_year >= "2014") {
                ++recent_mpi_only_count;
                std::vector<std::string> non_mpi_institutions;
                FindNonMPIInstitutions(record, local_block_starts, &non_mpi_institutions);
                if (not non_mpi_institutions.empty()) {
                    ++has_additional_non_mpi_institutions;
                    std::cout << StringUtil::Join(non_mpi_institutions, ", ") << '\n';
                }
            }
        }
    }

    std::cout << "Counted " << recent_mpi_only_count << " records originating at the MPI and not found locally.\n";
    std::cout << "Counted " << has_additional_non_mpi_institutions << " records that have MPI and institutions other than UB or IFK.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    GenerateStats(marc_reader.get());

    return EXIT_SUCCESS;
}
