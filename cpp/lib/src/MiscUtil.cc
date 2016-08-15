/** \file    MiscUtil.cc
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2016 Universitätsbibliothek Tübingen
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

#include "MiscUtil.h"
#include <stack>
#include <stdexcept>
#include "Compiler.h"
#include "StringUtil.h"
#include "util.h"


namespace MiscUtil {


char HexDigit(const unsigned value) {
    switch (value) {
    case 0:
        return '0';
    case 1:
        return '1';
    case 2:
        return '2';
    case 3:
        return '3';
    case 4:
        return '4';
    case 5:
        return '5';
    case 6:
        return '6';
    case 7:
        return '7';
    case 8:
        return '8';
    case 9:
        return '9';
    case 0xA:
        return 'A';
    case 0xB:
        return 'B';
    case 0xC:
        return 'C';
    case 0xD:
        return 'D';
    case 0xE:
        return 'E';
    case 0xF:
        return 'F';
    default:
        Error("in MiscUtil::HexDigit: invalid value " + std::to_string(value) + "!");
    }
}


namespace {


// Collects ASCII letters and underscores and returns the collected string.
// After the call "ch" points at the first character that is neither an ASCII letter nor an underscore.
// If an error occurred and empty string will be returned.
std::string ExtractName(std::string::const_iterator &ch, const std::string::const_iterator &end)
{
    std::string name;

    bool look_for_closing_brace;
    if (*ch == '{') {
        look_for_closing_brace = true;
        ++ch;
    } else
        look_for_closing_brace = false;

    while (ch != end and (StringUtil::IsAsciiLetter(*ch) or *ch == '_'))
        name += *ch++;
    if (unlikely(name.empty() or (ch == end and look_for_closing_brace)))
        return "";
    if (look_for_closing_brace) {
        if (*ch != '}')
            return "";
        ++ch;
    }

    return name;
}


// Extract names given the syntax "(name1,name2,...,nameN)".
bool ExtractLoopVars(std::string::const_iterator &ch, const std::string::const_iterator &end,
                     std::vector<std::string> * const loop_vars, std::string * const err_msg)
{
    loop_vars->clear();
    err_msg->clear();

    if (*ch != '(') {
        *err_msg = "Expected opening parenthesis after LOOP keyword.";
        return false;
    }
    ++ch;

    for (;;) {
        std::string name(ExtractName(ch, end));
        if (unlikely(name.empty())) {
            *err_msg = "Empty variable name or unexpected end-of-template!";
            return false;
        }

        loop_vars->emplace_back(name);
        if (unlikely(ch == end)) {
            *err_msg = "Unexpected end of template while processing a LOOP.";
            return false;
        }
        if (*ch == ')') {
            ++ch;
            return true;
        }
        if (*ch != ',') {
                *err_msg = "Expected comma or closing parenthesis after LOOP variable name, found '"
                           + std::string(1, *ch) + "' instead.";
            return false;
        }
        ++ch;
    }
}
    

inline bool IsLoopVar(const std::string &name, const std::vector<std::string> &loop_vars) {
    for (const auto &loop_var : loop_vars) {
        if (name == loop_var)
            return true;
    }

    return false;
}


// Checks that all loop variables are known and have the same number of values.
bool LoopVarsAreValid(const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                      const std::vector<std::string> &loop_vars, std::string * const err_msg)
{
    ssize_t last_size(-1);
    for (const auto &loop_var : loop_vars) {
        auto name_and_values(names_to_values_map.find(loop_var));
        if (unlikely(name_and_values == names_to_values_map.cend())) {
            *err_msg = "$" + loop_var + " is not a known variable!";
            return false;
        }

        const ssize_t size(static_cast<ssize_t>(name_and_values->second.size()));
        if (last_size == -1)
            last_size = size;
        else if (unlikely(last_size != size)) {
            *err_msg = "not all loop variables have the same number of values!";
            return false;
        }

        if (unlikely(size == 0)) {
            *err_msg = "$" + loop_var + " has zero values!";
            return false;
        }
    }

    return true;
}


bool EvalIfDefined(std::string::const_iterator &ch, const std::string::const_iterator &end, const unsigned line_no,
                   const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    if (unlikely(ch == end or *ch != '('))
        throw std::runtime_error("in MiscUtil::ProcessTemplate: expected '(' after IFDEFINED keyword on line "
                                 + std::to_string(line_no) +"!");
    ++ch;

    const std::string variable_name(ExtractName(ch, end));
    if (unlikely(variable_name.empty()))
        throw std::runtime_error("in MiscUtil::ProcessTemplate: failed to extract variable name after IFDEFINED( "
                                 "on line " + std::to_string(line_no) +"!");
        
    if (unlikely(ch == end or *ch != ')'))
        throw std::runtime_error("in MiscUtil::ProcessTemplate: expected ')' after IFDEFINED variable name on line "
                                 + std::to_string(line_no) +"!");
    ++ch;

    return names_to_values_map.find(variable_name) != names_to_values_map.cend();
}

    
} // unnamed namespace


std::string ExpandTemplate(const std::string &original_template,
                           const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    // Make sure every name in our map has at least one associated value:
    for (const auto &name_and_values : names_to_values_map) {
        if (unlikely(name_and_values.second.empty()))
            throw std::runtime_error("in MiscUtil::ProcessTemplate: map entry \"" + name_and_values.first
                                     + "\" has no associated values!");
    }

    std::string expanded_template;
    std::string::const_iterator ch(original_template.cbegin()), loop_start;
    bool in_loop(false);
    std::stack<bool> skipping;
    unsigned line_no(1), loop_start_line_no;
    std::vector<std::string> loop_vars;
    unsigned loop_index;
    while (ch != original_template.cend()) {
        if (*ch != '$') {
            if (*ch == '\n')
                ++line_no;
            if (skipping.empty() or not skipping.top())
                expanded_template += *ch++;
            else
                ++ch;
        } else { // We found a $-sign.
            ++ch;
            if (unlikely(ch == original_template.cend()))
                throw std::runtime_error("in MiscUtil::ProcessTemplate: $-sign found at end of template!");
            if (*ch == '$') {
                expanded_template += '$';
                ++ch;
            } else {
                const std::string name(ExtractName(ch, original_template.cend()));
                if (unlikely(name.empty()))
                    throw std::runtime_error("in MiscUtil::ProcessTemplate: error while attempting to extract a "
                                             "variable name on line " + std::to_string(line_no) + "!");

                if (name == "LOOP") {
                    if (unlikely(in_loop))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: illegal nested loop on line "
                                                 + std::to_string(line_no) + "!");
                    std::string err_msg;
                    if (unlikely(not (ExtractLoopVars(ch, original_template.cend(), &loop_vars, &err_msg))))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: invalid loop start syntax on line "
                                                 + std::to_string(line_no) + "! (" + err_msg +")");
                    if (unlikely(not LoopVarsAreValid(names_to_values_map, loop_vars, &err_msg)))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: " + err_msg + " (Line no. "
                                                 + std::to_string(line_no) + "!");
                    in_loop = true;
                    loop_start_line_no = line_no;
                    loop_start = ch;
                    loop_index = 0;
                } else if (name == "ENDLOOP") {
                    if (unlikely(not in_loop))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: $ENDLOOP found w/o prior "
                                                 "$LOOP on line " + std::to_string(line_no) + "!");
                    ++loop_index;
                    const auto name_and_values(names_to_values_map.find(loop_vars[0]));
                    if (loop_index < name_and_values->second.size()) {
                        line_no = loop_start_line_no;
                        ch = loop_start;
                        continue;
                    }
                    in_loop = false;
                } else if (name == "IFDEFINED")
                    skipping.push(not EvalIfDefined(ch, original_template.cend(), line_no, names_to_values_map));
                else if (name == "ENDIFDEFINED") {
                    if (unlikely(skipping.empty()))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: found ENDIFDEFINED w/o prior IFDEFINED!");
                    skipping.pop();
                } else { // Expand variable.
                    const auto name_and_values(names_to_values_map.find(name));
                    if (unlikely(name_and_values == names_to_values_map.cend()))
                        throw std::runtime_error("in MiscUtil::ProcessTemplate: $" + name
                                                 + " w/o corresponding value on line " + std::to_string(line_no)
                                                 + "!");
                    if (not IsLoopVar(name_and_values->first, loop_vars)) {
                        if (unlikely(name_and_values->second.size() > 1))
                            throw std::runtime_error("in MiscUtil::ProcessTemplate: loop variable $" + name
                                                     + " used in scalar context!");
                        expanded_template += name_and_values->second[0];
                    } else {
                        if (unlikely(name_and_values->second.size() <= loop_index))
                            throw std::runtime_error("in MiscUtil::ProcessTemplate: ran out of values for $" + name
                                                     + " in loop on line " + std::to_string(line_no) +
                                                     ". (This should *never* happen!)");
                        expanded_template += name_and_values->second[loop_index];
                    }   
                }
            }
        }
    }
    if (unlikely(in_loop))
        throw std::runtime_error("in MiscUtil::ProcessTemplate: unclosed $LOOP at end of template!");
    if (unlikely(not skipping.empty()))
        throw std::runtime_error("in MiscUtil::ProcessTemplate: unclosed $IFDEFINED at end of template!");

    return expanded_template;
}


bool IsValidPPN(const std::string &ppn_candidate) {
    if (ppn_candidate.length() != 9)
        return false;

    for (unsigned i(0); i < 8; ++i) {
        if (not StringUtil::IsDigit(ppn_candidate[i]))
            return false;
    }

    unsigned checksum(0);
    for (unsigned i(0); i < 8; ++i)
        checksum += (9 - i) * (ppn_candidate[i] - '0');
    checksum = (11 - (checksum % 11)) % 11;
    const char checksum_as_char(checksum == 10 ? 'X' : '0' + checksum);

    return ppn_candidate[8] == checksum_as_char;
}


} // namespace MiscUtil
