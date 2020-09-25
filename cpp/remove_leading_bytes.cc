/** \file    remove_leading_bytes.cc
 *  \brief   In-place removal of leading bytes of a file.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020 Library of the University of Tübingen

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

#include <cstdlib>
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("filename no_of_bytes_to_remove");

    uint64_t no_of_bytes_to_remove;
    if (not StringUtil::ToUInt64T(argv[2], &no_of_bytes_to_remove))
        LOG_ERROR("can't convert \"" + std::string(argv[2]) + "\" to an unsigned number!");

    FileUtil::RemoveLeadingBytes(argv[1], no_of_bytes_to_remove);

    return EXIT_SUCCESS;
}
