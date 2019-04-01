/** \file    augment_773a.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for filling in 773$a if the 773 field exists and $a is missing.
 */

/*
    Copyright (C) 2016-2019, Library of the University of Tübingen

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
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_input marc_output\n"
              << "       \"marc_input\" is the file that will be augmented and converted.\n"
              << "       \"marc_input\" will be scoured for titles that\n"
              << "       may be filled into 773$a fields where appropriate.\n"
              << "       Populates 773$a where it and 773$t are both missing and uplinks exist in 773$w.\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_map<std::string, std::string> control_numbers_to_titles_map;


bool RecordControlNumberToTitleMapping(MARC::Record * const record) {
    for (auto &_245_field : record->getTagRange("245")) {
        std::string title(_245_field.getFirstSubfieldWithCode('a'));
        if (_245_field.hasSubfield('b'))
            title += " " + _245_field.getFirstSubfieldWithCode('b');
        StringUtil::RightTrim(" \t/", &title);
        if (likely(not title.empty()))
            control_numbers_to_titles_map[record->getControlNumber()] = title;
    }

    return true;
}


void CollectControlNumberToTitleMappings(const bool verbose, MARC::Reader * const marc_reader) {
    if (verbose)
        std::cout << "Extracting control numbers to title mappings from \"" << marc_reader->getPath() << "\".\n";

    while (auto record = marc_reader->read())
        RecordControlNumberToTitleMapping(&record);

    if (verbose)
        std::cout << "Found " << control_numbers_to_titles_map.size() << " control number to title mappings.\n";
}


static unsigned patch_count;


// Looks for the existence of a 773 field.  Iff such a field exists and 773$t and 773$a is missing, we try to add it.
bool PatchUpOne773a(MARC::Record * const record, MARC::Writer * const marc_writer) {
    for (auto &_773_field : record->getTagRange("773")) {
        if ((not _773_field.hasSubfield('a') and not _773_field.hasSubfield('t')) and _773_field.hasSubfield('w')) {
            const std::string w_subfield(_773_field.getFirstSubfieldWithCode('w'));
            if (StringUtil::StartsWith(w_subfield, "(DE-627)")) {
                const std::string parent_control_number(w_subfield.substr(8));
                const auto control_number_and_title(control_numbers_to_titles_map.find(parent_control_number));
                if (control_number_and_title != control_numbers_to_titles_map.end()) {
                    _773_field.insertOrReplaceSubfield('a', control_number_and_title->second);
                    ++patch_count;
                }
            }
        }
    }

    marc_writer->write(*record);

    return true;
}


// Iterates over all records in a collection and attempts to insert 773$a subfields were they and the 773$t subfields are missing.
void PatchUp773aSubfields(const bool verbose, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    while (auto record = marc_reader->read())
        PatchUpOne773a(&record, marc_writer);

    if (verbose)
        std::cout << "Added 773$a subfields to " << patch_count << " records.\n";
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc;
        ++argv;
    }

    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    CollectControlNumberToTitleMappings(verbose, marc_reader.get());
    marc_reader->rewind();
    PatchUp773aSubfields(verbose, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
