/** \file    find_tue_dups.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Local data blocks are embedded marc records inside of a record using LOK-Fields.
 *  Each local data block belongs to an institution and is marked by the institution's sigil.
 *  This tool filters for local data blocks of some institutions of the University of Tübingen
 *  and deletes all other local blocks.
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
#include "Compiler.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


static const RegexMatcher * const tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));


bool FindTueSigil(const MarcRecord * const record, const std::pair<size_t, size_t> &block_start_and_end,
                  std::string * const sigil)
{
    std::vector<size_t> field_indices;
    record->findFieldsInLocalBlock("852", "??", block_start_and_end, &field_indices);

    for (size_t field_index : field_indices) {
        const std::string field_data(record->getFieldData(field_index));
        const Subfields subfields(field_data);
        if (subfields.extractSubfieldWithPattern('a', *tue_sigil_matcher, sigil))
            return true;
    }
    return false;
}


bool FindTueDups(const MarcRecord * const record) {
    std::vector<std::pair<size_t, size_t>> local_block_boundaries;
    ssize_t local_data_count = record->findAllLocalDataBlocks(&local_block_boundaries);
    if (local_data_count == 0)
        return false;

    std::vector<std::string> sigils;
    for (const auto &block_start_and_end : local_block_boundaries) {
        std::string sigil;
        if (FindTueSigil(record, block_start_and_end, &sigil))
            sigils.emplace_back(sigil);
    }
    if (sigils.size() < 2)
        return false;

    const std::string _008_contents(record->getFieldData("008"));
    std::string publication_year;
    if (likely(_008_contents.length() >= 11))
        publication_year = _008_contents.substr(7, 4);

    const std::string _079_contents(record->getFieldData("008"));
    std::string area;
    if (not _079_contents.empty()) {
        const Subfields subfields(_079_contents);
        area = subfields.getFirstSubfieldValue('f');
    }

    std::sort(sigils.begin(), sigils.end());
    std::cout << record->getControlNumber() << "(" << publication_year << ',' << area <<"): " << StringUtil::Join(sigils, ',') << '\n';

    return true;
}


void FindTueDups(MarcReader * const marc_reader) {
    unsigned count(0), dups_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++count;
        if (FindTueDups(&record))
            ++dups_count;
    }
    std::cerr << "Processed " << count << " records and found " << dups_count << " dups.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));

    try {
        FindTueDups(marc_reader.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
