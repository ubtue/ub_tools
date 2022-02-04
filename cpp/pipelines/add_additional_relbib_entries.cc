/** \file    add_additional_relbib_entries.cc
 *  \author  Johannes Riedl
 *
 *  A tool for tagging entries that are not yet officially part of
 *  the set of relbib titles but were indentified to be relevant
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

#include <iostream>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


static unsigned modified_count(0);
static unsigned record_count(0);


const std::string RELBIB_RELEVANT_IDS_FILENAME("/usr/local/ub_tools/cpp/data/relbib_auto_list.txt");
const std::string RELBIB_RELEVANT_TAG("191");
const char RELBIB_SUBFIELD('a');


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n"
              << "       Tags entries that are not yet officially part of the set of titles relevant for relbib\n"
              << "       but have been identified to be probably relevant.\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecord(MARC::Record * const record, const std::unordered_set<std::string> &relbib_relevant_set) {
    if (relbib_relevant_set.find(record->getControlNumber()) != relbib_relevant_set.end()) {
        if (record->findTag(RELBIB_RELEVANT_TAG) != record->end())
            LOG_ERROR("Field " + RELBIB_RELEVANT_TAG + " already populated for PPN " + record->getControlNumber());
        record->insertField(RELBIB_RELEVANT_TAG, { { RELBIB_SUBFIELD, "1" } });
        ++modified_count;
    }
}


void TagRelevantRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                        const std::unordered_set<std::string> &relbib_relevant_set) {
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(&record, relbib_relevant_set);
        marc_writer->write(record);
        ++record_count;
    }

    std::cout << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


void SetupRelBibRelevantSet(std::unordered_set<std::string> * const relbib_relevant_set) {
    std::unique_ptr<File> relbib_relevant(FileUtil::OpenInputFileOrDie(RELBIB_RELEVANT_IDS_FILENAME));
    std::string line;
    int retval;
    while ((retval = relbib_relevant->getline(&line, '\n'))) {
        if (not retval) {
            if (unlikely(relbib_relevant->anErrorOccurred()))
                LOG_ERROR("Error on reading in relbib relevant file " + relbib_relevant->getPath());
            if (unlikely(relbib_relevant->eof()))
                return;
            continue;
        }
        relbib_relevant_set->emplace(line);
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");

    auto marc_reader(MARC::Reader::Factory(marc_input_filename));
    auto marc_writer(MARC::Writer::Factory(marc_output_filename));

    std::unordered_set<std::string> relbib_relevant_set;
    SetupRelBibRelevantSet(&relbib_relevant_set);
    TagRelevantRecords(marc_reader.get(), marc_writer.get(), relbib_relevant_set);

    return EXIT_SUCCESS;
}
