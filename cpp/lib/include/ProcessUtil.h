/** \file    ProcessUtil.h
 *  \brief   The ProcessUtil interface.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2020 University Library of Tübingen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once


#include <string>
#include <unordered_set>
#include <sys/types.h>


namespace ProcessUtil {


/** \return  The list of PID's that have an open file descriptor for "path".
 *  \note    Does currently not include memory-mapped files!
 *  \warning This is not an atomic operation!
 */
std::unordered_set<pid_t> GetProcessIdsForPath(const std::string &path);


} // namespace ProcessUtil
