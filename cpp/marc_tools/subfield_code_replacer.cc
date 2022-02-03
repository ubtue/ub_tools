/** \file    subfield_code_replacer.cc
 *  \brief   A tool for replacing subfield codes in MARC-21 data sets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2017,2018 Library of the University of Tübingen

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
#include <memory>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output pattern1 [pattern2 .. patternN]\n";
    std::cerr << "  where each pattern must look like TTTa=b where TTT is a tag and \"a\" and \"b\"\n";
    std::cerr << "  are subfield codes.\n\n";
    std::exit(EXIT_FAILURE);
}


struct Replacement {
    std::string tag_;
    char old_code_, new_code_;

public:
    Replacement(const std::string &tag, const char old_code, const char new_code): tag_(tag), old_code_(old_code), new_code_(new_code) { }
};


// \return True if at least one code has been replaced else false.
bool ReplaceCodes(MARC::Record * const record, const std::vector<Replacement> &replacements) {
    bool replaced_at_least_one_code(false);

    for (const auto &replacement : replacements) {
        for (auto &field : record->getTagRange(replacement.tag_)) {
            if (field.replaceSubfieldCode(replacement.old_code_, replacement.new_code_))
                replaced_at_least_one_code = true;
        }
    }

    return replaced_at_least_one_code;
}


void ReplaceCodes(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, const std::vector<Replacement> &replacements) {
    unsigned total_count(0), modified_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++total_count;

        if (ReplaceCodes(&record, replacements))
            ++modified_count;

        marc_writer->write(record);
    }

    LOG_INFO("Read " + std::to_string(total_count) + " records.");
    LOG_INFO("Modified " + std::to_string(modified_count) + " record(s).");
}


void CollectReplacements(int argc, char **argv, std::vector<Replacement> * const replacements) {
    for (int arg_no(3); arg_no < argc; ++arg_no) {
        const std::string replacement_pattern(argv[arg_no]);
        if (replacement_pattern.length() != 6 or replacement_pattern[4] != '=')
            LOG_ERROR("bad replacement pattern: \"" + replacement_pattern + "\"!");
        replacements->emplace_back(replacement_pattern.substr(0, 3), replacement_pattern[3], replacement_pattern[5]);
    }
}


} // namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 4)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    std::vector<Replacement> replacements;
    CollectReplacements(argc, argv, &replacements);
    if (replacements.empty())
        LOG_ERROR("need at least one replacement pattern!");

    ReplaceCodes(marc_reader.get(), marc_writer.get(), replacements);

    return EXIT_SUCCESS;
}
