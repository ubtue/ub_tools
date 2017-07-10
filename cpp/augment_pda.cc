/** \file    augment_pda.cc
 *  \brief   Tag monographs not available for ILL as PDA
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

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


#include <boost/lexical_cast.hpp>
#include <iostream>
#include <unordered_map>
#include "Compiler.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "util.h"


static unsigned modified_count(0);
static unsigned record_count(0);


const std::string POTENTIALLY_PDA_TAG("192");
const char POTENTIALLY_PDA_SUBFIELD('a');
const int PDA_CUTOFF_YEAR(2014);


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] ill_list marc_input marc_output\n"
              << "       Insert an additional field for monographs published after " << PDA_CUTOFF_YEAR << "\n"
              << "       that are not available in SWB interlibrary loan (show up in the ill_list)\n"
              << "       thus providing a set of candidates for Patron Driven Acquisition (PDA)\n";
    std::exit(EXIT_FAILURE);
}


void ExtractILLPPNs(const bool verbose, const std::unique_ptr<File>& ill_list, std::unordered_set<std::string> * const ill_set) {
    std::string line;
    int retval;
    while (retval = ill_list->getline(&line, '\n')) {
        if (not retval) {
            if (unlikely(ill_list->anErrorOccurred()))
                Error("Error on relaind ill list " + ill_list->getPath());
            if (unlikely(ill_list->eof()))
                return;
        }
        if (verbose)
           std::cerr << "Adding " << line << " to ill set";
        ill_set->emplace(line);
    }
}


void ProcessRecord(const bool verbose, MarcRecord * const record, const std::unordered_set<std::string> * ill_set) {

     // We tag a record as potentially PDA if it is 
     // a) a monograph b) published after a given cutoff date c) not in the list of known SWB-ILLs
     
     if (not record->getLeader().isMonograph())
         return;

     if (record->isElectronicResource())
         return;
     
     const size_t _008_index(record->getFieldIndex("008"));
     if (unlikely(_008_index == MarcRecord::FIELD_NOT_FOUND))
         return;

     // Determine publication sort year given in Bytes 7-10 of field 008
     const std::string &_008_contents(record->getFieldData(_008_index));
     try {
         int publication_year(boost::lexical_cast<int>(_008_contents.substr(7,4)));
         if (publication_year < PDA_CUTOFF_YEAR)
             return;
     } catch (boost::bad_lexical_cast const&) {
         if (verbose) 
             std::cerr << "Could not determine publication year for record " << record->getControlNumber()
                   << " [ " <<  _008_contents.substr(7,4) << " given ]\n";
         return;
     }
     
     if (ill_set->find(record->getControlNumber()) == ill_set->end()) {
         if (record->getFieldIndex(POTENTIALLY_PDA_TAG) != MarcRecord::FIELD_NOT_FOUND)
             Error("Field " + POTENTIALLY_PDA_TAG + " already populated for PPN " + record->getControlNumber());
         record->insertSubfield(POTENTIALLY_PDA_TAG, POTENTIALLY_PDA_SUBFIELD, "1");
         ++modified_count;
     }
}


void TagRelevantRecords(const bool verbose, MarcReader * const marc_reader, MarcWriter * const marc_writer,
                        const std::unordered_set<std::string> * ill_set)
{
    while (MarcRecord record = marc_reader->read()) {
        ProcessRecord(verbose, &record, ill_set);
        marc_writer->write(record);
        ++record_count;
    }
    std::cerr << "Modified " << modified_count << " of " << record_count << " record(s).\n";
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

    if (argc != 4)
        Usage();


    const std::string ill_list_filename(argv[1]);
    const std::string marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Input file equals output file");

    if (unlikely(marc_input_filename == ill_list_filename || marc_output_filename == ill_list_filename))
        Error("ILL File equals marc input or output file");

    try {
        std::unordered_set<std::string> ill_set;
        std::unique_ptr<File> ill_reader(FileUtil::OpenInputFileOrDie(ill_list_filename));
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));

        ExtractILLPPNs(verbose, ill_reader, &ill_set);
        TagRelevantRecords(verbose, marc_reader.get(), marc_writer.get(), &ill_set);

    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}




