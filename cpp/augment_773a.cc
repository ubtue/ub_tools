/** \file    augment_773a.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  A tool for filling in 773$a if the 773 field exists and $a is missing.
 */

/*
    Copyright (C) 2016-2017, Library of the University of Tübingen

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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] marc_input marc_output\n"
              << "       \"marc_input\" is the file that will be augmented and converted.\n"
              << "       \"marc_input\" will be scoured for titles that\n"
              << "       may be filled into 773$a fields where appropriate.\n"
              << "       Populates 773$a where it is missing and uplinks exist in 773$x.\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_map<std::string, std::string> control_numbers_to_titles_map;


bool RecordControlNumberToTitleMapping(MarcRecord * const record, MarcWriter * const /*marc_writer*/,
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


void CollectControlNumberToTitleMappings(const bool verbose, MarcReader * const marc_reader) {
    if (verbose)
        std::cout << "Extracting control numbers to title mappings from \"" << marc_reader->getPath() << "\".\n";

    std::string err_msg;
    if (not MarcRecord::ProcessRecords(marc_reader, RecordControlNumberToTitleMapping,
                                       /* marc_writer = */nullptr, &err_msg))
        logger->error("error while looking for control numbers to title mappings: " + err_msg);

    if (verbose)
        std::cout << "Found " << control_numbers_to_titles_map.size() << " control number to title mappings.\n";
}


static unsigned patch_count;


// Looks for the existence of a 773 field.  Iff such a field exists and 773$a is missing, we try to add it.
bool PatchUpOne773a(MarcRecord * const record, MarcWriter * const marc_writer, std::string * const /*err_msg*/) {
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

    marc_writer->write(*record);

    return true;
}


// Iterates over all records in a collection and attempts to in 773$a subfields were they are missing.
void PatchUp773aSubfields(const bool verbose, MarcReader * const marc_reader, MarcWriter * const marc_writer) {
    std::string err_msg;
    if (not MarcRecord::ProcessRecords(marc_reader, PatchUpOne773a, marc_writer, &err_msg))
        logger->error("error while adding 773$a subfields to some records: " + err_msg);

    if (verbose)
        std::cout << "Added 773$a subfields to " << patch_count << " records.\n";
}


int main(int argc, char **argv) {
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

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[1], MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(argv[2], MarcWriter::BINARY));

    try {
        CollectControlNumberToTitleMappings(verbose, marc_reader.get());

        marc_reader->rewind();
        PatchUp773aSubfields(verbose, marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
