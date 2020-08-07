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


void InitialiseCodesToKeywordChainsMap(std::unordered_map<std::string, std::string> * const codes_to_keyword_chains_map) {
    const auto MAP_FILENAME(UBTools::GetTuelibPath() + "DIN_ISO_3166_geographic_codes_in_German");
    std::unordered_map<std::string, std::string> codes_to_keywords_map;
    unsigned line_no(0);
    constexpr char KEYWORD_SEPARATOR('/');
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

    unsigned level(0);
    while (codes_to_keyword_chains_map->size() < codes_to_keywords_map.size()) {
        for (const auto &[codes, keyword] : codes_to_keywords_map) {
            if (StringUtil::CharCount(keyword, '-') != level)
                continue;

            if (level == 0)
                (*codes_to_keyword_chains_map)[codes] = keyword;
            else {
                const auto LAST_DASH_POS(codes.rfind('-'));
                const auto code_prefix(codes.substr(0, LAST_DASH_POS));
                const auto code_and_keyword_prefix(codes_to_keyword_chains_map->find(code_prefix));
                if (unlikely(code_and_keyword_prefix == codes_to_keyword_chains_map->end()))
                    LOG_ERROR("code prefix \"" + code_prefix + "\" is missing!");
                (*codes_to_keyword_chains_map)[codes] = code_and_keyword_prefix->second + std::string(1, KEYWORD_SEPARATOR) + keyword;
            }
        }
        ++level;
    }
}


void GenerateExpandedGeographicCodes(MARC::Reader * const reader, MARC::Writer * writer,
                                     const std::unordered_map<std::string, std::string> &codes_to_keyword_chains_map)
{
    unsigned total_count(0), conversion_count(0);
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
            LOG_ERROR("record w/ PPN " + record.getControlNumber() + " contains missing code \""
                      + codes + "\" in 044$c!");

        record.insertField("GEO", 'a', codes_and_keyword_chain->second);
        ++conversion_count;
        writer->write(record);
    }

    LOG_INFO("Processed " + std::to_string(total_count) + " record(s) and converted "
             + std::to_string(conversion_count) + " codes to keyword chains.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input marc_output");

    std::unordered_map<std::string, std::string> codes_to_keyword_chains_map;
    InitialiseCodesToKeywordChainsMap(&codes_to_keyword_chains_map);

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto marc_writer(MARC::Writer::Factory(argv[2]));
    GenerateExpandedGeographicCodes(marc_reader.get(), marc_writer.get(), codes_to_keyword_chains_map);

    return EXIT_SUCCESS;
}
