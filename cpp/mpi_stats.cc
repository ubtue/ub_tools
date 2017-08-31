/** \file mpi_stats.cc
 *  \brief A tool for generating some stats.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_title_file\n";
    std::exit(EXIT_FAILURE);
}


bool IsMatchingRecord(const MarcRecord &record, const std::vector<std::pair<size_t, size_t>> &local_block_boundaries,
                      std::vector<std::string> &matching_subfield_a_values)
{
    for (const auto &local_block_boundary : local_block_boundaries) {
        const size_t block_start_index(local_block_boundary.first);
        const size_t block_end_index(local_block_boundary.second);
        for (size_t index(block_start_index); index < block_end_index; ++index) {
            const Subfields subfields(record.getFieldData(index));
            if (StringUtil::StartsWith(subfields.getFirstSubfieldValue('0'), "852")) {
                const auto begin_end(subfields.getIterators('a'));
                for (auto subfield_a(begin_end.first); subfield_a != begin_end.second; ++subfield_a) {
                    for (const auto &matching_subfield_a_value : matching_subfield_a_values) {
                        if (subfield_a->value_ == matching_subfield_a_value)
                            return true;
                    }
                }
            }
        }
    }

    return false;
}


bool IsMPIRecord(const MarcRecord &record, const std::vector<std::pair<size_t, size_t>> &local_block_boundaries) {
    static std::vector<std::string> subfield_a_values{ "DE-Frei85" };
    return IsMatchingRecord(record, local_block_boundaries, subfield_a_values);
}


bool IsUBOrIFKRecord(const MarcRecord &record, const std::vector<std::pair<size_t, size_t>> &local_block_boundaries) {
    static std::vector<std::string> subfield_a_values{ "DE-21", "DE-21-110" };
    return IsMatchingRecord(record, local_block_boundaries, subfield_a_values);
}


bool IsARecognisableYear(const std::string &year_candidate) {
    if (year_candidate.length() != 4)
        return false;

    for (char ch : year_candidate) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


// If we can find a recognisable year in 260$c we return it, o/w we return the empty string.
std::string GetPublicationYear(const MarcRecord &record) {
    const std::string _260_contents(record.getFieldData("260"));
    if (_260_contents.empty())
        return "";
    
    const Subfields subfields(_260_contents);
    const std::string year_candidate(subfields.getFirstSubfieldValue('c'));
    return IsARecognisableYear(year_candidate) ? year_candidate : "";
}


void FindNonMPIInstitutions(const MarcRecord &record,
                            const std::vector<std::pair<size_t, size_t>> &local_block_boundaries,
                            std::vector<std::string> * const non_mpi_institutions)
{
    non_mpi_institutions->clear();

    for (const auto &local_block_boundary : local_block_boundaries) {
        const size_t block_start_index(local_block_boundary.first);
        const size_t block_end_index(local_block_boundary.second);
        for (size_t index(block_start_index); index < block_end_index; ++index) {
            const Subfields subfields(record.getFieldData(index));
            if (StringUtil::StartsWith(subfields.getFirstSubfieldValue('0'), "852")) {
                const auto begin_end(subfields.getIterators('a'));
                for (auto subfield_a(begin_end.first); subfield_a != begin_end.second; ++subfield_a) {
                    if (subfield_a->value_ != "DE-Frei85")
                        non_mpi_institutions->emplace_back(subfield_a->value_);
                }
            }
        }
    }
}


void GenerateStats(MarcReader * const marc_reader) {
    unsigned recent_mpi_only_count(0), has_additional_non_mpi_institutions(0);
    while (const MarcRecord record = marc_reader->read()) {
        if (not record.getLeader().isMonograph())
            continue;

        std::vector<std::pair<size_t, size_t>> local_block_boundaries;
        record.findAllLocalDataBlocks(&local_block_boundaries);
        if (IsMPIRecord(record, local_block_boundaries) and not IsUBOrIFKRecord(record, local_block_boundaries)) {
            const std::string publication_year(GetPublicationYear(record));
            if (publication_year >= "2014") {
                ++recent_mpi_only_count;
                std::vector<std::string> non_mpi_institutions;
                FindNonMPIInstitutions(record, local_block_boundaries, &non_mpi_institutions);
                if (not non_mpi_institutions.empty()) {
                    ++has_additional_non_mpi_institutions;
                    std::cout << StringUtil::Join(non_mpi_institutions, ", ") << '\n';
                }
            }
        }
    }

    std::cout << "Counted " << recent_mpi_only_count << " records originating at the MPI and not found locally.\n";
    std::cout << "Counted " << has_additional_non_mpi_institutions
              << " records that have MPI and institutions other than UB or IFK.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc != 2)
            Usage();

        const std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));
        GenerateStats(marc_reader.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
