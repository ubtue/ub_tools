/** \file    MiscUtil.h
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2016 Universitätsbibliothek Tübingen.
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
#include <map>
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


/** A simple template expander.  Variables look like "$var" or "${var}".  There are two kinds of variables:
 *  Scalar-values ones and vector-valued ones.  Vector valued ones have more than a single value associated
 *  with them.  Two varables are special: "$LOOP" and "$ENDLOOP".  They are used as brackets for repeated
 *  sections.  A repeated section starts with "$LOOP(vvar1,vvar2,..,vvarN)" where vvar1, vvar2, and vvarN are
 *  vector-valued "loop" variables that all must have the same number of values.  The section up to the $ENDLOOP
 *  will then be expanded N times where N is the length of each of the loop variables.  On the first iteration
 *  the 0-th values will be used, then the values with index 1 and so on until the values have been exhausted.
 *  Should you need to embed a literal $-sign in the expanded template, use a "$$" in the original template.
 *  Variable names must consist of ASCII letters and underscores only.  Should you need an ASCII letter or an
 *  underscore immediately following a variable, use the ${var} syntax instead of the $var syntax.  The braces
 *  are also allowed with ${LOOP} and ${ENDLOOP}.  Another feature is a conditional starting with $IFDEFINED(var)
 *  and ending with $ENDIFDEFINED.  If "var" is not known any output will be suppressed until the corresponding
 *  $ENDIFDEFINED.  This feature can be nested.
 */
std::string ExpandTemplate(const std::string &original_template,
                           const std::map<std::string, std::vector<std::string>> &names_to_values_map);


/** \return True if "ppn_candidate" is a possible Pica Production Number, else false. */
bool IsValidPPN(const std::string &ppn_candidate);


} // namespace MiscUtil


#endif // ifndef MISC_UTIL_H
