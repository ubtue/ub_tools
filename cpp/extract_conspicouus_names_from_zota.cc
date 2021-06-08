/** \file    extract_conspicouus_names_from_zota.cc
 *  \brief   Extract zota names that contain more than three parts
             more current authority data
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

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
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


bool isZota(const MARC::Record &record) {
    const auto local_block_starts(record.findStartOfAllLocalDataBlocks());
    for (const auto &local_block_start : local_block_starts) {
         for (const auto LOK935_field : record.getLocalTagRange("935", local_block_start)) {
             if (LOK935_field.hasSubfieldWithValue('a', "zota"))
                 return true;
         }
    }
    return false;
}


bool HasHasConspicuousName(const std::string &candidate) {
    std::string err_msg;
    std::string to_split(candidate);
    const std::string last_name(StringUtil::ExtractHead(&to_split, ","));
    const std::string first_name(StringUtil::ExtractHead(&to_split, ","));
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("([^\\s]+\\s+){2,}", &err_msg));
    if (matcher == nullptr)
        LOG_ERROR("Failed to compile regex matcher: " + err_msg);
    return matcher->matched(first_name) or matcher->matched(last_name);
}


void ProcessRecords(MARC::Reader * const marc_reader) {
    while (MARC::Record record = marc_reader->read()) {
        if (isZota(record)) {
            static const std::vector<std::string> tags_to_check({"100", "700"});
            for (auto tag_to_check : tags_to_check) {
                for (auto &field : record.getTagRange(tag_to_check)) {
                     const auto &author(field.getFirstSubfieldWithCode('a'));
                     if (HasHasConspicuousName(author)) {
                         std::cout << record.getControlNumber() + " | " + author + '\n';

                     }
                }
            }
        }
    }
}

} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    const std::string marc_input_filename(argv[1]);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));

    ProcessRecords(marc_reader.get());
    return EXIT_SUCCESS;
}


