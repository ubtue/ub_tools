/** \file    delete_unused_local_data.cc
 *  \author  Oliver Obenland
 *
 *  Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *  Each local data block belongs to an institution and is marked by the institution's sigil.
 *  This tool filters for local data blocks of some institutions of the University of Tübingen
 *  and deletes all other local blocks.
 */

/*
    Copyright (C) 2016-2018, Library of the University of Tübingen

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
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


bool IsUnusedLocalBlock(const MARC::Record &record, const MARC::Record::const_iterator &block_start) {
    for (const auto &field : record.findFieldsInLocalBlock("852", block_start)) {
        const std::string field_data(field.getContents());
        if (field_data.find("aTü 135") != std::string::npos)
            return false;
        const size_t index = field_data.find("aDE-21");
        if (index != std::string::npos)
            return false;
    }
    return true;
}


void DeleteUnusedLocalData(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    ssize_t before_count(0), after_count(0);

    while (MARC::Record record = marc_reader->read()) {

        std::vector<MARC::Record::iterator> local_block_heads(record.findStartOfAllLocalDataBlocks()), local_blocks_to_delete;
        size_t local_data_count(local_block_heads.size());
        if (local_data_count == 0)
            return;

        before_count += local_data_count;
        for (const auto &local_block_start : local_block_heads) {
            if (IsUnusedLocalBlock(record, local_block_start)) {
                local_blocks_to_delete.emplace_back(local_block_start);
                --local_data_count;
            }
        }

        record.deleteLocalBlocks(local_blocks_to_delete);

        after_count += local_data_count;
        marc_writer->write(record);
    }
    std::cerr << ::progname << ": Deleted " << (before_count - after_count) << " of " << before_count << " local data blocks.\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(argv[2]));

    DeleteUnusedLocalData(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
