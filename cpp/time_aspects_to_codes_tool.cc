/** \file    time_aspects_to_codes_tool.cc
 *  \brief   A tool for converting time aspect references to numeric codes.
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
    ::Usage("[--date-query] time_aspect_reference_candidate");
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

    const std::string time_aspect_reference_candidate(StringUtil::TrimWhite(argv[1]));

    if (generate_date_query)
        std::cout << RangeUtil::ConvertToDatesQuery(StringUtil::Map(time_aspect_reference_candidate, '_', ':')) << '\n';
    else {
        std::string range;
        if (not RangeUtil::ConvertTextToTimeRange(time_aspect_reference_candidate, &range))
            LOG_ERROR("can't convert \"" + time_aspect_reference_candidate + "\" to a time aspect range!");
        std::cout << range << '\n';
    }

    return EXIT_SUCCESS;
}
