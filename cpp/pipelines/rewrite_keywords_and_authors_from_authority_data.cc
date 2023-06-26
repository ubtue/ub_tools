/** \file    rewrite_keywords_and_authors_from_authority_data
 *  \brief   Update fields with references to authority data with potentially
             more current authority data
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018-2023 Library of the University of Tübingen

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
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " master_marc_input authority_data_marc_input.mrc marc_output\n"
              << "The Authority data must be in the MARC-21 binary format.\n";
    std::exit(EXIT_FAILURE);
}


// Return the first matching primary field (Vorzugsbenennung) from authority data
// This implicitly assumes that the correct tag can be uniquely identified from the PPN
MARC::Record::const_iterator GetFirstPrimaryField(const MARC::Record &authority_record) {
    static const std::vector<std::string> tags_to_check{ "100", "151", "150", "110", "111", "130", "153" };
    for (const auto &tag_to_check : tags_to_check) {
        MARC::Record::const_iterator primary_field(authority_record.findTag(tag_to_check));
        if (primary_field != authority_record.end())
            return primary_field;
    }
    return authority_record.end();
}


bool GetAuthorityRecordFromPPN(const std::string &bsz_authority_ppn, MARC::Record * const authority_record,
                               MARC::Reader * const authority_reader, const std::unordered_map<std::string, off_t> &authority_offsets,
                               const MARC::Record &record) {
    auto authority_offset(authority_offsets.find(bsz_authority_ppn));
    if (authority_offset != authority_offsets.end()) {
        off_t authority_record_offset(authority_offset->second);
        if (authority_reader->seek(authority_record_offset)) {
            *authority_record = authority_reader->read();
            if (authority_record->getControlNumber() != bsz_authority_ppn)
                LOG_ERROR("We got a wrong PPN " + authority_record->getControlNumber() + " instead of " + bsz_authority_ppn);
            else
                return true;
        } else
            LOG_ERROR("Unable to seek to record for authority PPN " + bsz_authority_ppn + " referenced in title PPN "
                      + record.getControlNumber());
    } else {
        LOG_WARNING("Unable to find offset for authority PPN " + bsz_authority_ppn + " referenced in title PPN "
                    + record.getControlNumber());
        return false;
    }

    LOG_ERROR("Logical flaw in GetAuthorityRecordFromPPN: we should *never* get here!");
}


bool IsWorkTitleField(const MARC::Subfields &subfields) {
    return subfields.hasSubfieldWithValue('D', "u");
}


bool UpdateTitleDataField(MARC::Record::Field * const field, const MARC::Record authority_record) {
    auto authority_primary_field(GetFirstPrimaryField(authority_record));
    if (authority_primary_field == authority_record.end()) {
        LOG_WARNING("Could not find appropriate Tag for authority PPN " + authority_record.getControlNumber());
        return false;
    }
    MARC::Subfields subfields(field->getSubfields());
    // We have to make sure that the order of the subfields is inherited from the authority data
    // so delete the subfields to be replaced first
    // Moreover there is a special case with "Werktitel". These are in $a
    // in the authority data but must be mapped to $t in the title data
    for (const auto &authority_subfield : authority_primary_field->getSubfields()) {
        if (IsWorkTitleField(subfields) and authority_subfield.code_ == 'a')
            subfields.deleteAllSubfieldsWithCode('t');
        else
            subfields.deleteAllSubfieldsWithCode(authority_subfield.code_);
    }
    for (const auto &authority_subfield : authority_primary_field->getSubfields()) {
        if (IsWorkTitleField(subfields) and authority_subfield.code_ == 'a')
            subfields.appendSubfield('t', authority_subfield.value_);
        else
            subfields.appendSubfield(authority_subfield.code_, authority_subfield.value_);
    }
    field->setSubfields(subfields);
    return true;
}


void AugmentAuthors(MARC::Record * const record, MARC::Reader * const authority_reader,
                    const std::unordered_map<std::string, off_t> &authority_offsets, RegexMatcher * const matcher,
                    bool * const modified_record) {
    static std::vector<std::string> tags_to_check{ "100", "110", "111", "700", "710", "711" };
    for (auto tag_to_check : tags_to_check) {
        for (auto &field : record->getTagRange(tag_to_check)) {
            std::string _author_content(field.getContents());
            if (matcher->matched(_author_content)) {
                MARC::Record authority_record(std::string(MARC::Record::LEADER_LENGTH, ' '));
                if (GetAuthorityRecordFromPPN((*matcher)[1], &authority_record, authority_reader, authority_offsets, *record)) {
                    if (UpdateTitleDataField(&field, authority_record))
                        *modified_record = true;
                }
            }
        }
    }
}


void AugmentKeywords(MARC::Record * const record, MARC::Reader * const authority_reader,
                     const std::unordered_map<std::string, off_t> &authority_offsets, RegexMatcher * const matcher,
                     bool * const modified_record) {
    for (auto &field : record->getTagRange("689")) {
        std::string _689_content(field.getContents());
        if (matcher->matched(_689_content)) {
            MARC::Record authority_record(std::string(MARC::Record::LEADER_LENGTH, ' '));
            if (GetAuthorityRecordFromPPN((*matcher)[1], &authority_record, authority_reader, authority_offsets, *record)) {
                UpdateTitleDataField(&field, authority_record);
                *modified_record = true;
            }
        }
    }
}

// Despite the special handling of "Werktitel" we still have inconsistenties and side effect because
// for specific cases $a and $t have identical content - remove $t entirely in this cases
void DeduplicateIdenticalAAndTSubfieldsInStandardizedKeywords(MARC::Record * const record, bool * const modified_record) {
    for (auto &field : record->getTagRange("689")) {
        auto _689_subfields(field.getSubfields());
        const std::string subfieldAContent(_689_subfields.getFirstSubfieldWithCode('a'));
        if (not subfieldAContent.empty() and _689_subfields.hasSubfield('t')) {
            _689_subfields.deleteAllSubfieldsWithCodeMatching('t', ThreadSafeRegexMatcher(RegexMatcher::Escape(subfieldAContent)));
            *modified_record = true;
        }
    }
}


void AugmentKeywordsAndAuthors(MARC::Reader * const marc_reader, MARC::Reader * const authority_reader, MARC::Writer * const marc_writer,
                               const std::unordered_map<std::string, off_t> &authority_offsets) {
    std::string err_msg;
    RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory("\x1F"
                                          "0\\(DE-627\\)([^\x1F]+).*\x1F?",
                                          &err_msg));

    if (matcher == nullptr)
        LOG_ERROR("Failed to compile standardized keywords regex matcher: " + err_msg);


    unsigned record_count(0), modified_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++record_count;
        bool modified_record(false);
        AugmentAuthors(&record, authority_reader, authority_offsets, matcher, &modified_record);
        AugmentKeywords(&record, authority_reader, authority_offsets, matcher, &modified_record);
        DeduplicateIdenticalAAndTSubfieldsInStandardizedKeywords(&record, &modified_record);
        if (modified_record)
            ++modified_count;
        marc_writer->write(record);
    }

    LOG_INFO("Modified " + std::to_string(modified_count) + " of " + std::to_string(record_count) + " records.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string authority_data_marc_input_filename(argv[2]);
    const std::string marc_output_filename(argv[3]);
    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Title data input file name equals output file name!");
    if (unlikely(authority_data_marc_input_filename == marc_output_filename))
        LOG_ERROR("Authority data input file name equals output file name!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unique_ptr<MARC::Reader> authority_reader(MARC::Reader::Factory(authority_data_marc_input_filename, MARC::FileType::BINARY));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));
    std::unordered_map<std::string, off_t> authority_offsets;

    MARC::CollectRecordOffsets(authority_reader.get(), &authority_offsets);
    AugmentKeywordsAndAuthors(marc_reader.get(), authority_reader.get(), marc_writer.get(), authority_offsets);

    return EXIT_SUCCESS;
}
