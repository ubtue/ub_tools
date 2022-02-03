/** \brief Utility for concatenating strings in shell scripts.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <cstring>
#include "StringUtil.h"
#include "util.h"


std::string EscapeDoubleQuotesAndBangs(const std::string &s) {
    std::string escaped_s;
    escaped_s.reserve(s.size());

    for (auto ch : s) {
        if (unlikely(ch == '!'))
            escaped_s += "\"'!'\"";
        else {
            if (unlikely(ch == '"' or ch == '\\'))
                escaped_s += '\\';
            escaped_s += ch;
        }
    }

    return escaped_s;
}


int Main(int argc, char *argv[]) {
    if (argc == 1) {
        ::Usage(
            "[--emit-trailing-newline] [--cstyle-escape|--escape-double-quotes-and-bangs] [--] string1 [string2 .. stringN]\n"
            "In the unlikely case that your first string is \"--cstyle-escape\" use -- to indicate the\n"
            "end of flags, o/w if the first argument is --cstyle-escape we assume you mean the flag.\n\n");
        return EXIT_SUCCESS;
    }

    bool end_of_flags(false), escape(false), emit_trailing_newline(false), escape_double_quotes(false);
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        if (not end_of_flags and std::strcmp(argv[arg_no], "--") == 0) {
            end_of_flags = true;
            continue;
        }
        if (not end_of_flags and std::strcmp(argv[arg_no], "--cstyle-escape") == 0) {
            if (escape_double_quotes)
                LOG_ERROR("can't specify both, --cstyle-escape and --escape-double-quotes-and-bangs!");
            escape = true;
            continue;
        }
        if (not end_of_flags and std::strcmp(argv[arg_no], "--emit-trailing-newline") == 0) {
            emit_trailing_newline = true;
            continue;
        }
        if (not end_of_flags and std::strcmp(argv[arg_no], "--escape-double-quotes-and-bangs") == 0) {
            if (escape)
                LOG_ERROR("can't specify both, --cstyle-escape and --escape-double-quotes-and-bangs!");
            escape_double_quotes = true;
            continue;
        }

        end_of_flags = true;
        if (escape)
            std::cout << StringUtil::CStyleEscape(argv[arg_no]);
        else if (escape_double_quotes)
            std::cout << EscapeDoubleQuotesAndBangs(argv[arg_no]);
        else
            std::cout << argv[arg_no];
    }

    if (emit_trailing_newline)
        std::cout << '\n';

    return EXIT_SUCCESS;
}
