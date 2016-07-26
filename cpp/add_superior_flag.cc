/** \file    add_superior_flag.cc
 *  \author  Oliver Obenland
 *  \author  Dr. Johannes Ruscheinski
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
#include "Compiler.h"
#include "DirectoryEntry.h"
#include "FileUtil.h"
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
    std::cerr << "Usage: " << ::progname << " marc_input marc_output superior_ppns\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecord(XmlWriter * const xml_writer, MarcUtil::Record * const record) {
    record->setRecordWillBeWrittenAsXml(true);

    // Don't add the flag twice
    if (record->getFieldIndex("SPR") != -1) {
        record->write(xml_writer);
        return;
    }

    const auto iter(superior_ppns.find(record->getControlNumber()));
    if (iter != superior_ppns.end()) {
        if (unlikely(not record->insertField("SPR", superior_subfield_data)))
            Warning("Not enough room to add an SPR field! (Control number: " + record->getControlNumber() + ")");
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
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::unique_ptr<File> marc_input(FileUtil::OpenInputFileOrDie(argv[1]));
    const std::unique_ptr<File> marc_output(FileUtil::OpenOutputFileOrDie(argv[2]));
    const std::unique_ptr<File> superior_ppn_input(FileUtil::OpenInputFileOrDie(argv[3]));

    try {
        LoadSuperiorPPNs(superior_ppn_input.get());

        Subfields superior_subfield(/* indicator1 = */' ', /* indicator2 = */' ');
        superior_subfield.addSubfield('a', "1"); // Could be anything but we can't have an empty field.
        superior_subfield_data = superior_subfield.toString();

        AddSuperiorFlag(marc_input.get(), marc_output.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
