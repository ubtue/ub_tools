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


// Return the first matching primary field from authority data
// This implicitly assumes that the correct tag can be uniquely identified from the PPN
MARC::Record::const_iterator GetFirstPrimaryField(const MARC::Record& authority_record) {
     std::vector<std::string> tags_to_check({"100", "151", "150", "110", "111", "130"});
     for (auto tag_to_check : tags_to_check) {
         MARC::Record::const_iterator primary_field(authority_record.findTag(tag_to_check));
         if (primary_field != authority_record.end())
             return primary_field;
     }
     return authority_record.end();
}


bool GetAuthorityRecordFromPPN(std::string bsz_authority_ppn, MARC::Record * const authority_record, MARC::Reader * const authority_reader,
                                       const std::map<std::string, off_t> &authority_offsets) {
    auto authority_offset(authority_offsets.find(bsz_authority_ppn));
    if (authority_offset != authority_offsets.end()) {
        off_t authority_record_offset(authority_offset->second);
        if (authority_reader->seek(authority_record_offset)) {
            *authority_record = authority_reader->read();
            if (authority_record->getControlNumber() != bsz_authority_ppn)
                logger->error("We got a wrong PPN " + authority_record->getControlNumber() +
                              " instead of " + bsz_authority_ppn);
            else
                return true;

        } else
            logger->error("Unable to seek to record for authority PPN " + bsz_authority_ppn);
    } else {
        logger->warning("Unable to find offset for authority PPN " + bsz_authority_ppn);
        return false;
    }
    std::runtime_error("Logical flaw in GetAuthorityRecordFromPPN");
}


void UpdateTitleField(MARC::Record::Field * const field, const MARC::Record authority_record) {
     auto authority_primary_field(GetFirstPrimaryField(authority_record));
     if (authority_primary_field == authority_record.end())
         logger->error("Could not find appropriate Tag for authority PPN " + authority_record.getControlNumber());
     MARC::Subfields subfields(field->getSubfields());
     for (auto &authority_subfield : authority_primary_field->getSubfields()) {
         if (subfields.hasSubfield(authority_subfield.code_))
             subfields.replaceFirstSubfield(authority_subfield.code_, authority_subfield.value_);
         else
             subfields.addSubfield(authority_subfield.code_, authority_subfield.value_);
      }
      field->setContents(subfields, field->getIndicator1(), field->getIndicator2());
}


void AugmentAuthors(MARC::Record * const record, MARC::Reader * const authority_reader, const std::map<std::string, off_t> &authority_offsets,
                    RegexMatcher * const matcher) {
    std::vector<std::string> tags_to_check({"100", "110", "111", "700", "710", "711"});
    for (auto tag_to_check : tags_to_check) {
        for (auto &field : record->getTagRange(tag_to_check)) {
            std::string _author_content(field.getContents());
            if (matcher->matched(_author_content)) {
                MARC::Record authority_record(""/*empty leader*/);
                if (GetAuthorityRecordFromPPN((*matcher)[1], &authority_record, authority_reader, authority_offsets))
                    UpdateTitleField(&field, authority_record);
            }
        }
    }
}


void AugmentKeywords(MARC::Record * const record, MARC::Reader * const authority_reader, const std::map<std::string, off_t> &authority_offsets,
                    RegexMatcher * const matcher) {
    for (auto &field : record->getTagRange("689")) {
        std::string _689_content(field.getContents());
        if (matcher->matched(_689_content)) {
             MARC::Record authority_record(""/*empty leader*/);
             if (GetAuthorityRecordFromPPN((*matcher)[1], &authority_record, authority_reader, authority_offsets))
                 UpdateTitleField(&field, authority_record);
        }
    }
}


void AugmentKeywordsAndAuthors(MARC::Reader * const marc_reader, MARC::Reader * const authority_reader, MARC::Writer * const marc_writer,
                               const std::map<std::string, off_t>& authority_offsets) {
    std::string err_msg;
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\x1F""0\\(DE-576\\)([^\x1F]+).*\x1F?", &err_msg));

    if (matcher == nullptr)
        logger->error("Failed to compile standardized keywords regex matcher: " + err_msg);

    while (MARC::Record record = marc_reader->read()) {
       ++record_count;
       AugmentAuthors(&record, authority_reader, authority_offsets, matcher);
       AugmentKeywords(&record, authority_reader, authority_offsets, matcher);
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

