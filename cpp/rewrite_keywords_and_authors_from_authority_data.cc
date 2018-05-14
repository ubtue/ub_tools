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


const char STANDARDIZED_KEYWORD_TYPE_FIELD('D');
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
     if (type == "p") //Personenschlagwort
         return "100";
     if (type == "g") //Geographikum
         return "151"; 
     if (type == "s") //Sachschlagwort
         return "150";
     if (type == "b") //Körperschaft
         return "110";
     if (type == "f") //Konferenzen
         return "111";
     if (type == "u") //Werktitel
         return "130";
     throw std::runtime_error("Invalid keyword type: " + type);
}

// Return the first matching primary field from authority data
MARC::Record::const_iterator GetFirstPrimaryField(const MARC::Record& authority_record) {
     std::vector<std::string> tags_to_check({"100", "151", "150", "110", "111", "130"});
     for (auto tag_to_check : tags_to_check) {
         MARC::Record::const_iterator primary_field(authority_record.findTag(tag_to_check));
         if (primary_field != authority_record.end()) {
//XXXX
std::cout << "FOUND FIELD: "  << primary_field->toString() << '\n';
             return primary_field;
         }
 
     }
     return authority_record.end();
}


void AugmentKeywordsAndAuthors(MARC::Reader * const marc_reader, MARC::Reader * const authority_reader, MARC::Writer * const marc_writer, 
                               const std::map<std::string, off_t>& authority_offsets) {
    std::string err_msg;
    RegexMatcher * const standardized_keywords_matcher(
        RegexMatcher::RegexMatcherFactory("\x1F""0\\(DE-576\\)([^\x1F]+).*\x1F""2gnd", &err_msg));

    if (standardized_keywords_matcher == nullptr)
        logger->error("Failed to compile standardized keywords regex matcher: " + err_msg);

    while (MARC::Record record = marc_reader->read()) {
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
                        if (type_subfields.size() != 1) {
                            logger->error("Invalid number of subfields for subfield code " + std::string(1,STANDARDIZED_KEYWORD_TYPE_FIELD) + "[" + 
                             field.toString() + "] --- SIZE: " + std::to_string(type_subfields.size()));
                        }
                        // Get the data
                        std::string authority_tag(GetAuthorityTagForType(type_subfields[0]));
//XXX
std::cout << "Authority tag: " << authority_tag << '\n';
                        auto authority_primary_field(authority_record.findTag(authority_tag));
                        if ( authority_primary_field == authority_record.end()) {
                            logger->warning("Could not find Tag " + authority_tag + " in authority record " + bsz_authority_ppn);
                            // Potentially the mapping given in STANDARDIZED_KEYWORD_TYPE_FIELD is inappropriate, so we 
                            // we take the first primary form (Vorlageform) we encounter
                            authority_primary_field = GetFirstPrimaryField(authority_record);
                            if (authority_primary_field == authority_record.end())
                                logger->error("Could not find appropriate Tag for authority PPN " + bsz_authority_ppn);
                        }
                        // Make sure we replace all the relevant subfields
                        MARC::Subfields subfields(field.getSubfields());
                        for (auto &authority_subfield : authority_primary_field->getSubfields()) {
//XXX
std::cout << "Replacing subfield " << authority_subfield.code_  << " with value " << authority_subfield.value_ << '\n';
                            if (subfields.hasSubfield(authority_subfield.code_))
                                subfields.replaceFirstSubfield(authority_subfield.code_, authority_subfield.value_);
                            else
                                subfields.addSubfield(authority_subfield.code_, authority_subfield.value_);
                        }
                        field.setContents(subfields, field.getIndicator1(), field.getIndicator2());
//XXX
std::cout << "NEW CONTENT: " << field.toString() << '\n';
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

