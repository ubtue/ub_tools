/** \brief Generates a list of fields that contain subfields that reference other records
 *  \author Dr. Johannes Ruscheinski
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <set>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("marc_input");
}


void ListCrossLinkFields(MARC::Reader * const reader) {
    std::set<std::string> cross_link_fields;
    while (const auto record = reader->read()) {
        for (const auto &field : record) {
            if (field.isControlField() or not field.hasSubfield('w'))
                continue;

            const auto subfields(field.getSubfields());
            for (const auto &subfield : subfields) {
                if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)")) {
                    cross_link_fields.emplace(field.getTag().toString());
                    break;
                }
            }
        }
    }

    for (const auto &tag : cross_link_fields)
        std::cout << tag << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    ListCrossLinkFields(marc_reader.get());

    return EXIT_SUCCESS;
}
