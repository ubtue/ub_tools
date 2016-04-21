/** \file    add_superior_flag.cc
 *  \author  Oliver Obenland
 *
 *  A tool for marking superior records that have associated inferior records in our data sets.
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
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "File.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "XmlWriter.h"


static unsigned modified_count(0);
static std::set<std::string> superior_ppns;
static std::string superior_subfield_data;


void Usage() {
    std::cerr << "Usage: " << progname << " marc_input marc_output superior_ppns\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecord(XmlWriter * const xml_writer, MarcUtil::Record * const record) {
    record->setRecordWillBeWrittenAsXml(true);

    // Don't add the flag twice
    if (record->getFieldIndex("SPR") != -1) {
        record->write(xml_writer);
        return;
    }

    const std::vector<std::string> &field_data(record->getFields());
    const auto iter(superior_ppns.find(field_data.at(0)));
    if (iter != superior_ppns.end()) {
        if (not record->insertField("UBR", superior_subfield_data)) {
            Warning("Not enough room to add a SPR field! (Control number: " + field_data[0] + ")");
        }
	++modified_count;
    }

    record->write(xml_writer);
}


void AddSuperiorFlag(File * const input, File * const output) {
    XmlWriter xml_writer(output);
    xml_writer.openTag("marc:collection",
                       { std::make_pair("xmlns:marc", "http://www.loc.gov/MARC21/slim"),
                         std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                         std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd")});

    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input))
	ProcessRecord(&xml_writer, &record);

    xml_writer.closeTag();

    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void LoadSuperiorPPNs(const std::string &child_refs_filename) {
    std::ifstream child_refs(child_refs_filename.c_str());
    if (not child_refs.is_open())
        Error("Failed to open \"" + child_refs_filename + "\" for reading!");

    std::string line;
    unsigned line_no(0);
    while (std::getline(child_refs, line)) {
        ++line_no;
        superior_ppns.emplace(line);
    }

    if (unlikely(superior_ppns.empty()))
        Error("Found no data in \"" + child_refs_filename + "\"!");
    std::cerr << "Read " << line_no << " superior PPNs.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
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
        LoadSuperiorPPNs(argv[3]);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }

    Subfields superior_subfield(/* indicator1 = */' ', /* indicator2 = */' ');
    superior_subfield.addSubfield('a', "1");
    superior_subfield_data = superior_subfield.toString();

    AddSuperiorFlag(&marc_input, &marc_output);
}
