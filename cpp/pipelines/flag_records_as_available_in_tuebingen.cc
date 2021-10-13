/** \file flag_records_as_available_in_tuebingen.cc
 *  \author Johannes Riedl
 *  \author Dr. Johannes Ruscheinski
 *
 *  Adds an ITA field with a $a subfield set to "1", if a record represents an object that is
 *  available in Tübingen. Adds a further subfield $t set to $1 if criteria for the Tübinger Aufsatz Dienst (TAD)
 *  match (currently sigil $DE-21 exclusively).
 */

/*
    Copyright (C) 2017-2020, Library of the University of Tübingen

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

#include <algorithm>
#include <iostream>
#include <cstring>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


void ProcessSuperiorRecord(const MARC::Record &record,
                           RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppns,
                           RegexMatcher * const tad_sigil_matcher, std::unordered_set<std::string> * const tad_superior_ppns,
                           unsigned * const extracted_count, unsigned * const extracted_tad_count)
{
    // We are done if this is not a superior work
    if (not record.hasFieldWithTag("SPR"))
        return;

    auto local_block_starts(record.findStartOfAllLocalDataBlocks());

    for (const auto &local_block_start : local_block_starts) {
        for (const auto &_852_field : record.findFieldsInLocalBlock("852", local_block_start)) {
            std::string sigil;
            if (_852_field.extractSubfieldWithPattern('a', *tue_sigil_matcher, &sigil)) {
                de21_superior_ppns->insert(record.getControlNumber());
                ++*extracted_count;
            }
            if (_852_field.extractSubfieldWithPattern('a', *tad_sigil_matcher, &sigil)) {
                tad_superior_ppns->insert(record.getControlNumber());
                ++*extracted_tad_count;
            }
        }
    }
}


void LoadDE21AndTADPPNs(MARC::Reader * const marc_reader,
                        RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppns,
                        RegexMatcher * const tad_sigil_matcher, std::unordered_set<std::string> * const tad_superior_ppns,
                        unsigned * const extracted_count, unsigned * const extracted_tad_count)
{
    while (const MARC::Record record = marc_reader->read())
         ProcessSuperiorRecord(record, tue_sigil_matcher, de21_superior_ppns, tad_sigil_matcher, tad_superior_ppns,
                               extracted_count, extracted_tad_count) ;

    LOG_DEBUG("Finished extracting " + std::to_string(*extracted_count) + " superior records and " +
               std::to_string(*extracted_tad_count) + " TAD superior records");
}


void FlagRecordAsInTuebingenAvailable(MARC::Record * const record, const bool tad_available, unsigned * const modified_count) {
    tad_available ? record->insertField("ITA", {  { 'a', "1" }, { 't', "1" } }) : record->insertField("ITA", 'a', "1");
    ++*modified_count;
}


bool AlreadyHasLOK852DE21(const MARC::Record &record, RegexMatcher * const tue_sigil_matcher) {
    const auto local_block_starts(record.findStartOfAllLocalDataBlocks());
    if (local_block_starts.empty())
        return false;

    for (const auto local_block_start : local_block_starts) {
        for (const auto &_852_field : record.findFieldsInLocalBlock("852", local_block_start)) {
            std::string sigil;
            if (_852_field.extractSubfieldWithPattern('a', *tue_sigil_matcher, &sigil))
                 return true;
        }
    }
    return false;
}


void ProcessRecord(MARC::Record * const record, MARC::Writer * const marc_writer,
                   RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppns,
                   RegexMatcher * const tad_sigil_matcher, std::unordered_set<std::string> * const tad_superior_ppns,
                   unsigned * const modified_count)
{
    if (AlreadyHasLOK852DE21(*record, tue_sigil_matcher)) {
        const bool tad_available(AlreadyHasLOK852DE21(*record, tad_sigil_matcher));
        FlagRecordAsInTuebingenAvailable(record, tad_available, modified_count);
        marc_writer->write(*record);
        return;
    }

    if (not record->isArticle()) {
        marc_writer->write(*record);
        return;
    }

    auto superior_ppn_set(record->getSuperiorControlNumbers(/* additional_tags=*/ { "776" }));
    // Do we have superior PPN that has DE-21
    auto current_superior_iterator(std::begin(superior_ppn_set));
    for (const auto &superior_ppn : superior_ppn_set) {
        if (de21_superior_ppns->find(superior_ppn) != de21_superior_ppns->end()) {
            // Make sure we do not return prematurely
            // If the current superior PPN does not match TAD criteria search all other candidates
            if (tad_superior_ppns->find(superior_ppn) != tad_superior_ppns->end())
                FlagRecordAsInTuebingenAvailable(record, true /* tad_available */, modified_count);
            else {
                for (/* intentionally empty */; current_superior_iterator != superior_ppn_set.end(); ++current_superior_iterator) {
                    if (tad_superior_ppns->find(*current_superior_iterator) != tad_superior_ppns->end())
                        FlagRecordAsInTuebingenAvailable(record, true /* tad_available */, modified_count);
                }
            }
            marc_writer->write(*record);
            return;
        }
        ++current_superior_iterator;
    }
    marc_writer->write(*record);
}


void AugmentRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppn,
                    RegexMatcher * const tad_sigil_matcher, std::unordered_set<std::string> * const tad_superior_ppn,
                    unsigned * const extracted_count, unsigned * const extracted_tad_count,
                    unsigned * const modified_count)
{
    marc_reader->rewind();
    while (MARC::Record record = marc_reader->read())
        ProcessRecord(&record, marc_writer, tue_sigil_matcher, de21_superior_ppn,
                      tad_sigil_matcher, tad_superior_ppn, modified_count);
    LOG_INFO("Extracted " + std::to_string(*extracted_count) + " superior PPNs with DE-21, "
             + std::to_string(*extracted_tad_count) + " superior PPNs as TAD candidates and modified "
             + std::to_string(*modified_count) + " records");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("spr_augmented_marc_input marc_output\n"
                " Notice that this program requires the SPR tag for superior works\n"
                " to be set for appropriate results\n\n");

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));

    unsigned extracted_count(0), extracted_tad_count(0), modified_count(0);
    const std::unique_ptr<RegexMatcher> tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));
    const std::unique_ptr<RegexMatcher> tad_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21$"));
    std::unordered_set<std::string> de21_superior_ppns;
    std::unordered_set<std::string> tad_superior_ppns;

    LoadDE21AndTADPPNs(marc_reader.get(), tue_sigil_matcher.get(), &de21_superior_ppns,
                       tad_sigil_matcher.get(), &tad_superior_ppns,
                       &extracted_count, &extracted_tad_count);
    AugmentRecords(marc_reader.get(), marc_writer.get(), tue_sigil_matcher.get(), &de21_superior_ppns,
                   tad_sigil_matcher.get(), &tad_superior_ppns,
                   &extracted_count, &extracted_tad_count, &modified_count);

    return EXIT_SUCCESS;
}
