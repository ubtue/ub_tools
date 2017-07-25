/** \file    
 *  \author Johannes Riedl  
 *
 *  A tool for generating a sorted list of works that are part of the 
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

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


#include <algorithm>
#include <iostream>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "TextUtil.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"

static unsigned extracted_count(0);
static std::set<std::string> de21_ppns;
static const RegexMatcher * const tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));

void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input de21_output_ppns\n";
    std::exit(EXIT_FAILURE);
}

void ProcessRecord(const MarcRecord &record) {
    // We are done if this is not a superior work
    if (record.getFieldData("SPR").empty())
        return;

    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record.findAllLocalDataBlocks(&local_block_boundaries);
    if (local_data_count == 0)
        return;

    for (const auto &block_start_and_end : local_block_boundaries) {
        std::vector<size_t> field_indices;
        record.findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);
    
        for (size_t field_index : field_indices) {
            const std::string field_data(record.getFieldData(field_index));
            const Subfields subfields(field_data);
            std::string sigil;
            if (subfields.extractSubfieldWithPattern('a', *tue_sigil_matcher, &sigil)) {
                de21_ppns.insert(record.getControlNumber());
                ++extracted_count;
            }
        }
    }
}


void WriteDE21Output(const std::unique_ptr<File> &output) {
    for (const auto ppn : de21_ppns) {
         output->write(ppn + '\n');
    }
}

void LoadDE21PPNs(MarcReader * const marc_reader) {
    while (MarcRecord record = marc_reader->read())
         ProcessRecord(record);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));
    const std::unique_ptr<File> de21_output(FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        LoadDE21PPNs(marc_reader.get());
        WriteDE21Output(de21_output);
        std::cerr << "Extracted " << extracted_count << " PPNs\n"; 
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}


