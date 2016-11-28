/** \file    convert_bible_references.cc
 *  \brief   Tool for RDA conversion of bible reference norm data.
 *  \author  Dr. Johannes Ruscheinski
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
#include <fstream>
#include <iostream>
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


static unsigned conversion_count(0);


// Spits out those records that have a 040$e field which starts with "rak" and has a 130 field.
// In that case it sets 040$e to "rda", move the contents of 130$a to 130$p and sets 130$a
// to "Bibel".
void ProcessRecord(MarcRecord * const record, MarcWriter * const marc_writer) {
    const size_t _040_index(record->getFieldIndex("040"));
    if (_040_index == MarcRecord::FIELD_NOT_FOUND)
        return; // Nothing to do!

    const size_t _130_index(record->getFieldIndex("130"));
    if (_130_index == MarcRecord::FIELD_NOT_FOUND)
        return; // Nothing to do!

    Subfields _040_subfields(record->getSubfields(_040_index));
    if (not StringUtil::StartsWith(_040_subfields.getFirstSubfieldValue('e'), "rak"))
        return; // Nothing to do!

    Subfields _130_subfields(record->getSubfields(_130_index));
    if (not _130_subfields.hasSubfield('a'))
        return; // Nothing to do!

    _040_subfields.setSubfield('e', "rda");
    record->updateField(_040_index, _040_subfields);

    if (not _130_subfields.hasSubfield('p') and _130_subfields.getFirstSubfieldValue('a') != "Bibel") {
        _130_subfields.moveSubfield('a', 'p');
        _130_subfields.setSubfield('a', "Bibel");
        record->updateField(static_cast<size_t>(_130_index), _130_subfields);
    }

    marc_writer->write(*record);

    ++conversion_count;
}


void ConvertBibleRefs(MarcReader * const marc_reader, MarcWriter * const marc_writer) {
    while (MarcRecord record = marc_reader->read())
        ProcessRecord(&record, marc_writer);

    std::cerr << "Converted " << conversion_count << " record(s).\n";
}


void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_input norm_data_output\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (marc_input_filename == marc_output_filename)
        Error("input filename can't equal the output filename!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    try {
        ConvertBibleRefs(marc_reader.get(), marc_writer.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
