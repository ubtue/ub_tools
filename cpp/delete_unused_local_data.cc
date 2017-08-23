/** \file    delete_unused_local_data.cc
 *  \author  Oliver Obenland
 *
 *  Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *  Each local data block belongs to an institution and is marked by the institution's sigil.
 *  This tool filters for local data blocks of some institutions of the University of Tübingen
 *  and deletes all other local blocks.
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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

#include <iostream>
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "util.h"


static ssize_t count(0), before_count(0), after_count(0);


void Usage() {
    std::cerr << "Usage: " << ::progname << "  marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


bool IsUnusedLocalBlock(const MarcRecord * const record, const std::pair<size_t, size_t> &block_start_and_end) {
    std::vector<size_t> field_indices;
    record->findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);

    for (size_t field_index : field_indices) {
        const std::string field_data(record->getFieldData(field_index));
        if (field_data.find("aTü 135") != std::string::npos)
            return false;
        const size_t index = field_data.find("aDE-21");
        if (index != std::string::npos)
            return false;
    }
    return true;
}


void ProcessRecord(MarcRecord * const record) {
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    std::vector<std::pair<size_t, size_t>> local_blocks_to_delete;
    ssize_t local_data_count = record->findAllLocalDataBlocks(&local_block_boundaries);
    if (local_data_count == 0)
        return;

    before_count += local_data_count;
    for (const auto &block_start_and_end : local_block_boundaries) {
        if (IsUnusedLocalBlock(record, block_start_and_end)) {
            local_blocks_to_delete.emplace_back(block_start_and_end);
            --local_data_count;
        }
    }
    record->deleteFields(local_blocks_to_delete);

    after_count += local_data_count;
}


void DeleteUnusedLocalData(MarcReader * const marc_reader, MarcWriter * const marc_writer) {
    while (MarcRecord record = marc_reader->read()) {
        ++count;
        ProcessRecord(&record);
        marc_writer->write(record);
    }
    std::cerr << ::progname << ": Deleted " << (before_count - after_count) << " of " << before_count
              << " local data blocks.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2], MarcWriter::BINARY));

    try {
        DeleteUnusedLocalData(marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
