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
#include <istream>
#include <ostream>
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


/** A simple template expander.  All special constructs are in curly brackets.  To emit normal curly brackets
 *  you must duplicate them.  Variable names as defined by "names_to_values_map" must start with a lowercase ASCII
 *  letter, followed by lowercase ASCII letters, underscores or ASCII digits.  All keywords are all uppercase.
 *  The list of keywords is IF, ELSE, ENDIF, DEFINED, LOOP and ENDLOOP.  The only conditional for an IF is
 *  DEFINED(var).  This returns true if "var" is a key in "names_to_values_map", else false.  Output gets suppressed
 *  if DEFINED(var) elavuates to false.  ELSE is optional.  Loops look like "LOOP var1[,var2..]" if more than one
 *  variable name has been specified, all variables must have the same cardinality.  In a loop "var1" etc. are
 *  automatically indexed based on the current iteration.
 *
 *  \throws std::runtime_error if anything goes wrong, i.e. if a syntax error has been detected.
 */
void ExpandTemplate(std::istream &input, std::ostream &output,
                    const std::map<std::string, std::vector<std::string>> &names_to_values_map);


/** \return True if "ppn_candidate" is a possible Pica Production Number, else false. */
bool IsValidPPN(const std::string &ppn_candidate);


} // namespace MiscUtil


#endif // ifndef MISC_UTIL_H
