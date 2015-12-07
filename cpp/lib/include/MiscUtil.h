/** \file    MiscUtil.h
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MISC_UTIL_H
#define MISC_UTIL_H


#include <algorithm>
#include <fstream>
#include <list>
#include <set>
#include <vector>
#include <stdexcept>
#include <string>
#include <cerrno>
#include <clocale>
#include <cstdlib>


namespace MiscUtil {


/** \brief  Convert a numeric value between 0 and 15 to a hex digit.
 *  \param  value  A number between 0 and 15.
 *  \return The character representing the hexadecimal value.
 */
char HexDigit(const unsigned value);


} // namespace MiscUtil


#endif // ifndef MISC_UTIL_H
