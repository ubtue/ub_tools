/** \file    add_additional_relbib_entries.cc
 *  \author  Johannes Riedl
 *
 *  A tool for tagging entries that are not yet officially part of
 *  the set of relbib titles but were indentified to be relevant
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
#include "FileUtil.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"

static unsigned modified_count(0);
static unsigned record_count(0);

const std::string relbib_relevant_ids_filename = "/usr/local/ub_tools/cpp/data/relbib_auto_list.txt";
const std::string relbib_relevant_tag = "191";
const char relbib_subfield = 'a';


void Usage() {
    std::cerr << "Usage: " << progname << " marc_input marc_output\n"
              << "       Tag entries that are not yet officially part of the set of titles relevant for relbib\n"
              << "       but have been identified to be probably relevant.\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecord(MarcRecord * const record, const std::unordered_set<std::string> &relbib_relevant_set) {
     if (relbib_relevant_set.find(record->getControlNumber()) != relbib_relevant_set.end()){
         if (record->getFieldIndex(relbib_relevant_tag) != MarcRecord::FIELD_NOT_FOUND)
             Error("Field " + relbib_relevant_tag + " already populated for PPN " + record->getControlNumber());
         record->insertSubfield(relbib_relevant_tag, relbib_subfield, "1");
         ++modified_count;
     }
}


void TagRelevantRecords(File * const marc_input, File * const marc_output, std::unordered_set<std::string> &relbib_relevant_set) {
    while (MarcRecord record = MarcReader::Read(marc_input)) {
        ProcessRecord(&record, relbib_relevant_set);
        MarcWriter::Write(record, marc_output);
        ++record_count;
    }
    std::cerr << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


void SetupRelBibRelevantSet(std::unordered_set<std::string> &relbib_relevant_set) {
    std::unique_ptr<File> relbib_relevant(FileUtil::OpenInputFileOrDie(relbib_relevant_ids_filename));
    std::string line;
    int retval;
    while (retval = relbib_relevant->getline(&line, '\n')){
        if (!retval) {
            if (relbib_relevant->anErrorOccurred())
                Error("Error on reading in relbib relevant file " + relbib_relevant->getPath());
            if (relbib_relevant->eof()) {
                relbib_relevant->close();
                return;                             
            }
            continue;
        }
        relbib_relevant_set.emplace(line);
    }
    
}

int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    std::unique_ptr<File> marc_input(FileUtil::OpenInputFileOrDie(marc_input_filename));
  
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Title data input file name equals output file name!");

    File marc_output(marc_output_filename, "w");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
        std::unordered_set<std::string> relbib_relevant_set;
        SetupRelBibRelevantSet(relbib_relevant_set);
        TagRelevantRecords(marc_input.get(), &marc_output, relbib_relevant_set);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}




