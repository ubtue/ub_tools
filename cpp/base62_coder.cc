/** \brief Utility for converting to and from base62.
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

#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("--decode base62_number|--encode base10_number");
}


const char BASE62_DIGITS[](
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz");


std::string EncodeBase10ToBase62(const std::string &base10_number) {
    unsigned long long binary_number;
    if (not StringUtil::ToUnsignedLongLong(base10_number, &binary_number))
        LOG_ERROR("not a base 62 number \"" + base10_number + "\"!");

    std::string base62_number;
    while (binary_number != 0) {
        base62_number = BASE62_DIGITS[binary_number % 62u] + base62_number;
        binary_number /= 62u;
    }

    return base62_number.empty() ? "0" : base62_number;
}


unsigned long long DecodeBase62ToBinary(const std::string &base62_number) {
    unsigned long long binary_number(0);
    for (const char ch : base62_number) {
        const auto position(std::strchr(BASE62_DIGITS, ch));
        if (unlikely(position == nullptr))
            LOG_ERROR("not a base 10 digit '" + std::string(1, ch) + "'!");
        binary_number *= 62u;
        binary_number += position - BASE62_DIGITS;
    }

    return binary_number;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    if (__builtin_strcmp(argv[1], "--decode") == 0)
        std::cout << DecodeBase62ToBinary(argv[2]) << '\n';
    else if (__builtin_strcmp(argv[1], "--encode") == 0)
        std::cout << EncodeBase10ToBase62(argv[2]) << '\n';
    else
        Usage();

    return EXIT_SUCCESS;
}
