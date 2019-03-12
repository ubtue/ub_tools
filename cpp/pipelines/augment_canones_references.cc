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
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


void ParseRanges(const std::string &ppn, const std::string &ranges, unsigned * const range_start, unsigned * const range_end) {
    static RegexMatcher *matcher1(RegexMatcher::RegexMatcherFactoryOrDie("(\\d+),(\\d+),(\\d+)"));
    if (matcher1->matched(ranges)) {
        const unsigned part1(StringUtil::ToUnsigned((*matcher1)[1]));
        if (unlikely(part1 == 0 or part1 >= 10000))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        const unsigned part2(StringUtil::ToUnsigned((*matcher1)[2]));
        if (unlikely(part2 == 0 or part2 >= 100))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        const unsigned part3(StringUtil::ToUnsigned((*matcher1)[3]));
        if (unlikely(part3 == 0 or part3 >= 100))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        *range_start = *range_end = part1 * 10000 + part2 * 100 + part3;
        return;
    }

    unsigned canones;
    if (StringUtil::ToUnsigned(ranges, &canones)) {
        if (unlikely(canones == 0 or canones >= 10000))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        *range_start = canones * 10000;
        *range_end   = canones * 10000 + 9999;
        return;
    }

    static RegexMatcher *matcher2(RegexMatcher::RegexMatcherFactoryOrDie("(\\d+)-(\\d+)"));
    if (matcher2->matched(ranges)) {
        const unsigned canones1(StringUtil::ToUnsigned((*matcher2)[1]));
        if (unlikely(canones1 == 0 or canones1 >= 10000))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        const unsigned canones2(StringUtil::ToUnsigned((*matcher2)[2]));
        if (unlikely(canones2 == 0 or canones2 >= 10000))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        *range_start = canones1 * 10000;
        *range_end   = canones2 * 10000 + 9999;
        return;
    }

    static RegexMatcher *matcher3(RegexMatcher::RegexMatcherFactoryOrDie("(\\d+),(\\d+)"));
    if (matcher3->matched(ranges)) {
        const unsigned part1(StringUtil::ToUnsigned((*matcher3)[1]));
        if (unlikely(part1 == 0 or part1 >= 10000))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        const unsigned part2(StringUtil::ToUnsigned((*matcher3)[2]));
        if (unlikely(part2 == 0 or part2 >= 100))
            LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");

        *range_start = *range_end = part1 * 10000 + part2 * 100 + 99;
        return;
    }

    LOG_ERROR("don't know how to parse codex parts \"" + ranges + "\"! (PPN: " + ppn + ")");
}


// To understand this code read https://github.com/ubtue/tuefind/wiki/Codices
std::string FieldToCanonLawCode(const std::string &ppn, const MARC::Record::Field &_110_field) {
    const std::string t_subfield(_110_field.getFirstSubfieldWithCode('t'));
    const std::string year(_110_field.getFirstSubfieldWithCode('f'));
    const std::string p_subfield(_110_field.getFirstSubfieldWithCode('p'));

    enum Codex { CIC1917, CIC1983, CCEO } codex;
    if (::strcasecmp(t_subfield.c_str(), "Codex canonum ecclesiarum orientalium") == 0)
        codex = CCEO;
    else {
        if (unlikely(year.empty()))
            LOG_ERROR("missing year for Codex Iuris Canonici!");
        if (year == "1917")
            codex = CIC1917;
        else if (year == "1983")
            codex = CIC1983;
        else
            LOG_ERROR("bad year for Codex Iuris Canonici \"" + year + "\"!");
    }

    unsigned range_start, range_end;
    if (p_subfield.empty()) {
        range_start = 0;
        range_end = 99999999;
    } else
        ParseRanges(ppn, p_subfield, &range_start, &range_end);

    switch (codex) {
    case CIC1917:
        return StringUtil::ToString(100000000 + range_start) + "_" + StringUtil::ToString(100000000 + range_end);
    case CIC1983:
        return StringUtil::ToString(200000000 + range_start) + "_" + StringUtil::ToString(200000000 + range_end);
    case CCEO:
        return StringUtil::ToString(300000000 + range_start) + "_" + StringUtil::ToString(300000000 + range_end);
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

        (*authority_ppns_to_canon_law_codes_map)[record.getControlNumber()] = FieldToCanonLawCode(record.getControlNumber(), *_110_field);
    }

    LOG_INFO("found " + std::to_string(authority_ppns_to_canon_law_codes_map->size()) + " canon law records among "
             + std::to_string(total_count) + " authority records.");
}


void ProcessRecords(MARC::Reader * const reader, MARC::Writer * const writer,
                    const std::unordered_map<std::string, std::string> &authority_ppns_to_canon_law_codes_map)
{
    unsigned total_count(0), augmented_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        bool augmented_record(false);
        for (const auto &_689_field : record.getTagRange("689")) {
            if (_689_field.getFirstSubfieldWithCode('2') != "gnd")
                continue;

            std::vector<std::string> authority_ppns;
            for (const auto &subfield : _689_field.getSubfields()) {
                if (subfield.code_ == '0' and StringUtil::StartsWith(subfield.value_, "(DE-576)"))
                    authority_ppns.emplace_back(subfield.value_.substr(__builtin_strlen("(DE-576)")));
            }

            for (const auto &authority_ppn : authority_ppns) {
                const auto ppn_and_canon_law_code(authority_ppns_to_canon_law_codes_map.find(authority_ppn));
                if (ppn_and_canon_law_code != authority_ppns_to_canon_law_codes_map.cend()) {
                    record.insertField("CAL", { { 'a', ppn_and_canon_law_code->second } });
                    augmented_record = true;
                }
            }
        }
        if (augmented_record)
            ++augmented_count;

        writer->write(record);
    }

    LOG_INFO("augmented " + std::to_string(augmented_count) + " of " + std::to_string(total_count) + " records.");
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
