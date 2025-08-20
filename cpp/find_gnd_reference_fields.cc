/** \brief Utility for removing unreferenced authority records
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader) {
    std::string err_msg;
    RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory("\x1F"
                                          "0\\(DE-588\\)([^\x1F]+).*\x1F"
                                          "2gnd",
                                          &err_msg));
    if (matcher == nullptr)
        LOG_ERROR("failed to compile a regex in CollectGNDReferences: " + err_msg);

    std::set<std::string> gnd_reference_tags;
    while (const MARC::Record record = marc_reader->read()) {
        for (const auto &field : record) {
            if (matcher->matched(field.getContents()))
                gnd_reference_tags.emplace(field.getTag().toString());
        }
    }

    for (const auto &gnd_reference_tag : gnd_reference_tags)
        std::cout << gnd_reference_tag << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("title_data");

    std::unique_ptr<MARC::Reader> marc_title_reader(MARC::Reader::Factory(argv[1]));

    std::unordered_set<std::string> gnd_numbers;
    ProcessRecords(marc_title_reader.get());

    return EXIT_SUCCESS;
}
