/** \file    augment_time_aspects.cc
 *  \brief   A tool for adding normalised time references to MARC-21 datasets.
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

#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


static const unsigned OFFSET(10000000);


// \return the current day as a range endpoint
inline std::string Now() {
    unsigned year, month, day;
    TimeUtil::GetCurrentDate(&year, &month, &day);
    return StringUtil::ToString(year + OFFSET, /* radix = */10, /* width = */8, /* padding_char = */'0')
           +  StringUtil::ToString(month, /* radix = */10, /* width = */2, /* padding_char = */'0')
           +  StringUtil::ToString(day, /* radix = */10, /* width = */2, /* padding_char = */'0');
}


bool ConvertTextToRange(const std::string &text, std::string * const range) {
    static auto matcher1(RegexMatcher::RegexMatcherFactoryOrDie("(\\d{3,4})-(\\d{3,4})"));
    if (matcher1->matched(text)) {
        const unsigned year1(StringUtil::ToUnsigned((*matcher1)[1]));
        const unsigned year2(StringUtil::ToUnsigned((*matcher1)[2]));
        *range = StringUtil::ToString(year1 + OFFSET, /* radix = */10, /* width = */8, /* padding_char = */'0') + "0101_"
                 + StringUtil::ToString(year2 + OFFSET, /* radix = */10, /* width = */8, /* padding_char = */'0') + "1231";
        return true;
    }

    static auto matcher2(RegexMatcher::RegexMatcherFactoryOrDie("(\\d\\d\\d\\d)-"));
    if (matcher2->matched(text)) {
        const unsigned year(StringUtil::ToUnsigned((*matcher2)[1]));
        *range = StringUtil::ToString(year + OFFSET, /* radix = */10, /* width = */8, /* padding_char = */'0') + "0101_" + Now();
        return true;
    }

    static auto matcher3(RegexMatcher::RegexMatcherFactoryOrDie("(\\d{2,4})(?: v\\. ?Chr\\.)?-(\\d{2,4}) v\\. ?Chr\\."));
    if (matcher3->matched(text)) {
        const unsigned year1(StringUtil::ToUnsigned((*matcher3)[1]));
        const unsigned year2(StringUtil::ToUnsigned((*matcher3)[2]));
        *range = StringUtil::ToString(OFFSET - year1, /* radix = */10, /* width = */8, /* padding_char = */'0') + "0101_"
                 + StringUtil::ToString(OFFSET - year2, /* radix = */10, /* width = */8, /* padding_char = */'0') + "1231";
        return true;
    }

    return false;
}


void LoadAuthorityData(MARC::Reader * const reader,
                       std::unordered_map<std::string, std::string> * const authority_ppns_to_time_codes_map)
{
    unsigned total_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        const auto _548_field(record.findTag("548"));
        if (_548_field != record.end() and _548_field->hasSubfieldWithValue('i', "Zeitraum")) {
            const std::string free_form_range_candidate(_548_field->getFirstSubfieldWithCode('a'));
            std::string range;
            if (ConvertTextToRange(free_form_range_candidate, &range))
                (*authority_ppns_to_time_codes_map)[record.getControlNumber()] = range;
            else
                LOG_WARNING("can't convert \"" + free_form_range_candidate + "\" to a time range!");
        }
    }

    LOG_INFO("found " + std::to_string(authority_ppns_to_time_codes_map->size()) + " time aspect records among "
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


inline std::vector<std::string>::const_iterator FindFirstPrefixMatch(const std::string &s, const std::vector<std::string> &prefixes) {
    for (auto prefix(prefixes.cbegin()); prefix != prefixes.cend(); ++prefix) {
        if (StringUtil::StartsWith(s, *prefix))
            return prefix;
    }

    return prefixes.cend();
}


void ProcessRecords(MARC::Reader * const reader, MARC::Writer * const writer,
                    const std::unordered_map<std::string, std::string> &authority_ppns_to_time_codes_map)
{
    static const std::vector<std::string> TIME_ASPECT_GND_LINKING_TAGS{ "689" };
    static const std::vector<std::string> _689_PREFIXES{ "Geschichte ", "Geistesgeschichte ", "Ideengeschichte ", "Kirchengeschichte ",
                                                         "Sozialgeschichte ", "Vor- und Frühgeschichte ", "Weltgeschichte ", "Prognose " };

    unsigned total_count(0), augmented_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        std::string range;
        for (const std::string &tag : TIME_ASPECT_GND_LINKING_TAGS) {
            for (const auto &time_aspect_field : record.getTagRange(tag)) {
                const auto a_subfield(time_aspect_field.getFirstSubfieldWithCode('a'));
                const auto matched_prefix(FindFirstPrefixMatch(a_subfield, _689_PREFIXES));
                if (matched_prefix == _689_PREFIXES.cend())
                    continue;

                if (ConvertTextToRange(a_subfield.substr(matched_prefix->length()), &range))
                    goto augment_record;
            }

            std::vector<std::string> authority_ppns;
            CollectAuthorityPPNs(record, tag, &authority_ppns);
            for (const auto authority_ppn : authority_ppns) {
                const auto authority_ppn_and_time_code(authority_ppns_to_time_codes_map.find(authority_ppn));
                if (authority_ppn_and_time_code != authority_ppns_to_time_codes_map.cend()) {
                    range = authority_ppn_and_time_code->second;
                    goto augment_record;
                }
            }
        }

augment_record:
        if (not range.empty()) {
            record.insertField("TIM", { { 'a', range } });
            ++augmented_count;
        }

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
    std::unordered_map<std::string, std::string> authority_ppns_to_time_codes_map;
    LoadAuthorityData(authority_reader.get(), &authority_ppns_to_time_codes_map);

    auto title_reader(MARC::Reader::Factory(title_input_filename));
    auto title_writer(MARC::Writer::Factory(title_output_filename));
    ProcessRecords(title_reader.get(), title_writer.get(), authority_ppns_to_time_codes_map);

    return EXIT_SUCCESS;
}
