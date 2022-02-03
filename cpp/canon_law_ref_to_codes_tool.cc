/** \file    canon_law_ref_to_codes_tool.cc
 *  \brief   A tool for converting canon law references to numeric codes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019 Library of the University of Tübingen

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
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "MapUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--debug|--query] canon_law_reference_candidate\n"
        "When --debug has been specified additional tracing output will be generated.\n"
        "When --query has been specified SOLR search queries will be output.\n"
        "If --map-file-directory=... has been specified, the provided path will be prefixed to all\n"
        "map-file arguments and, if the map arguments are left off, the default names will be used.");
}


void HandleAliases(const bool generate_solr_query, const std::string &canon_law_reference_candidate) {
    LOG_DEBUG("Entering HandleAliases().");

    std::unordered_multimap<std::string, std::string> aliases_to_codes_map;
    MapUtil::DeserialiseMap(UBTools::GetTuelibPath() + "canon_law_aliases.map", &aliases_to_codes_map);

    LOG_DEBUG("looking for \"" + canon_law_reference_candidate + "\".");
    const auto begin_end(aliases_to_codes_map.equal_range(canon_law_reference_candidate));
    if (begin_end.first != begin_end.second) {
        LOG_DEBUG("Found an alias to a code mapping.");
        std::string query;
        for (auto pair(begin_end.first); pair != begin_end.second; ++pair) {
            if (generate_solr_query) {
                if (not query.empty())
                    query += ' ';
                query += StringUtil::Map(pair->second, ':', '_');
            } else
                std::cout << pair->second << '\n';
        }
        if (generate_solr_query)
            std::cout << query << '\n';

        std::exit(EXIT_SUCCESS);
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    bool generate_solr_query(false);
    if (std::strcmp(argv[1], "--debug") == 0) {
        logger->setMinimumLogLevel(Logger::LL_DEBUG);
        ++argv, --argc;
    } else if (std::strcmp(argv[1], "--query") == 0) {
        generate_solr_query = true;
        ++argv, --argc;
    }

    if (argc != 2)
        Usage();

    std::string query(argv[1]);
    TextUtil::NormaliseDashes(&query);
    query = StringUtil::Trim(TextUtil::CollapseWhitespace(TextUtil::UTF8ToLower(query)));

    HandleAliases(generate_solr_query, query);

    const auto first_space_pos(query.find(' '));
    const std::string codex(TextUtil::UTF8ToUpper(first_space_pos == std::string::npos ? query : query.substr(0, first_space_pos)));
    if (codex != "CIC17" and codex != "CIC83" and codex != "CCEO")
        LOG_ERROR("bad codex \"" + codex + "\", must be one of \"CIC17\", \"CIC83\" or \"CCEO\"!");
    unsigned upper, lower;
    if (codex == "CIC17")
        upper = lower = 100000000;
    else if (codex == "CIC83")
        upper = lower = 200000000;
    else
        upper = lower = 300000000;

    // In order to understand the following mess, have a look at https://github.com/ubtue/tuefind/wiki/Codices
    if (first_space_pos == std::string::npos)
        upper += 99999999;
    else {
        const std::string rest(query.substr(first_space_pos + 1));
        const auto first_dash_pos(rest.find('-'));
        const auto first_comma_pos(rest.find(','));

        if (first_comma_pos != std::string::npos) {
            unsigned paragraph;
            if (not StringUtil::ToUnsigned(rest.substr(0, first_comma_pos), &paragraph) or paragraph > 9999)
                LOG_ERROR("invalid paragraph in \"" + rest + "\"!");

            const auto dash_pos(rest.find('-', first_comma_pos + 1));
            const auto second_comma_pos(rest.find(',', first_comma_pos + 1));
            if (dash_pos != std::string::npos) { // Example: CIC83 790,1-2
                unsigned part1;
                if (not StringUtil::ToUnsigned(rest.substr(first_comma_pos + 1, dash_pos - first_comma_pos - 1), &part1) or part1 > 99)
                    LOG_ERROR("can't convert \"" + rest + "\" to a valid reference! (1)");

                unsigned part2;
                if (not StringUtil::ToUnsigned(rest.substr(dash_pos + 1), &part2) or part2 > 99)
                    LOG_ERROR("can't convert \"" + rest + "\" to a valid reference! (2)");

                lower += paragraph * 10000 + part1 * 100;
                upper += paragraph * 10000 + part2 * 100 + 99;
            } else if (second_comma_pos != std::string::npos) { // Example: CIC83 1044,2,2
                unsigned part1;
                if (not StringUtil::ToUnsigned(rest.substr(first_comma_pos + 1, second_comma_pos - first_comma_pos - 1), &part1)
                    or part1 > 99)
                    LOG_ERROR("can't convert \"" + rest + "\" to a valid reference! (3)");

                unsigned part2;
                if (not StringUtil::ToUnsigned(rest.substr(second_comma_pos + 1), &part2) or part2 > 99)
                    LOG_ERROR("can't convert \"" + rest + "\" to a valid reference! (4)");

                lower += paragraph * 10000 + part1 * 100 + part2;
                upper += paragraph * 10000 + part1 * 100 + part2;
            } else { // Example: CCEO 1087,2
                unsigned part;
                if (not StringUtil::ToUnsigned(rest.substr(first_comma_pos + 1), &part) or part > 99)
                    LOG_ERROR("can't convert \"" + rest + "\" to a valid reference! (5)");

                lower += paragraph * 10000 + part * 100;
                upper += paragraph * 10000 + part * 100 + 99;
            }
        } else if (first_dash_pos != std::string::npos) {
            unsigned range_start;
            if (not StringUtil::ToUnsigned(rest.substr(0, first_dash_pos), &range_start) or range_start > 9999)
                LOG_ERROR("bad range start in \"" + rest + "\"!");
            unsigned range_end;
            if (not StringUtil::ToUnsigned(rest.substr(first_dash_pos + 1), &range_end) or range_end > 9999)
                LOG_ERROR("bad range end in \"" + rest + "\"!");
            if (range_start >= range_end)
                LOG_ERROR("invalid range \"" + rest + "\"!");

            lower += range_start * 10000;
            upper += range_end * 10000 + 9999;
        } else {
            unsigned paragraph;
            if (not StringUtil::ToUnsigned(rest, &paragraph) or paragraph == 0 or paragraph > 9999)
                LOG_ERROR("can't convert \"" + rest + "\" to a valid paragraph!");

            lower += paragraph * 10000;
            upper += paragraph * 10000 + 9999;
        }
    }

    std::cout << lower << (generate_solr_query ? '_' : ':') << upper << '\n';

    return EXIT_SUCCESS;
}
