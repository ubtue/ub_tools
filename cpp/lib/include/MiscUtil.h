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
 *  The list of keywords is IF, ELSE, ENDIF, DEFINED, LOOP and ENDLOOP.  The conditionals for an IF are either
 *  DEFINED(var), var == "value" var1 == var2, var != "value" and var1 != var2.  The DEFINED(var) returns true if
 *  "var" is a key in "names_to_values_map", else false.  Two conditions may be combined with the keywords AND or OR
 *  String constants must start and end with a double quote.  Three backslash escapes are supported, "\\" for a
 *  literal backslash, "\n" for a newline and "\"" for an embedded double quote.  Output gets suppressed if a
 *  condition evaluates to false.  ELSE is optional.  Loops look like "LOOP var1[,var2..]" if more than one variable
 *  name has been specified, all variables must have the same cardinality.  In a loop "var1" etc. are automatically
 *  indexed based on the current iteration.
 *
 *  \throws std::runtime_error if anything goes wrong, i.e. if a syntax error has been detected.
 */
void ExpandTemplate(std::istream &input, std::ostream &output,
                    const std::map<std::string, std::vector<std::string>> &names_to_values_map);


/** Given a Pica Production Number w/o the checksum digit, this function will return said digit. */
char GeneratePPNChecksumDigit(const std::string &ppn_without_checksum_digit);


/** \return True if "ppn_candidate" is a possible Pica Production Number, else false. */
bool IsValidPPN(const std::string &ppn_candidate);


/** \brief  A safe (i.e. throws on error) wrapper around getenv(3).
 *  \param  name  The name of an environment variable.
 *  \return The value of the environment variable if set (else throws an exception).
 */
std::string GetEnv(const char * const name);
inline std::string GetEnv(const std::string &name) { return GetEnv(name.c_str()); }


/** \brief  A safe wrapper around getenv(3).
 *  \param  name  The name of an environment variable.
 *  \return The value of the environment variable if set otherwise the empty string.
 */
std::string SafeGetEnv(const char * const name);
inline std::string SafeGetEnv(const std::string &name) { return SafeGetEnv(name.c_str()); }


/** \brief Checks for existence of an environment variable
 *  \param  name  The name of an environment variable.
 *  \return true of the variable exists, false otherwise.
 */
bool EnvironmentVariableExists(const std::string &name);


/** \brief A wrapper around setenv(3).
 *  \param name      The name of the environment variable.
 *  \param value     The value of the environment variable.
 *  \param overwrite Whether or not the current value for the given environment variable may be overwritten.
 *  \return          True if the addition of "name=value" is successful.
 */
void SetEnv(const std::string &name, const std::string &value, const bool overwrite = true);


// \return The POSIX login name. */
std::string GetUserName();


} // namespace MiscUtil


#endif // ifndef MISC_UTIL_H
