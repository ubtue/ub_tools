/** \file    rewrite_keywords_and_authors_from_authority_data
 *  \brief   Update fields with references to authority data with potentially
             more current authority data
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018, Library of the University of Tübingen

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
#include <map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const char STANDARDIZED_KEYWORD_TYPE_FIELD('d');
static unsigned int record_count;

namespace {

void Usage() __attribute__((noreturn));

void Usage() {
    std::cerr << "Usage: " << ::progname << " master_marc_input authority_data_marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}

// Create a list of PPNs and File Offsets
void CreateAuthorityOffsets(MARC::Reader * const authority_reader, std::map<std::string, off_t> * const authority_offsets) {
   off_t record_offset(authority_reader->tell());
   while (const MARC::Record record = authority_reader->read()) {
      authority_offsets->emplace(record.getControlNumber(), record_offset);
      // Shift to next record
      record_offset = authority_reader->tell();
   }
}

std::string GetAuthorityTagForType(const std::string &type){
     if (type == "u")
         return "110";
     //FIXME MORE VALUES
     return "0";
}



void AugmentKeywordsAndAuthors(MARC::Reader * const marc_reader, MARC::Reader * const authority_reader, MARC::Writer * const marc_writer, 
                               const std::map<std::string, off_t>& authority_offsets) {
    std::string err_msg;
    RegexMatcher * const standardized_keywords_matcher(
        RegexMatcher::RegexMatcherFactory("(\x1F""0\\(DE-576)([^\x1F]+\\).*\x1F""2gnd", &err_msg));

    if (standardized_keywords_matcher == nullptr)
        logger->error("Failed to compile standardized keywords regex matcher" + err_msg);

    while (const MARC::Record record = marc_reader->read()) {
       ++record_count;
       // FIXME: Add author handling

       // Augment standardized keywords
        for (auto &field : record.getTagRange("689")) {
            std::string _689_content(field.getContents());
            if (standardized_keywords_matcher->matched(_689_content)) {
                std::string bsz_authority_ppn((*standardized_keywords_matcher)[1]);
                auto authority_offset(authority_offsets.find(bsz_authority_ppn));
                if (authority_offset != authority_offsets.end()) {
                    off_t authority_record_offset(authority_offset->second);
                    if (authority_reader->seek(authority_record_offset)) {
                        const MARC::Record authority_record(authority_reader->read());
                        if (authority_record.getControlNumber() != bsz_authority_ppn)
                            logger->error("We got a wrong PPN " + authority_record.getControlNumber() + 
                                          " instead of " + bsz_authority_ppn);

                        std::vector<std::string> type_subfields(field.getSubfields().extractSubfields(STANDARDIZED_KEYWORD_TYPE_FIELD));
                        if (type_subfields.size() != 1)
                            logger->error("More than one type subfield for subfield code " + std::string(1,STANDARDIZED_KEYWORD_TYPE_FIELD));
                        // Get the data
                        std::string authority_tag(GetAuthorityTagForType(type_subfields[0]));
                        auto authority_primary_field(authority_record.findTag(authority_tag));
                        if ( authority_primary_field == authority_record.end())
                            logger->error("Could not find Tag " + authority_tag + " in authority record " + bsz_authority_ppn);
                        // Make sure we replace all the relevant subfields
                        for (auto &authority_subfield : authority_primary_field->getSubfields())
                            field.getSubfields().replaceFirstSubfield(authority_subfield.code_, authority_subfield.value_);
                    } else {
                        logger->error("Unable to seek to record for authority PPN " + bsz_authority_ppn);
                    }
                } else {
                    logger->error("Unable to find offset for authority PPN " + bsz_authority_ppn);
                }
            }
        }
        marc_writer->write(record);
    }
}

}

int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string authority_data_marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        logger->error("Title data input file name equals output file name!");
    if (unlikely(authority_data_marc_input_filename == marc_output_filename))
        logger->error("Authority data input file name equals output file name!");


    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename, MARC::Reader::BINARY));
    std::unique_ptr<MARC::Reader> authority_reader(MARC::Reader::Factory(authority_data_marc_input_filename,
                                                                     MARC::Reader::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::Writer::BINARY));
    std::map<std::string, off_t> authority_offsets;

    try {
        CreateAuthorityOffsets(authority_reader.get(), &authority_offsets); 
        AugmentKeywordsAndAuthors(marc_reader.get(), authority_reader.get(), marc_writer.get(), authority_offsets);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }

    return 0;
}

