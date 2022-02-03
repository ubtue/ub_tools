/** \brief Utility for generating a list of titles and authors from a collection of MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_data\n";
    std::exit(EXIT_FAILURE);
}


void ExtractAuthors(const MARC::Record &record, std::vector<std::string> * const authors) {
    authors->clear();

    for (const auto &field : record.getTagRange("100")) {
        const auto subfields(field.getSubfields());
        const std::string author(subfields.getFirstSubfieldWithCode('a'));
        if (likely(not author.empty()))
            authors->emplace_back(author);
    }
}


void ProcessRecords(MARC::Reader * const marc_reader) {
    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        const auto field_245(record.findTag("245"));
        if (unlikely(field_245 == record.end()))
            continue;

        const std::string main_title(field_245->getSubfields().getFirstSubfieldWithCode('a'));
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


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    ProcessRecords(marc_reader.get());

    return EXIT_SUCCESS;
}
