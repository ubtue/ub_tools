/** \file    Template.h
 *  \brief   Declarations of template-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2016-2018 Universitätsbibliothek Tübingen.
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

#ifndef TEMPLATE_H
#define TEMPLATE_H


#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <vector>


namespace Template {


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
std::string ExpandTemplate(const std::string &template_string,
                           const std::map<std::string, std::vector<std::string>> &names_to_values_map);


} // namespace MiscUtil


#endif // ifndef TEMPLATE_H
