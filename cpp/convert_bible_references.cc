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
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


static unsigned conversion_count(0);


// Spits out those records that have a 040$e field which starts with "rak" and has a 130 field.
// In that case it sets 040$e to "rda", move the contents of 130$a to 130$p and sets 130$a
// to "Bibel".
void ProcessRecord(XmlWriter * const xml_writer, MarcUtil::Record * const record) {
    const ssize_t _040_index(record->getFieldIndex("040"));
    if (_040_index == -1)
	return; // Nothing to do!

    const ssize_t _130_index(record->getFieldIndex("130"));
    if (_130_index == -1)
	return; // Nothing to do!

    const std::vector<std::string> field_data(record->getFields());
    Subfields _040_subfields(field_data[_040_index]);
    if (not StringUtil::StartsWith(_040_subfields.getFirstSubfieldValue('e'), "rak"))
	return; // Nothing to do!

    Subfields _130_subfields(field_data[_130_index]);
    if (not _130_subfields.hasSubfield('a'))
	return; // Nothing to do!

    record->setRecordWillBeWrittenAsXml(true);

    _040_subfields.setSubfield('e', "rda");
    record->updateField(static_cast<size_t>(_040_index), _040_subfields.toString());

    if (not (_130_subfields.hasSubfield('p') and _130_subfields.getFirstSubfieldValue('a') == "Bibel")) {
	if (not _130_subfields.moveSubfield('a', 'p'))
	    return;
	_130_subfields.setSubfield('a', "Bibel");
	record->updateField(static_cast<size_t>(_130_index), _130_subfields.toString());
    }

    record->write(xml_writer);

    ++conversion_count;
}


void ConvertBibleRefs(File * const input, File * const output) {
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input))
	ProcessRecord(&xml_writer, &record);

    xml_writer.closeTag();

    std::cerr << "Converted " << conversion_count << " record(s).\n";
}


void Usage() {
    std::cerr << "Usage: " << progname << " norm_data_input norm_data_output\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    File marc_input(marc_input_filename, "rm");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[2]);
    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
	ConvertBibleRefs(&marc_input, &marc_output);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
