/** \file    time_aspects_to_codes_tool.cc
 *  \brief   A tool for converting time aspect references to numeric codes.
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
#include <cstdlib>
#include "RangeUtil.h"
#include "StringUtil.h"
#include "util.h"


int Main(int argc, char **argv) {
    if (argc != 2)
        ::Usage("time_aspect_reference_candidate");

    const std::string time_aspect_reference_candidate(StringUtil::TrimWhite(argv[1]));

    std::string range;
    if (not RangeUtil::ConvertTextToTimeRange(time_aspect_reference_candidate, &range))
        LOG_ERROR("can't convert \"" + time_aspect_reference_candidate + "\" to a time aspect range!");

    std::cout << range << '\n';

    return EXIT_SUCCESS;
}
