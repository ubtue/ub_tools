/** \file    expand_DIN_ISO_3166_geographic_codes.cc
 *  \author  Dr Johannes Ruscheinski
 *
 *  Converts codes stored in MARC field 044 and generates geographic fully-spelled-out
 *  keyword chains in MARC field GEO
 */

/*
    Copyright (C) 2020, Library of the University of Tübingen

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
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


constexpr char KEYWORD_SEPARATOR('/');


void InitialiseCodesToKeywordChainsMap(std::unordered_map<std::string, std::string> * const codes_to_keyword_chains_map) {
    const auto MAP_FILENAME(UBTools::GetTuelibPath() + "DIN_ISO_3166_geographic_codes_in_German");
    std::unordered_map<std::string, std::string> codes_to_keywords_map;
    unsigned line_no(0);
    for (auto line : FileUtil::ReadLines(MAP_FILENAME)) {
        ++line_no;
        if (unlikely(line.empty()))
            continue;

        const auto FIRST_PIPE_POS(line.find('|'));
        auto keyword(line.substr(0, FIRST_PIPE_POS));
        const auto codes(line.substr(FIRST_PIPE_POS + 1));
        if (unlikely(keyword.empty() or codes.empty()))
            LOG_ERROR("malformed line #" + std::to_string(line_no) + " in \"" + MAP_FILENAME + "\"!");

        codes_to_keywords_map[codes] = StringUtil::Map(&keyword, KEYWORD_SEPARATOR, ';'); // The mapping is probably unnecessary.
    }
    LOG_INFO("Extracted " + std::to_string(codes_to_keywords_map.size()) + " mappings from \"" + MAP_FILENAME + "\".");

    unsigned level(0);
    while (codes_to_keyword_chains_map->size() < codes_to_keywords_map.size()) {
        for (const auto &[codes, keyword] : codes_to_keywords_map) {
            if (StringUtil::CharCount(codes, '-') != level)
                continue;

            if (level == 0)
                (*codes_to_keyword_chains_map)[codes] = keyword;
            else {
                const auto LAST_DASH_POS(codes.rfind('-'));
                const auto code_prefix(codes.substr(0, LAST_DASH_POS));
                const auto code_and_keyword_prefix(codes_to_keyword_chains_map->find(code_prefix));
                if (unlikely(code_and_keyword_prefix == codes_to_keyword_chains_map->end()))
                    LOG_ERROR("code prefix \"" + code_prefix + "\" needed for \"" + codes + "\" is missing!");
                (*codes_to_keyword_chains_map)[codes] = code_and_keyword_prefix->second + std::string(1, KEYWORD_SEPARATOR) + keyword;
            }
        }
        ++level;
    }
}


std::string ExtractSubfield(const std::string &line, const size_t subfield_contents_start_pos) {
    const auto next_dollar_pos(line.find('$', subfield_contents_start_pos));
    if (next_dollar_pos == std::string::npos)
        return line.substr(subfield_contents_start_pos);
    return line.substr(subfield_contents_start_pos, next_dollar_pos - subfield_contents_start_pos);
}


constexpr char MARC_SUBFIELD_SEPARATOR('\x1F');


void InitialiseLocationTo689ContentsMap(std::unordered_map<std::string, std::string> * const locations_to_689_contents_map) {
    const auto FIELD_CONTENTS_FILENAME(UBTools::GetTuelibPath() + "geographic_689_field_contents");

    unsigned line_no(0);
    for (auto line : FileUtil::ReadLines(FIELD_CONTENTS_FILENAME)) {
        ++line_no;
        if (unlikely(line.empty()))
            continue;

        // Primary location
        const auto dollar_a_pos(line.find("$a"));
        if (unlikely(dollar_a_pos == std::string::npos))
            continue;
        std::string location(ExtractSubfield(line, dollar_a_pos + 2));
        if (unlikely(location == "Deutsches Reich"))
            location = "Deutschland <Deutsches Reich>";
        else if (unlikely(location == "Trentino-Südtirol"))
            location = "Italien (Südtirol-Trentino s.dort)";
        else if (unlikely(StringUtil::StartsWith(location, "Kanton ")))
            location = location.substr(__builtin_strlen("Kanton ")) + " <Kanton>";
        else {
            // Optional secondary location
            const auto dollar_g_pos(line.find("$g", dollar_a_pos + 2));
            if (dollar_g_pos != std::string::npos)
                location += " <" + ExtractSubfield(line, dollar_g_pos + 2) + ">";
        }

        if (unlikely(location == "Südafrika"))
            location = "Südafrika <Staat>";
        else if (unlikely(location == "Österreich"))
            (*locations_to_689_contents_map)["Österreich (-12.11.1918)"] = StringUtil::Map(&line, '$', MARC_SUBFIELD_SEPARATOR);
        else if (unlikely(location == "Föderative Republik Jugoslawien"))
            location = "Jugoslawien <Föderative Republik> <Jugoslawien>";
        else if (unlikely(location == "El Salvador"))
            location = "ElSalvador";
        else if (unlikely(location == "Demokratische Republik Kongo"))
            location = "Kongo <Republik>";

        (*locations_to_689_contents_map)[location] = StringUtil::Map(&line, '$', MARC_SUBFIELD_SEPARATOR);
    }

    LOG_INFO("Loaded " + std::to_string(locations_to_689_contents_map->size()) + " mappings from location names to 689 field contents.");
}


// Given "Europa/Deutschland/Baden-Württemberg" this would return "Baden-Württemberg".
std::string GetMostSpecificGeographicLocation(const std::string &geo_keyword_chain) {
    const auto last_keyword_separator_pos(geo_keyword_chain.rfind(KEYWORD_SEPARATOR));
    if (last_keyword_separator_pos == std::string::npos) // Not a chain, but an individual keyword!
        return geo_keyword_chain;
    return geo_keyword_chain.substr(last_keyword_separator_pos + 1);
}


std::string &NormaliseLocation(std::string * const location) {
    const auto comma_space_pos(location->find(", "));
    if (comma_space_pos != std::string::npos) {
        const auto auxillary_location(location->substr(comma_space_pos + 2));
        location->resize(comma_space_pos);
        *location += " <";
        *location += auxillary_location;
        *location += '>';
    }

    return *location;
}


std::string ExtractGeoKeyword(const MARC::Record::Field &_689_field) {
    if (_689_field.getFirstSubfieldWithCode('d') != "g" and _689_field.getFirstSubfieldWithCode('q') != "g")
        return "";

    std::string geo_keyword(_689_field.getFirstSubfieldWithCode('a'));
    const auto subfield_g_contents(_689_field.getFirstSubfieldWithCode('g'));
    if (subfield_g_contents.empty())
        return geo_keyword;
    return geo_keyword + " <" + subfield_g_contents + ">";
}


// Returns true if we added "new_689_contents" in a new 689 field, else false.
bool Add689GeographicKeywordIfMissing(MARC::Record * const record, const std::string &new_689_contents) {
    const MARC::Record::Field new_689_field(MARC::Tag("689"), new_689_contents + "\x1F""dg");
    const std::string new_geo_keyword(ExtractGeoKeyword(new_689_field));
    for (const auto &_689_field : record->getTagRange("689")) {
        if (ExtractGeoKeyword(_689_field) == new_geo_keyword)
            return false; // New geographic keyword is not needed because we already have it!
    }

    record->insertField(new_689_field);
    return true;
}


void GenerateExpandedGeographicCodes(MARC::Reader * const reader, MARC::Writer * writer,
                                     const std::unordered_map<std::string, std::string> &codes_to_keyword_chains_map,
                                     const std::unordered_map<std::string, std::string> &locations_to_689_contents_map)
{
    unsigned total_count(0), conversion_count(0), _689_addition_count(0);
    while (auto record = reader->read()) {
        ++total_count;

        const auto _044_field(record.findTag("044"));
        if (_044_field == record.end()) {
            writer->write(record);
            continue;
        }

        const auto codes(_044_field->getFirstSubfieldWithCode('c'));
        if (codes.empty()) {
            writer->write(record);
            continue;
        }

        const auto codes_and_keyword_chain(codes_to_keyword_chains_map.find(codes));
        if (unlikely(codes_and_keyword_chain == codes_to_keyword_chains_map.cend()))
            LOG_WARNING("record w/ PPN " + record.getControlNumber() + " contains missing code \""
                        + codes + "\" in 044$c!");
        else {
            auto most_specific_location(GetMostSpecificGeographicLocation(codes_and_keyword_chain->second));
            NormaliseLocation(&most_specific_location);
            const auto most_specific_location_and_689_contents(locations_to_689_contents_map.find(most_specific_location));
            if (unlikely(most_specific_location_and_689_contents == locations_to_689_contents_map.cend()))
                LOG_WARNING("did not find \"" + most_specific_location + "\" in the locations to 689-contents map!");
            else if (Add689GeographicKeywordIfMissing(&record, most_specific_location_and_689_contents->second))
                ++_689_addition_count;

            record.insertField("GEO", 'a', codes_and_keyword_chain->second);
            ++conversion_count;
        }

        writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " record(s), converted "
             + std::to_string(conversion_count) + " code(s) to keyword chains and added "
             + std::to_string(_689_addition_count) + " new 689 normalised keyword(s).");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    std::unordered_map<std::string, std::string> codes_to_keyword_chains_map;
    InitialiseCodesToKeywordChainsMap(&codes_to_keyword_chains_map);

    std::unordered_map<std::string, std::string> locations_to_689_contents_map;
    InitialiseLocationTo689ContentsMap(&locations_to_689_contents_map);

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    GenerateExpandedGeographicCodes(marc_reader.get(), marc_writer.get(), codes_to_keyword_chains_map,
                                    locations_to_689_contents_map);

    return EXIT_SUCCESS;
}
