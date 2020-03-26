/** \file flag_records_as_available_in_tuebingen.cc
 *  \author Johannes Riedl
 *  \author Dr. Johannes Ruscheinski
 *
 *  Adds an ITA field with a $a subfield set to "1", if a record represents an object that is
 *  available in Tübingen.
 */

/*
    Copyright (C) 2017-2019, Library of the University of Tübingen

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


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " spr_augmented_marc_input marc_output\n";
    std::cerr << "  Notice that this program requires the SPR tag for superior works\n";
    std::cerr << "  to be set for appropriate results\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessSuperiorRecord(const MARC::Record &record, RegexMatcher * const tue_sigil_matcher,
                           std::unordered_set<std::string> * const de21_superior_ppns, unsigned * const extracted_count)
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
        }
    }
}


void LoadDE21PPNs(MARC::Reader * const marc_reader, RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppns,
                  unsigned * const extracted_count)
{
    while (const MARC::Record record = marc_reader->read())
         ProcessSuperiorRecord(record, tue_sigil_matcher, de21_superior_ppns, extracted_count);

    LOG_DEBUG("Finished extracting " + std::to_string(*extracted_count) + " superior records");
}


void CollectSuperiorPPNs(const MARC::Record &record, std::unordered_set<std::string> * const superior_ppn_set) {
    static RegexMatcher * const superior_ppn_matcher(RegexMatcher::RegexMatcherFactory(".DE-627.(.*)"));
    static const std::vector<std::string> tags{ "800", "810", "830", "773", "776" };

    for (const auto &tag : tags) {
        const auto field(record.findTag(tag));
        if (field != record.end()) {
            // Remove superfluous prefixes
            for (auto &superior_ppn : field->getSubfields().extractSubfields('w')) {
                std::string err_msg;
                if (not superior_ppn_matcher->matched(superior_ppn, &err_msg)) {
                    if (not err_msg.empty())
                        LOG_ERROR("Error with regex für superior works " + err_msg);
                    continue;
                }
                superior_ppn_set->insert((*superior_ppn_matcher)[1]);
            }
        }
    }
}


void FlagRecordAsInTuebingenAvailable(MARC::Record * const record, unsigned * const modified_count) {
    record->insertField("ITA", { { 'a', "1" } });
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


void ProcessRecord(MARC::Record * const record, MARC::Writer * const marc_writer, RegexMatcher * const tue_sigil_matcher,
                   std::unordered_set<std::string> * const de21_superior_ppns, unsigned * const modified_count) {
    if (AlreadyHasLOK852DE21(*record, tue_sigil_matcher)) {
        FlagRecordAsInTuebingenAvailable(record, modified_count);
        marc_writer->write(*record);
        return;
    }

    if (not record->isArticle()) {
        marc_writer->write(*record);
        return;
    }

    std::unordered_set<std::string> superior_ppn_set;
    CollectSuperiorPPNs(*record, &superior_ppn_set);
    // Do we have superior PPN that has DE-21
    for (const auto &superior_ppn : superior_ppn_set) {
        if (de21_superior_ppns->find(superior_ppn) != de21_superior_ppns->end()) {
            FlagRecordAsInTuebingenAvailable(record, modified_count);
            marc_writer->write(*record);
            return;
        }
    }
    marc_writer->write(*record);
}


void AugmentRecords(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    RegexMatcher * const tue_sigil_matcher, std::unordered_set<std::string> * const de21_superior_ppn,
                    unsigned * const extracted_count, unsigned * const modified_count) {
    marc_reader->rewind();
    while (MARC::Record record = marc_reader->read())
        ProcessRecord(&record, marc_writer, tue_sigil_matcher, de21_superior_ppn, modified_count);
    LOG_INFO("Extracted " + std::to_string(*extracted_count) + " superior PPNs with DE-21 and modified "
             + std::to_string(*modified_count) + " records");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    const std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));

    unsigned extracted_count(0), modified_count(0);
    const std::unique_ptr<RegexMatcher> tue_sigil_matcher(RegexMatcher::RegexMatcherFactory("^DE-21.*"));
    std::unordered_set<std::string> de21_superior_ppns;

    LoadDE21PPNs(marc_reader.get(), tue_sigil_matcher.get(), &de21_superior_ppns, &extracted_count);
    AugmentRecords(marc_reader.get(), marc_writer.get(), tue_sigil_matcher.get(), &de21_superior_ppns, &extracted_count, &modified_count);

    return EXIT_SUCCESS;
}
