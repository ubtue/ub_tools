/** \brief Utility for displaying various bits of info about a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_data\n";
    std::exit(EXIT_FAILURE);
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 2)
        Usage();

    MARC::BinaryReader reader(argv[1]);
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie("/tmp/out.mrc"));
    MARC::BinaryWriter writer(output.get());

    try {
        unsigned record_count(0);
        size_t max_record_size(0), max_field_count(0), max_local_block_count(0), max_subfield_count(0);
        std::map<MARC::Record::RecordType, unsigned> record_types_and_counts;
        while (const MARC::Record record = reader.read()) {
            writer.write(record);
            ++record_count;
            if (record.size() > max_record_size)
                max_record_size = record.size();
            if (record.getNumberOfFields() > max_field_count)
                max_field_count = record.getNumberOfFields();

            const MARC::Record::RecordType record_type(record.getRecordType());
            ++record_types_and_counts[record_type];
            if (record_type == MARC::Record::RecordType::UNKNOWN)
                std::cerr << "Unknown record type '" << record.getLeader()[6] << "' for control number "
                          << record.getControlNumber() << ".\n";

            for (const auto &field : record) {
                if (field.isDataField()) {
                    const MARC::Subfields subfields(field);
                    if (unlikely(subfields.size() > max_subfield_count))
                        max_subfield_count = subfields.size();
                }
            }

            std::vector<std::pair<size_t, size_t>> local_block_boundaries;
            const size_t local_block_count(record.findAllLocalDataBlocks(&local_block_boundaries));
            if (local_block_count > max_local_block_count)
                max_local_block_count = local_block_count;
        }

        std::cerr << "Read " << record_count << " record(s).\n";
        std::cerr << "The largest record contains " << max_record_size << " bytes.\n";
        std::cerr << "The record with the largest number of fields contains " << max_field_count << " field(s).\n";
        std::cerr << "The record with the most local data blocks has " << max_local_block_count
                  << " local block(s).\n";
        std::cerr << "Counted " << record_types_and_counts[MARC::Record::RecordType::BIBLIOGRAPHIC]
                  << " bibliographic record(s), " << record_types_and_counts[MARC::Record::RecordType::AUTHORITY]
                  << " classification record(s), " << record_types_and_counts[MARC::Record::RecordType::CLASSIFICATION]
                  << " authority record(s), and " << record_types_and_counts[MARC::Record::RecordType::UNKNOWN]
                  << " record(s) of unknown record type.\n";
        std::cerr << "The field with the most subfields has " << max_subfield_count << " subfield(s).\n";
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
