/** \file    augment_canones_references.cc
 *  \brief   A tool for adding numerical canon law references to MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019, Library of the University of Tübingen

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
#include <string>
#include <unordered_map>
#include <cstdlib>
#include <strings.h>
#include "BibleUtil.h"
#include "Compiler.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


// To understand this code read https://github.com/ubtue/tuefind/wiki/Codices
std::string FieldToCanonLawCode(const std::string &ppn, const std::string &subfield_codex, const std::string &subfield_year,
                                const std::string &subfield_part)
{
    enum Codex { CIC1917, CIC1983, CCEO } codex;
    if (::strcasecmp(subfield_codex.c_str(), "Codex canonum ecclesiarum orientalium") == 0)
        codex = CCEO;
    else {
        if (unlikely(subfield_year.empty()))
            LOG_ERROR("missing year for Codex Iuris Canonici! (PPN: " + ppn + ")");
        if (subfield_year == "1917")
            codex = CIC1917;
        else if (subfield_year == "1983")
            codex = CIC1983;
        else
            LOG_ERROR("bad year for Codex Iuris Canonici \"" + subfield_year + "\"! (PPN: " + ppn + ")");
    }

    unsigned range_start, range_end;
    if (subfield_part.empty()) {
        range_start = 0;
        range_end = 99999999;
    } else if (not MiscUtil::ParseCanonLawRanges(subfield_part, &range_start, &range_end))
        LOG_ERROR("don't know how to parse codex parts \"" + subfield_part + "\"! (PPN: " + ppn + ")");

    switch (codex) {
    case CIC1917:
        return StringUtil::ToString(100000000 + range_start) + "_" + StringUtil::ToString(100000000 + range_end);
    case CIC1983:
        return StringUtil::ToString(200000000 + range_start) + "_" + StringUtil::ToString(200000000 + range_end);
    case CCEO:
        return StringUtil::ToString(300000000 + range_start) + "_" + StringUtil::ToString(300000000 + range_end);
    default:
        LOG_ERROR("unknown codex: " + std::to_string(codex));
    }
}


void LoadAuthorityData(MARC::Reader * const reader,
                       std::unordered_map<std::string, std::string> * const authority_ppns_to_canon_law_codes_map)
{
    unsigned total_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        const auto _110_field(record.findTag("110"));
        if (_110_field == record.end() or ::strcasecmp(_110_field->getFirstSubfieldWithCode('a').c_str(), "Katholische Kirche") != 0)
            continue;

        const std::string t_subfield(_110_field->getFirstSubfieldWithCode('t'));
        if (::strcasecmp(t_subfield.c_str(),"Codex Iuris Canonici") != 0
            and ::strcasecmp(t_subfield.c_str(), "Codex canonum ecclesiarum orientalium") != 0)
            continue;

        (*authority_ppns_to_canon_law_codes_map)[record.getControlNumber()] = FieldToCanonLawCode(record.getControlNumber(),
                                                                                                  t_subfield,
                                                                                                  _110_field->getFirstSubfieldWithCode('f'),
                                                                                                  _110_field->getFirstSubfieldWithCode('p'));
    }

    LOG_INFO("found " + std::to_string(authority_ppns_to_canon_law_codes_map->size()) + " canon law records among "
             + std::to_string(total_count) + " authority records.");
}


void CollectAuthorityPPNs(const MARC::Record &record, const MARC::Tag &linking_field, std::vector<std::string> * const authority_ppns) {
    for (const auto &field : record.getTagRange(linking_field)) {
        const MARC::Subfields subfields(field.getSubfields());
        for (const auto &subfield : subfields) {
            if (subfield.code_ == '0' and StringUtil::StartsWith(subfield.value_, "(DE-576)"))
                authority_ppns->emplace_back(subfield.value_.substr(__builtin_strlen("(DE-576)")));
        }
    }
}


void ProcessRecords(MARC::Reader * const reader, MARC::Writer * const writer,
                    const std::unordered_map<std::string, std::string> &authority_ppns_to_canon_law_codes_map)
{
    static const std::vector<std::string> CANONES_GND_LINKING_TAGS{ "689", "655" };

    unsigned total_count(0), augmented_count(0);
    std::map<std::string, unsigned> reference_counts;

    while (auto record = reader->read()) {
        ++total_count;

        bool augmented_record(false);
        for (const auto &linking_tag : CANONES_GND_LINKING_TAGS) {
            std::vector<std::string> authority_ppns;
            CollectAuthorityPPNs(record, linking_tag, &authority_ppns);

            if (not authority_ppns.empty()) {
                for (const auto &authority_ppn : authority_ppns) {
                    const auto ppn_and_canon_law_code(authority_ppns_to_canon_law_codes_map.find(authority_ppn));
                    if (ppn_and_canon_law_code != authority_ppns_to_canon_law_codes_map.cend()) {
                        record.insertField("CAL", { { 'a', ppn_and_canon_law_code->second } });
                        augmented_record = true;
                        ++reference_counts[linking_tag];
                    }
                }
            }
        }

        if (not augmented_record) {
            // check if the codex data is embedded directly in the 689 field
            // apparently, 689$t is repeatable and the first instance (always?) appears to be 'Katholische Kirche'
            std::vector<std::string> ranges_to_insert;
            for (const auto &_689_field : record.getTagRange("689")) {
                if (_689_field.getFirstSubfieldWithCode('a') != "Katholische Kirche")
                    continue;

                std::string subfield_codex, subfield_year, subfield_part;
                for (const auto &subfield : _689_field.getSubfields()) {
                    if (subfield.code_ == 't' and subfield.value_ != "Katholische Kirche")
                        subfield_codex = subfield.value_;
                    else if (subfield.code_ == 'f')
                        subfield_year = subfield.value_;
                    else if (subfield.code_ == 'p')
                        subfield_part = subfield.value_;
                }

                if (not subfield_codex.empty() and not subfield_year.empty() and not subfield_part.empty()) {
                    ranges_to_insert.emplace_back(FieldToCanonLawCode(record.getControlNumber(), subfield_codex, subfield_year,
                                                                      subfield_part));
                    augmented_record = true;
                    ++reference_counts["689*"];
                }
            }

            for (const auto &range : ranges_to_insert)
                record.insertField("CAL", { { 'a', range } });
        }

        if (augmented_record)
            ++augmented_count;

        writer->write(record);
    }

    LOG_INFO("augmented " + std::to_string(augmented_count) + " of " + std::to_string(total_count) + " records.");
    LOG_INFO("found " + std::to_string(reference_counts["689"]) + " references in field 689");
    LOG_INFO("found " + std::to_string(reference_counts["689*"]) + " direct references in field 689");
    LOG_INFO("found " + std::to_string(reference_counts["655"]) + " references in field 655");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        ::Usage("ixtheo_titles authority_records augmented_ixtheo_titles");

    const std::string title_input_filename(argv[1]);
    const std::string authority_filename(argv[2]);
    const std::string title_output_filename(argv[3]);
    if (unlikely(title_input_filename == title_output_filename))
        LOG_ERROR("Title input file name equals title output file name!");
    if (unlikely(title_input_filename == authority_filename))
        LOG_ERROR("Title input file name equals authority file name!");
    if (unlikely(title_output_filename == authority_filename))
        LOG_ERROR("Title output file name equals authority file name!");

    auto authority_reader(MARC::Reader::Factory(authority_filename));
    std::unordered_map<std::string, std::string> authority_ppns_to_canon_law_codes_map;
    LoadAuthorityData(authority_reader.get(), &authority_ppns_to_canon_law_codes_map);

    auto title_reader(MARC::Reader::Factory(title_input_filename));
    auto title_writer(MARC::Writer::Factory(title_output_filename));
    ProcessRecords(title_reader.get(), title_writer.get(), authority_ppns_to_canon_law_codes_map);

    return EXIT_SUCCESS;
}
