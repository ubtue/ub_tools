/** \file    convert_bible_references.cc
 *  \brief   Tool for RDA conversion of bible reference norm data.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2018, Library of the University of Tübingen

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
#include "MARC.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " norm_data_input norm_data_output\n";
    std::exit(EXIT_FAILURE);
}


// Spits out those records that have a 040$e field which starts with "rak" and has a 130 field.
// In that case it sets 040$e to "rda", move the contents of 130$a to 130$p and sets 130$a
// to "Bibel".
bool ConvertRecord(MARC::Record * const record, MARC::Writer * const marc_writer) {
    auto _040_field(record->getFirstField("040"));
    if (_040_field == record->end())
        return false; // Nothing to do!

    auto _130_field(record->getFirstField("130"));
    if (_130_field == record->end())
        return false; // Nothing to do!

    MARC::Subfields _040_subfields(_040_field->getSubfields());
    if (not StringUtil::StartsWith(_040_subfields.getFirstSubfieldWithCode('e'), "rak"))
        return false; // Nothing to do!

    MARC::Subfields _130_subfields(_130_field->getSubfields());
    if (not _130_subfields.hasSubfield('a'))
        return false; // Nothing to do!

    _040_subfields.addSubfield('e', "rda");
    _040_field->setContents(_040_subfields, _040_field->getIndicator1(), _040_field->getIndicator2());

    if (not _130_subfields.hasSubfield('p') and _130_subfields.getFirstSubfieldWithCode('a') != "Bibel") {
        _130_subfields.moveSubfield('a', 'p');
        _130_subfields.addSubfield('a', "Bibel");
        _130_field->setContents(_130_subfields, _130_field->getIndicator1(), _130_field->getIndicator2());
    }

    marc_writer->write(*record);

    return true;
}


void ConvertBibleRefs(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned conversion_count(0);
    while (MARC::Record record = marc_reader->read()) {
        if (ConvertRecord(&record, marc_writer))
            ++conversion_count;
    }

    LOG_INFO("Converted " + std::to_string(conversion_count) + " record(s).");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (marc_input_filename == marc_output_filename)
        LOG_ERROR("input filename can't equal the output filename!");

    auto marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::FileType::BINARY));
    auto marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY));

    ConvertBibleRefs(marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
