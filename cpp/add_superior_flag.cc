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
#include "MarcXmlWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


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
        if (not record->insertField("SPR", superior_subfield_data))
            Warning("Not enough room to add a SPR field! (Control number: " + field_data[0] + ")");
        else
            ++modified_count;
    }

    record->write(xml_writer);
}


void AddSuperiorFlag(File * const input, File * const output) {
    MarcXmlWriter xml_writer(output);

    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input))
        ProcessRecord(&xml_writer, &record);

    std::cerr << "Modified " << modified_count << " record(s).\n";
}


void LoadSuperiorPPNs(File * const input) {
    unsigned line_no(0);
    std::string line;
    while (input->getline(&line)) {
        ++line_no;
        superior_ppns.emplace(line);
    }

    if (unlikely(superior_ppns.empty()))
        Error("Found no data in \"" + input->getPath() + "\"!");
    std::cerr << "Read " << line_no << " superior PPNs.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[2]);
    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    const std::string superior_ppns_filename(argv[3]);
    File superior_ppns_input(superior_ppns_filename, "r");
    if (not superior_ppns_input)
        Error("can't open \"" + superior_ppns_filename + "\" for reading!");

    try {
        LoadSuperiorPPNs(&superior_ppns_input);

        Subfields superior_subfield(/* indicator1 = */' ', /* indicator2 = */' ');
        superior_subfield.addSubfield('a', "1"); // Could be anything but we can't have an empty field.
        superior_subfield_data = superior_subfield.toString();

        AddSuperiorFlag(&marc_input, &marc_output);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
