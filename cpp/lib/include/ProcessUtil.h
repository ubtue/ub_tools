/** \file    ProcessUtil.h
 *  \brief   The ProcessUtil interface.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2020 University Library of Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once


#include <string>
#include <unordered_set>
#include <sys/types.h>


namespace ProcessUtil {


/** \return  The list of PID's that have an open file descripto for "path".
 *  \note    Does currently not include memory-mapped files!
 *  \warning This is not an atomic operation!
 */
std::unordered_set<pid_t> GetProcessIdsForPath(const std::string &path);


} // namespace ProcessUtil
