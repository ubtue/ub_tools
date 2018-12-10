/** \brief Utility for generating data/tags_and_index_terms.map from BSZ records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "TextUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input map_output\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(MARC::Reader * const reader, std::unordered_map<std::string, std::string>  * const subject_terms_to_tags_map) {
    static const std::vector<std::string> SUBJECT_ACCESS_TAGS{ "647", "648", "650", "651" };

    while (const auto record = reader->read()) {
        for (const auto &subject_access_tag : SUBJECT_ACCESS_TAGS) {
            for (const auto &field : record.getTagRange(subject_access_tag)) {
                const auto subject(field.getFirstSubfieldWithCode('a'));
                if (likely(not subject.empty() and field.getFirstSubfieldWithCode('2') == "gnd"))
                    (*subject_terms_to_tags_map)[TextUtil::UTF8ToLower(field.getContents())] = subject_access_tag;
            }
        }
    }

    LOG_INFO("found " + std::to_string(subject_terms_to_tags_map->size()) + " unique (tag, subject term) pairs.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unordered_map<std::string, std::string> subject_terms_to_tags_map;
    ProcessRecords(marc_reader.get(), &subject_terms_to_tags_map);

    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    for (const auto subject_term_and_tag : subject_terms_to_tags_map)
        (*output) << subject_term_and_tag.second << StringUtil::CStyleEscape(subject_term_and_tag.first) << '\n';

    return EXIT_SUCCESS;
}
