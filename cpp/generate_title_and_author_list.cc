/** \brief Utility for generating a list of titles and authors from a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "Subfields.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ExtractAuthors(const MarcRecord &record, std::vector<std::string> * const authors) {
    authors->clear();

    std::vector<size_t> field_indices;
    record.getFieldIndices("100", &field_indices);
    for (const size_t field_index : field_indices) {
        const std::string field_contents(record.getFieldData(field_index));
        if (unlikely(field_contents.empty()))
            continue;

        const Subfields subfields(field_contents);
        const std::string author(subfields.getFirstSubfieldValue('a'));
        if (likely(not author.empty()))
            authors->emplace_back(author);
    }
}


void ProcessRecords(MarcReader * const marc_reader) {
    unsigned record_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;

        const std::string _245_contents(record.getFieldData("245"));
        if (unlikely(_245_contents.empty()))
            continue;

        const Subfields subfields(_245_contents);
        const std::string main_title(subfields.getFirstSubfieldValue('a'));
        if (unlikely(main_title.empty()))
            continue;

        std::cout << main_title << '\n';

        std::vector<std::string> authors;
        ExtractAuthors(record, &authors);
        for (const auto &author : authors)
            std::cout << '\t' << author << '\n';
    }

    std::cout << "Processed " << record_count << " MARC record(s).\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1]));

    try {
        ProcessRecords(marc_reader.get());
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
