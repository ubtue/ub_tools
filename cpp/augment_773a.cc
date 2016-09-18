/** \file    augment_773a.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for filling in 773$a if the 773 field exists and $a is missing.
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] marc_input marc_output\n"
              << "       \"marc_input\" is the file that will be augmented and converted.\n"
              << "       \"marc_input\" will be scoured for titles that\n"
              << "       may be filled into 773$a fields where appropriate.\n"
              << "       Populates 773$a where it is missing and uplinks exist in 773$x.\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_map<std::string, std::string> control_numbers_to_titles_map;


bool RecordControlNumberToTitleMapping(MarcRecord * const record, File * const /*output*/,
                 std::string * const /* err_msg */)
{
    size_t _245_index;
    if (likely((_245_index = record->getFieldIndex("245")) != MarcRecord::FIELD_NOT_FOUND)) {
        const Subfields _245_subfields(record->getSubfields(_245_index));
        std::string title(_245_subfields.getFirstSubfieldValue('a'));
        if (_245_subfields.hasSubfield('b'))
            title += " " + _245_subfields.getFirstSubfieldValue('b');
        StringUtil::RightTrim(" \t/", &title);
        if (likely(not title.empty()))
            control_numbers_to_titles_map[record->getControlNumber()] = title;
    }

    return true;
}


void CollectControlNumberToTitleMappings(const bool verbose, File * const input) {
    if (verbose)
        std::cout << "Extracting control numbers to title mappings from \"" << input->getPath() << "\".\n";

    std::string err_msg;
    if (not MarcRecord::ProcessRecords(input, /* output = */ nullptr, RecordControlNumberToTitleMapping, &err_msg))
        Error("error while looking for control numbers to title mappings: " + err_msg);

    if (verbose)
        std::cout << "Found " << control_numbers_to_titles_map.size() << " control number to title mappings.\n";
}


static unsigned patch_count;


// Looks for the existence of a 773 field.  Iff such a field exists and 773$a is missing, we try to add it.
bool PatchUpOne773a(MarcRecord * const record, File * const output, std::string * const /*err_msg*/) {
    size_t _773_index;
    if ((_773_index = record->getFieldIndex("773")) != MarcRecord::FIELD_NOT_FOUND) {
        Subfields _773_subfields(record->getSubfields(_773_index));
        if (not _773_subfields.hasSubfield('a') and _773_subfields.hasSubfield('w')) {
            const std::string w_subfield(_773_subfields.getFirstSubfieldValue('w'));
            if (StringUtil::StartsWith(w_subfield, "(DE-576)")) {
                const std::string parent_control_number(w_subfield.substr(8));
                const auto control_number_and_title(control_numbers_to_titles_map.find(parent_control_number));
                if (control_number_and_title != control_numbers_to_titles_map.end()) {
                    _773_subfields.addSubfield('a', control_number_and_title->second);
                    record->updateField(_773_index, _773_subfields.toString());
                    ++patch_count;
                }
            }
        }
    }

    MarcWriter::Write(*record, output);

    return true;
}


// Iterates over all records in a collection and attempts to in 773$a subfields were they are missing.
void PatchUp773aSubfields(const bool verbose, File * const input, File * const output) {
    std::string err_msg;
    if (not MarcRecord::ProcessRecords(input, output, PatchUpOne773a, &err_msg))
        Error("error while adding 773$a subfields to some records: " + err_msg);

    if (verbose)
        std::cout << "Added 773$a subfields to " << patch_count << " records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

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

    const std::string marc_input_filename(argv[1]);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[2]);
    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        CollectControlNumberToTitleMappings(verbose, &marc_input);

        marc_input.rewind();
        PatchUp773aSubfields(verbose, &marc_input, &marc_output);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
