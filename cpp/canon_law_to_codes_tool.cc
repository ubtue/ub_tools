/** \file    canon_law_to_codes_tool.cc
 *  \brief   A tool for converting canon law references to numeric codes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019-2020 Library of the University of Tübingen

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
#include <cstdlib>
#include "RangeUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--date-query] canon_law_reference_candidate");
}


} // namespace


int Main(int argc, char **argv) {
    if (argc != 2 and argc != 3)
        Usage();

    bool generate_date_query(false);
    if (argc == 3) {
        if (__builtin_strcmp(argv[1], "--date-query") != 0)
            Usage();
        generate_date_query = true;
        --argc, ++argv;
    }

    const std::string canon_law_reference_candidate(StringUtil::TrimWhite(argv[1]));
    std::string range;

    enum Codex { CIC1917, CIC1983, CCEO } codex;
    if (StringUtil::StartsWith(canon_law_reference_candidate, "CCEO", /* ignore_case = */ true)) {
        codex = CCEO;
        range = StringUtil::TrimWhite(canon_law_reference_candidate.substr(__builtin_strlen("CCEO")));
    } else if (StringUtil::StartsWith(canon_law_reference_candidate, "CIC1917", /* ignore_case = */ true)) {
        codex = CIC1917;
        range = StringUtil::TrimWhite(canon_law_reference_candidate.substr(__builtin_strlen("CIC1917")));
    } else if (StringUtil::StartsWith(canon_law_reference_candidate, "CIC1983", /* ignore_case = */ true)) {
        codex = CIC1983;
        range = StringUtil::TrimWhite(canon_law_reference_candidate.substr(__builtin_strlen("CIC1983")));
    } else
        LOG_ERROR("can't determine codes!");

    unsigned range_start, range_end;
    if (range.empty()) {
        range_start = 0;
        range_end = 99999999;
    } else if (not RangeUtil::ParseCanonLawRanges(range, &range_start, &range_end))
        LOG_ERROR("don't know how to parse codex parts \"" + range + "\"!");

    const std::string separator(generate_date_query ? ":" : "_");
    std::string query;
    switch (codex) {
    case CIC1917:
        query = StringUtil::ToString(100000000 + range_start) + separator + StringUtil::ToString(100000000 + range_end);
        break;
    case CIC1983:
        query = StringUtil::ToString(200000000 + range_start) + separator + StringUtil::ToString(200000000 + range_end);
        break;
    case CCEO:
        query = StringUtil::ToString(300000000 + range_start) + separator + StringUtil::ToString(300000000 + range_end);
        break;
    }
    std::cout << (generate_date_query ? RangeUtil::ConvertToDatesQuery(query) : query) << '\n';

    return EXIT_SUCCESS;
}
