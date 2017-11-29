/** \file    StringUtil.cc
 *  \brief   Implementation of string utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Wagner Truppel
 *  \author  Paul Vander Griend
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2002-2005 Dr. Johannes Ruscheinski.
 *  Copyright 2017 Universitätsbibliothek Tübingen
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

#include "StringUtil.h"
#include <algorithm>
#include <set>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdio>
#include <alloca.h>
#include <iomanip>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <stdarg.h>
#include <sstream>
#include <unistd.h>
#include "Compiler.h"


#ifdef DIM
#      undef DIM
#endif
#define DIM(array)      (sizeof(array)/sizeof(array[0]))


namespace {


__attribute__((constructor)) bool InitializeLocale() {
    // Try to force the use of the iVia standard locale:
    if (std::setlocale(LC_CTYPE, StringUtil::IVIA_STANDARD_LOCALE.c_str()) == nullptr) {
        const std::string error_message("in InitializeLocale: setlocale(3) failed: "
                                        + StringUtil::IVIA_STANDARD_LOCALE + "\n");
        const ssize_t dummy = ::write(STDERR_FILENO, error_message.c_str(), error_message.length());
        (void)dummy;
        ::_exit(EXIT_FAILURE);
    }

    // Try to force the use of the iVia standard locale:
    if (std::setlocale(LC_MONETARY, StringUtil::IVIA_STANDARD_LOCALE.c_str()) == nullptr) {
        const std::string error_message("In InitializeLocale: setlocale(3) failed for LC_MONETARY.  Cannot "
                                        "honour the '" + StringUtil::IVIA_STANDARD_LOCALE + "' locale!\n");
        const ssize_t dummy = ::write(STDERR_FILENO, error_message.c_str(), error_message.length());
        (void)dummy;
        ::_exit(EXIT_FAILURE);
    }

    return true;
}


char ToHexChar(const unsigned u) {
    switch (u) {
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
        throw std::runtime_error("in ToHexChar: value out of range (" + StringUtil::ToString(u) + ")!");
    }
}


} // unnamed namespace


namespace StringUtil {


std::string ToLower(std::string * const s) {
    for (std::string::iterator ch(s->begin()); ch != s->end(); ++ch)
        *ch = tolower(*ch);

    return *s;
}


std::string ToLower(const std::string &s) {
    std::string result(s);
    for (std::string::iterator ch(result.begin()); ch != result.end(); ++ch)
        *ch = tolower(*ch);

    return result;
}


std::string ToUpper(std::string * const s) {
    for (std::string::iterator ch(s->begin()); ch != s->end(); ++ch)
        *ch = toupper(*ch);

    return *s;
}


std::string ToUpper(const std::string &s) {
    std::string result(s);
    for (std::string::iterator ch(result.begin()); ch != result.end(); ++ch)
        *ch = toupper(*ch);

    return result;
}


bool IsAllUppercase(const std::string &s) {
    if (s.empty())
        return false;

    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (not isupper(*ch))
            return false;
    }

    return true;
}


bool IsAllLowercase(const std::string &s) {
    if (s.empty())
        return false;

    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (not islower(*ch))
            return false;
    }

    return true;
}


bool IsInitialCapsString(const std::string &s)
{
        if (s.length() < 2)
                return false;

        std::string::const_iterator ch(s.begin());
        if (not isupper(*ch))
                return false;

        for (++ch; ch != s.end(); ++ch) {
                if (not islower(*ch))
                        return false;
        }

        return true;
}


bool IsWhitespace(const std::string &s) {
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (not IsWhitespace(*ch))
            return false;
    }

    return true;
}


char ToHex(const unsigned nibble) {
    switch (nibble) {
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
        throw std::runtime_error("in ToHex: number " + std::to_string(nibble) + " not in [0,15] range!");
    }
}


std::string RightTrim(std::string * const s, char trim_char) {
    size_t original_length = s->length();
    if (original_length == 0)
        return *s;

    size_t trimmed_length = original_length;
    while (trimmed_length > 0 and (*s)[trimmed_length-1] == trim_char)
        --trimmed_length;

    if (trimmed_length < original_length)
        s->resize(trimmed_length);

    return *s;
}


std::string RightTrim(const std::string &s, char trim_char)
{
        std::string temp_s(s);
        return RightTrim(&temp_s, trim_char);
}


std::string RightTrim(const std::string &trim_set, std::string * const s) {
    size_t original_length = s->length();
    if (original_length == 0)
        return *s;

    size_t trimmed_length = original_length;
    const char * const set = trim_set.c_str();
    while (trimmed_length > 0 and ::strchr(set, (*s)[trimmed_length-1]) != nullptr)
        --trimmed_length;

    if (trimmed_length < original_length)
        *s = s->substr(0, trimmed_length);

    return *s;
}


std::string RightTrim(const std::string &trim_set, const std::string &s) {
    std::string temp_s(s);
    return RightTrim(trim_set, &temp_s);
}


char *strrtrim(char *s, char trim_char) {
    const size_t len = std::strlen(s);
    char *cp = s + len;
    while (--cp >= s and *cp == trim_char)
        /* intentionally empty */;
    *(cp + 1) = '\0';

    return s;
}


std::string LeftTrim(std::string * const s, char trim_char) {
    size_t original_length = s->length();
    if (original_length == 0)
        return *s;

    size_t no_of_leading_trim_chars(0);
    while (no_of_leading_trim_chars < original_length - 1 and (*s)[no_of_leading_trim_chars] == trim_char)
        ++no_of_leading_trim_chars;

    if (no_of_leading_trim_chars > 0)
        *s = s->substr(no_of_leading_trim_chars);

    return *s;
}


std::string LeftTrim(const std::string &s, char trim_char) {
    std::string temp_s(s);
    return LeftTrim(&temp_s, trim_char);
}


std::string LeftTrim(const std::string &trim_set, std::string * const s) {
    size_t original_length = s->length();
    if (original_length == 0)
        return *s;

    size_t no_of_leading_trim_chars(0);
    const char * const set = trim_set.c_str();
    while (no_of_leading_trim_chars < original_length - 1 and std::strchr(set, (*s)[no_of_leading_trim_chars]) != nullptr)
        ++no_of_leading_trim_chars;

    if (no_of_leading_trim_chars > 0)
        *s = s->substr(no_of_leading_trim_chars);

    return *s;
}


std::string LeftTrim(const std::string &trim_set, const std::string &s) {
    std::string temp_s(s);
    return LeftTrim(trim_set, &temp_s);
}


std::string Trim(std::string * const s, char trim_char) {
    RightTrim(s, trim_char);
    return LeftTrim(s, trim_char);
}


std::string Trim(const std::string &s, char trim_char) {
    std::string temp_s(s);
    return Trim(&temp_s, trim_char);
}


std::string Trim(const std::string &trim_set, std::string * const s) {
        RightTrim(trim_set, s);
        return LeftTrim(trim_set, s);
}


std::string Trim(const std::string &s, const std::string &trim_set) {
        std::string temp_s(s);
        return Trim(trim_set, &temp_s);
}


std::string ToString(long long n, const unsigned radix, const int width, const char grouping_char,
                     const unsigned grouping_size)
{
    assert(radix >= 2 and radix <= 36);

    static const char DIGITS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[129+1]; // enough room for a base-2 negative 128 bit number
    char *cp = buf + sizeof(buf) - 1;
    *cp = '\0';
    std::string result;

    bool negate = n < 0;
    if (n < 0)
        n = -n;

    unsigned count(0);
    do {
        *--cp = DIGITS[n % radix];
        n /= radix;
        ++count;
        if (grouping_char != '\0' and count % grouping_size == 0)
            *--cp = grouping_char;
    } while (n != 0);

    // Remove a leading "grouping_char" if necessary and update "count":
    if (grouping_char != '\0') {
        if (count % grouping_size == 0)
            ++cp;
        count += (count - 1) / grouping_size;
    }

    if (negate)
        *--cp = '-';

    const unsigned abs_width(abs(width));
    if (count < abs_width) {
        if (width < 0)
            return cp + std::string(abs_width - count, ' ');
        else
            return std::string(abs_width - count, ' ') + cp;
    } else
        return cp;
}


std::string ToString(long n, const unsigned radix, const int width) {
    assert(radix >= 2 and radix <= 36);

    static const char DIGITS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[129 + 1]; // enough room for a base-2 negative 128 bit number
    char *cp = buf + sizeof(buf) - 1;
    *cp = '\0';
    std::string result;

    bool negate = n < 0;
    if (n < 0)
        n = -n;

    unsigned count(0);
    do {
        *--cp = DIGITS[n % radix];
        n /= radix;
        ++count;
    } while (n != 0);

    if (negate)
        *--cp = '-';

    const unsigned abs_width(abs(width));
    if (count < abs_width) {
        if (width < 0)
            return cp + std::string(abs_width - count, ' ');
        else
            return std::string(abs_width - count, ' ') + cp;
    } else
        return cp;
}


std::string ToString(unsigned long long n, const unsigned radix, const int width, const char grouping_char,
                     const unsigned grouping_size)
{
    assert(radix >= 2 and radix <= 36);

    static const char DIGITS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[256];
    char *cp = buf + sizeof(buf) - 1;
    *cp = '\0';
    std::string result;

    unsigned count(0);
    do {
        *--cp = DIGITS[n % radix];
        n /= radix;
        ++count;
        if (grouping_char != '\0' and count % grouping_size == 0)
            *--cp = grouping_char;
    } while (n != 0);

    // Remove a leading "grouping_char" if necessary and update "count":
    if (grouping_char != '\0') {
        if (count % grouping_size == 0)
            ++cp;
        count += (count - 1) / grouping_size;
    }

    const unsigned abs_width(abs(width));
    if (count < abs_width) {
        if (width < 0)
            return cp + std::string(abs_width - count, ' ');
        else
            return std::string(abs_width - count, ' ') + cp;
    }

    return cp;
}


std::string ToString(const double n, const unsigned precision) {
    std::stringstream stream;
    if (precision > 1) // Only show decimal point if we have more than one digit of precision:
        stream.setf(std::ios_base::showpoint);
    stream << std::setprecision(precision) << n;

    // Do we have a trailing naked . ?
    if (unlikely(stream.str().rfind('.') == stream.str().size() - 1))
        return stream.str().substr(0, stream.str().size() - 1);

    return stream.str();
}


std::string ToHexString(const std::string &s){
    std::string hex_string;
    hex_string.reserve(s.size() * 2);

    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        hex_string += ToHexChar(static_cast<unsigned char>(*ch) >> 4u);
        hex_string += ToHexChar(static_cast<unsigned char>(*ch) & 0xFu);
    }

    return hex_string;
}


std::string ToHexString(uint32_t u32) {
    std::string hex_string;
    hex_string.reserve(8);

    for (unsigned nybble(0); nybble < 8; ++nybble) {
        hex_string += ToHex(u32 & 0xFu);
        u32 >>= 4u;
    }
    std::reverse(hex_string.begin(), hex_string.end());

    return hex_string;
}


std::string ToHexString(const uint8_t u8) {
    std::string hex_string;
    hex_string += ToHex(u8 >> 4u);  // upper nybble
    hex_string += ToHex(u8 & 0xFu); // lower nybble

    return hex_string;
}


// FromHex -- returns a binary nibble corresponding to "ch".
//
unsigned char FromHex(const char ch) {
    switch (toupper(ch)) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'A':
        return 0xA;
    case 'B':
        return 0xB;
    case 'C':
        return 0xC;
    case 'D':
        return 0xD;
    case 'E':
        return 0xE;
    case 'F':
        return 0xF;
    default: {
        std::string error_message("invalid hex character '");
        error_message += ch;
        error_message += "'!";
        throw std::runtime_error(error_message);
    }
    }
}


std::string FromHexString(const std::string &hex_string) {
    std::string decoded_string;
    decoded_string.reserve(hex_string.size() / 2);

    for (std::string::const_iterator ch(hex_string.begin()); ch != hex_string.end(); ++ch) {
        unsigned char first_nibble(FromHex(*ch));
        ++ch;
        if (unlikely(ch == hex_string.end()))
            throw std::runtime_error("invalid hex string (must contain an even number of characters)!");
        unsigned char second_nibble(FromHex(*ch));
        decoded_string += static_cast<char>((first_nibble << 4u) | second_nibble);
    }

    return decoded_string;
}


// ToNumber -- convert a string into a number.
//
bool ToNumber(const std::string &s, int * const n, const unsigned base) {
    if (unlikely(s.empty()))
        return false;

    char *end_ptr;
    errno = 0;
    const long l = std::strtol(s.c_str(), &end_ptr, base);
    *n = l;

    return (l <= INT_MAX and l >= INT_MIN) and (*end_ptr == '\0') and (errno == 0);
}


// ToNumber -- convert a string into a number.
//
bool ToNumber(const std::string &s, long * const n, const unsigned base) {
    if (unlikely(s.empty()))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtol(s.c_str(), &end_ptr, base);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToNumber -- convert a string into an unsigned number.
//
bool ToNumber(const std::string &s, unsigned * const n, const unsigned base) {
    if (unlikely(s.empty()))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtoul(s.c_str(), &end_ptr, base);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToNumber -- convert a string into a number.
//
long ToNumber(const std::string &s, const unsigned base) {
    long n;
    if (unlikely(not ToNumber(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToNumber: can't convert \"" + s + "\" to a long!");

    return n;
}


unsigned short ToUnsignedShort(const std::string &s, const unsigned base) {
    unsigned short n;
    if (unlikely(not ToUnsignedShort(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToUnsignedShort: can't convert \"" + s + "\" to an unsigned!");

    return n;
}


// ToUnsignedShort -- convert a string to an unsigned number.
//
bool ToUnsignedShort(const std::string &s, unsigned short * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end() or *ch == '-'))
        return false;

    char *end_ptr;
    errno = 0;
    const unsigned long ul = std::strtoul(s.c_str(), &end_ptr, base);
    *n = static_cast<unsigned>(ul);

    return (*end_ptr == '\0') and (errno == 0) and (ul <= USHRT_MAX);
}


unsigned ToUnsigned(const std::string &s, const unsigned base) {
    unsigned n;
    if (unlikely(not ToUnsigned(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToUnsigned: can't convert \"" + s + "\" to an unsigned!");

    return n;
}


// ToUnsigned -- convert a string to an unsigned number.
//
bool ToUnsigned(const std::string &s, unsigned * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end() or *ch == '-'))
        return false;

    char *end_ptr;
    errno = 0;
    const unsigned long ul = std::strtoul(s.c_str(), &end_ptr, base);
    *n = static_cast<unsigned>(ul);

    return (*end_ptr == '\0') and (errno == 0) and (ul <= UINT_MAX);
}


// ToUnsignedLong -- convert a string to an unsigned long number.
//
unsigned long ToUnsignedLong(const std::string &s, const unsigned base) {
    unsigned long n;
    if (unlikely(not ToUnsignedLong(s,  &n, base)))
        throw std::runtime_error("in StringUtil::ToUnsignedLong: can't convert " + s);

    return n;
}


// ToUnsignedLong -- convert a string to an unsigned long number.
//
bool ToUnsignedLong(const std::string &s, unsigned long * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end() or *ch == '-'))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtoul(s.c_str(), &end_ptr, base);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToUnsignedLongLong -- convert a string to an unsigned long long number.
//
unsigned long long ToUnsignedLongLong(const std::string &s, const unsigned base) {
    unsigned long long n;
    if (unlikely(not ToUnsignedLongLong(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToUnsignedLongLong: can't convert " + s);

    return n;
}


// ToUnsignedLongLong -- convert a string to an unsigned long long number.
//
bool ToUnsignedLongLong(const std::string &s, unsigned long long * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end() or *ch == '-'))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtoull(s.c_str(), &end_ptr, base);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToUInt64T -- convert a string to a uint64_t number
//
bool ToUInt64T(const std::string &s, uint64_t * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end() or *ch == '-'))
        return false;

    char *end_ptr;
    unsigned long long temp;
    errno = 0;
    temp = std::strtoull(s.c_str(), &end_ptr, base);

    // Make sure the number fits in a uint64_t:
    if (temp > UINT64_MAX)
        return false;
    else
        *n = static_cast<uint64_t>(temp);

    return (*end_ptr == '\0') and (errno == 0);
}


uint64_t ToUInt64T(const std::string &s, const unsigned base) {
    uint64_t n;
    if (unlikely(not ToUInt64T(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToUInt64T: can't convert " + s);

    return n;
}


// ToInt64T -- convert a string to an int64_t number
//
bool ToInt64T(const std::string &s, int64_t * const n, const unsigned base) {
    std::string::const_iterator ch(s.begin());
    while (ch != s.end() and isspace(*ch))
        ++ch;
    if (unlikely(ch == s.end()))
        return false;

    char *end_ptr;
    errno = 0;
    const long long temp(std::strtoll(s.c_str(), &end_ptr, base));

    // Make sure the number fits in a int64_t:
    if (temp > INT64_MAX)
        return false;
    *n = static_cast<int64_t>(temp);

    return (*end_ptr == '\0') and (errno == 0);
}


int64_t ToInt64T(const std::string &s, const unsigned base) {
    int64_t n;
    if (unlikely(not ToInt64T(s, &n, base)))
        throw std::runtime_error("in StringUtil::ToInt64T: can't convert " + s);

    return n;
}


// ToDouble -- convert a string to a double-precision number.
//
bool ToDouble(const std::string &s, double * const n) {
    if (unlikely(s.empty()))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtod(s.c_str(), &end_ptr);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToDouble -- convert a string to a double-precision number.
//
double ToDouble(const std::string &s) {
    double n;
    if (unlikely(not ToDouble(s, &n)))
        throw std::runtime_error("in StringUtil::ToDouble: can't convert \"" + s + "\"!");

    return n;
}


// ToFloat -- convert a string to a float-precision number.
//
bool ToFloat(const std::string &s, float * const n) {
    if (unlikely(s.empty()))
        return false;

    char *end_ptr;
    errno = 0;
    *n = std::strtof(s.c_str(), &end_ptr);

    return (*end_ptr == '\0') and (errno == 0);
}


// ToFloat -- convert a string to a float-precision number.
//
float ToFloat(const std::string &s) {
    float n;
    if (unlikely(not ToFloat(s, &n)))
        throw std::runtime_error("in StringUtil::ToFloat: can't convert \"" + s + "\"!");

    return n;
}


bool ToBool(const std::string &value, bool * const b) {
    if (::strcasecmp(value.c_str(), "true") == 0 or ::strcasecmp(value.c_str(), "yes") == 0
        or ::strcasecmp(value.c_str(), "on") == 0)
    {
        *b = true;
        return true;
    }

    if (::strcasecmp(value.c_str(), "false") == 0 or ::strcasecmp(value.c_str(), "off") == 0
        or ::strcasecmp(value.c_str(), "no") == 0)
    {
        *b = false;
        return true;
    }

    return false;
}


bool ToBool(const std::string &value) {
    bool b;
    if (likely(ToBool(value, &b)))
        return b;

    throw std::runtime_error("in StringUtil::ToBool: can't convert \"" + value + "\" to a bool!");
}


// AlphaCompare -- compares "lhs" and "rhs" in a case-idependent manner, skipping over leading spaces and/or articles.
//                 Return values analogous to strcmp(3).
//
int AlphaCompare(const char *lhs, const char *rhs) {
    // Notice the single trailing spaces!
    static const char *articles[] = { "the ", "a ", "an ", "le ", "les ", "la ", "der ",
                                      "die ", "das ", "el " };

    // skip over leading spaces:
    while (*lhs == ' ')
        ++lhs;

    // skip over an LHS article:
    for (unsigned article_no = 0; article_no < DIM(articles); ++article_no) {
        size_t len = std::strlen(articles[article_no]);
        if (::strncasecmp(lhs, articles[article_no], len) == 0 and
            ::strncasecmp("a capella", lhs, 9) != 0)
            {
                lhs += len;
                break;
            }
    }

    while (*lhs == ' ')
        ++lhs;

    // skip over leading spaces:
    while (*rhs == ' ')
        ++rhs;


    // skip over an RHS article:
    for (unsigned article_no = 0; article_no < DIM(articles); ++article_no) {
        size_t len = std::strlen(articles[article_no]);
        if (::strncasecmp(rhs, articles[article_no], len) == 0 and
            ::strncasecmp("a capella", rhs, 9) != 0)
            {
                rhs += len;
                break;
            }
    }

    while (*rhs == ' ')
        ++rhs;

    return ::strcasecmp(lhs, rhs);
}


const char *SkipLeadingArticle(const char *text) {
    // Notice the single trailing spaces!
    static const char *articles[] = { "the ", "a ", "an ", "le ", "les ", "la ", "der ", "die ", "das ", "el " };

    for (unsigned article_no = 0; article_no < DIM(articles); ++article_no) {
        size_t len = std::strlen(articles[article_no]);
        if (::strncasecmp(text, articles[article_no], len) == 0 and ::strncasecmp("a capella", text, 9) != 0) {
            text += len;
            break;
        }
    }

    while (isspace(*text))
        ++text;

    return text;
}


const char *SkipNonAlphanumericChars(const char *text) {
    while (*text and not isalnum(*text))
        ++text;
    return text;
}


void RemoveTrailingLineEnd(char *line) {
    // we want to remove "\n", "\r", "\r\n", or \n\r", but not "\n\n" or "\r\r"
    const unsigned line_length = std::strlen(line);
    if (line_length >= 1) {
        if (line_length >= 2) {
            char *last_chars = &line[line_length - 2];
            if ((last_chars[0] == '\r' and last_chars[1] == '\n') or (last_chars[0] == '\n' and last_chars[1] == '\r'))
                line[std::strlen(line) - 2] = '\0';
            else if (line[line_length - 1] == '\n' or line[line_length - 1] == '\r')
                line[std::strlen(line) - 1] = '\0';
        } else if (line[line_length - 1] == '\n' or line[line_length - 1] == '\r')
            line[std::strlen(line) - 1] = '\0';
    }
}


void RemoveTrailingLineEnd(std::string * const line) {
    unsigned line_length = line->length();
    if (line_length >= 1) {
        if (line_length >= 2) {
            std::string last_chars = line->substr(line->length()-2, 2);
            if ((last_chars == "\n\r") or (last_chars == "\r\n"))
                line->erase(line->length()-2);
            else if (line->substr(line_length - 1) == "\n" or line->substr(line_length - 1) == "\r")
                line->erase(line->length()-1);
        } else if (line->substr(line_length - 1) == "\n" or line->substr(line_length - 1) == "\r")
            line->erase(line->length()-1);
    }
}


char *RemoveTrailingLineEnds(char * const line) {
    // we want to remove "\n", "\r\n", or \n\r", but not "\n\n" or "\r\r"
    size_t length = std::strlen(line);
    while (length > 0 and line[length-1] == '\n')
        line[length-1] = '\0';

    return line;
}


std::string &RemoveTrailingLineEnds(std::string * const line) {
    size_t new_length = line->length();
    if (new_length > 0 and (*line)[new_length-1] == '\n')
        --new_length;

    if (new_length < line->length())
        *line = line->substr(0, new_length);

    return *line;
}


const char *trans_table[256] = {
    "\x00", "\x01", "\x02", "\x03", "\x04", "\x05", "\x06", "\x07", "\x08", "\x09", "\x0A", "\x0B", "\x0C", "\x0D", "\x0E", "\x0F",
    "\x10", "\x11", "\x12", "\x13", "\x14", "\x15", "\x16", "\x17", "\x18", "\x19", "\x1A", "\x1B", "\x1C", "\x1D", "\x1E", "\x1F",
    "\x20", "\x21", "\x22", "\x23", "\x24", "\x25", "\x26", "\x27", "\x28", "\x29", "\x2A", "\x2B", "\x2C", "\x2D", "\x2E", "\x2F",
    "\x30", "\x31", "\x32", "\x33", "\x34", "\x35", "\x36", "\x37", "\x38", "\x39", "\x3A", "\x3B", "\x3C", "\x3D", "\x3E", "\x3F",
    "\x40", "\x41", "\x42", "\x43", "\x44", "\x45", "\x46", "\x47", "\x48", "\x49", "\x4A", "\x4B", "\x4C", "\x4D", "\x4E", "\x4F",
    "\x50", "\x51", "\x52", "\x53", "\x54", "\x55", "\x56", "\x57", "\x58", "\x59", "\x5A", "\x5B", "\x5C", "\x5D", "\x5E", "\x5F",
    "\x60", "\x61", "\x62", "\x63", "\x64", "\x65", "\x66", "\x67", "\x68", "\x69", "\x6A", "\x6B", "\x6C", "\x6D", "\x6E", "\x6F",
    "\x70", "\x71", "\x72", "\x73", "\x74", "\x75", "\x76", "\x77", "\x78", "\x79", "\x7A", "\x7B", "\x7C", "\x7D", "\x7E", "\x7F",
    "\x80", "\x81", "\x82", "\x83", "\x84", "\x85", "\x86", "\x87", "\x88", "\x89", "\x8A", "\x8B", "\x8C", "\x8D", "\x8E", "\x8F",
    "\x90", "\x91", "\x92", "\x93", "\x94", "\x95", "\x96", "\x97", "\x98", "\x99", "\x9A", "\x9B", "\x9C", "\x9D", "\x9E", "\x9F",
    "\xA0", "\xA1", "c",    "L",    "\xA4", "Y",    "\xA6", "\xA7", "\xA8", "(c)",  "\xAA", "\xAB", "\xAC", "\xAD", "(R)",  "\xAF",
    "\xB0", "\xB1", "2",    "3",    "\xB4", "\xB5", "\xB6", "\xB7", "\xB8", "\xB9", "\xBA", "\xBB", "1/4",  "1/2",  "3/4",  "\xBF",
    "A",    "A",    "A",    "A",    "A",    "A",    "AE",   "C",    "E",    "E",    "E",    "E",    "I",    "I",    "I",    "I",
    "D",    "N",    "O",    "O",    "O",    "O",    "O",    "x",    "O",    "U",    "U",    "U",    "U",    "Y",    "P",    "ss",
    "a",    "a",    "a",    "a",    "a",    "a",    "ae",   "c",    "e",    "e",    "e",    "e",    "i",    "i",    "i",    "i",
    "d",    "n",    "o",    "o",    "o",    "o",    "o",    "/",    "o",    "u",    "u",    "u",    "u",    "y",    "p",    "y"
};


std::string &AnsiToAscii(std::string * const ansi_string) {
    std::string ascii_string;
    for (std::string::const_iterator ch(ansi_string->begin()); ch != ansi_string->end(); ++ch)
        ascii_string += trans_table[static_cast<unsigned char>(*ch)];

    *ansi_string = ascii_string;
    return *ansi_string;
}


// Map -- Replace every occurrence of a char with a new char
//
std::string &Map(std::string * const s, const char old_char, const char new_char) {
    for (std::string::iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (*ch == old_char)
            *ch = new_char;
    }
    return *s;
}


// Map -- Replace every occurrence of a char with a new char
//
std::string &Map(std::string * const s, const std::string &old_set, const std::string &new_set) {
    if (unlikely(old_set.length() != new_set.length()))
        throw std::runtime_error("in StringUtil::Subst: old_set.length() != new_set.length()");

    for (std::string::iterator ch(s->begin()); ch != s->end(); ++ch) {
        std::string::size_type pos;
        if ((pos = old_set.find(*ch)) != std::string::npos)
            *ch = new_set[pos];
    }

    return *s;
}


// Collapse -- compress multiple occurrences of ch into a single occurrence.
//
std::string &Collapse(std::string * const s, char scan_ch) {
    std::string::iterator current(s->begin());

    bool skipping(false);
    for (std::string::const_iterator leading(s->begin()); leading != s->end(); ++leading) {
        if (*leading == scan_ch) {
            if (skipping)
                continue;
            *current++ = *leading;
            skipping = true;
        }
        else {
            skipping = false;
            *current++ = *leading;
        }
    }

    s->resize(current - s->begin());
    return *s;
}


// CollapseWhitespace -- Collapses multiple occurrences of whitespace into a single space
//
std::string &CollapseWhitespace(std::string * const s) {
    std::string result;
    result.reserve(s->size());

    bool last_char_was_not_space(true);
    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (IsSpace(*ch)) {
            // Add whitespace as a space, but only if
            // the last char wasn't also a space.
            if (last_char_was_not_space) {
                result += ' ';
                last_char_was_not_space = false;
            }
        }
        else {
            // Add any non-whitespace character unconditionally
            result += *ch;
            last_char_was_not_space = true;
        }
    }

    return *s = result;
}


// CollapseAndTrimWhitespace -- Collapses multiple occurrences of whitespace into a single space and removes leading
// and trailing whitespace.
std::string &CollapseAndTrimWhitespace(std::string * const s) {
    std::string result;
    result.reserve(s->size());

    std::string::iterator ch(s->begin());

    // Skip over leading whitespace, if any:
    while (ch != s->end() and IsSpace(*ch))
        ++ch;

    bool last_char_was_not_space(true);
    for (/* Empty. */; ch != s->end(); ++ch) {
        if (IsSpace(*ch)) {
            // Add whitespace as a space, but only if
            // the last char wasn't also a space.
            if (last_char_was_not_space) {
                result += ' ';
                last_char_was_not_space = false;
            }
        } else {
            // Add any non-whitespace character unconditionally
            result += *ch;
            last_char_was_not_space = true;
        }
    }

    // Trim a trailing whitespace, if necessary:
    if (not last_char_was_not_space)
        result.resize(result.size() - 1);

    return *s = result;
}


// Match -- regular expression pattern match.  '?' represents exactly one arbitrary character
//          while '*' represents an arbitrary sequence of characters including the null sequence.
//          Backslash '\' is the `escape' character and removes the special meaning of any following
//          character.  '[' starts a character class (set). Therefore in order to match any of
//          {'\','*','?','['} you have to preceed them with a single '\'.
//          Character classes are enclosed in square brackets and negated if the first character in
//          the set is a caret '^'.  So in order to match any single decimal digit you may specify
//          "[0123456789]" where the order of the digits doesn't matter.  In order to match any
//          single character with the exception of a single decimal digit specify "[^0123456789]".
//
//          (c© 1988,1990,2001 Johannes Ruscheinski. All Rights Reserved.  I grant the exception
//          that this function may be contained and used with the rest of the iVia library
//          under the the same copyright rules as the iVia library if it is any version of
//          the GPL (GNU Lesser General Public License).  Johannes Ruscheinski Jul-27, 2001.
//
//          Examples:           "fr5\?."         matches       "fr5?."
//                              "fred?2"         matches       "fred.2"
//                              "bob*"           matches       "bob.exe"
//                              "bill*"          matches       "bill"
//                              "joe?5"          doesn't match "joe15"
//                              "[A-Z]"          matches any single captial letter
//                              "cell[ABC].txt"  matches       "cellC.txt"
//                              "cell[^ABC].txt" doesn't match "cellA.txt"
//                              "cell[^0-9].txt" doesn't match "cell6.txt"
//                              "*"              matches everything
//                              "?A"             matches any single character followed by 'A'
//
bool Match(const char *pattern, const char *s, bool ignore_case) {
    const int MAX_ASTERISKS = 100;
    const char *back[MAX_ASTERISKS][2];
    int asterisk_index = 0;

    while (*s != '\0' or *pattern != '\0') {
        switch (*pattern) {
        case '*':
            if (asterisk_index == MAX_ASTERISKS)
                throw std::runtime_error("StringUtil::Match: too many levels of '*'!");
            back[asterisk_index][0] = pattern;
            back[asterisk_index][1] = s;
            ++asterisk_index;
            ++pattern;
            continue;
        goback:
            --asterisk_index;
            while (asterisk_index >= 0 and *back[asterisk_index][1] == '\0')
                --asterisk_index;
            if (asterisk_index < 0)
                return false;
            pattern = back[asterisk_index][0] + 1;
            s = ++back[asterisk_index][1];
            ++asterisk_index;
            continue;
        case '?':
            if (*s == '\0') {
                if (asterisk_index)
                    goto goback;
                return false;
            }
            break;
        case '[': { // match a character class
            ++pattern;
            const bool invert = *pattern == '^';
            if (invert)
                ++pattern;
            bool matched = false;
            for (/*empty*/; *pattern != ']' and *pattern != '\0'; ++pattern) {
                if (*pattern == '\\') {
                    ++pattern;

                    // Don't allow zero bytes in character classes.
                    if (*pattern == '\0')
                        break;
                }

                // Character range?
                if (pattern[1] == '-' and (pattern[2] != ']' and pattern[2] != '\0')) {
                    // Yes, we have a range!

                    char range_start(*pattern);
                    pattern += 2;
                    char range_end(*pattern);
                    char ch(*s);

                    if (ignore_case) {
                        ch          = tolower(ch);
                        range_start = tolower(range_start);
                        range_end   = tolower(range_end);
                    }

                    if (range_start > range_end) {
                        char msg[100+1];
                        std::sprintf(msg, "in StringUtil::Match:: invalid character range %c-%c!", range_start, range_end);
                        throw std::runtime_error(msg);
                    }

                    if (ch >= range_start and ch <= range_end)
                        matched = true;
                }
                else { // Regular single character match.
                    char ch(*s), pattern_ch(*pattern);
                    if (ignore_case) {
                        ch        = tolower(ch);
                        pattern_ch = tolower(pattern_ch);
                    }

                    if (ch == pattern_ch)
                        matched = true;
                }
            }
            if (*pattern != ']')
                throw std::runtime_error("in StringUtil::Match: missing ']'!");
            if (invert)
                matched = !matched;
            if (not matched) {
                if (asterisk_index)
                    goto goback;
                return false;
            }
            break;
        }
        case '\\': // take next character literally!
            ++pattern;
            [[fallthrough]];
        default: {
            int c1, c2;
            if (ignore_case) {
                c1 = tolower(*s);
                c2 = tolower(*pattern);
            } else { // case sensitive match
                c1 = *s;
                c2 = *pattern;
            }
            if (c1 != c2) {
                if (asterisk_index)
                    goto goback;
                return false;
            }
            break;
        }
        }

        if (*s != '\0')
            ++s;
        if (*pattern)
            ++pattern;
    }

    return true;
}


std::string &ReplaceString(const std::string &old_text, const std::string &new_text, std::string * const s,
                           const bool global)
{
    const std::string::size_type old_text_length(old_text.length());
    if (old_text_length == 0)
        return *s;

    // Find first occurrence of string to be replaced:
    std::string::size_type old_text_start_pos(s->find(old_text, 0));
    if (old_text_start_pos == std::string::npos)
        return *s;

    // Replace first occurrence:
    const std::string::size_type new_text_length(new_text.length());
    s->replace(old_text_start_pos, old_text_length, new_text);
    if (not global)
        return *s;

    std::string::size_type total_string_length(s->length());
    while ((old_text_start_pos = s->find(old_text, old_text_start_pos + new_text_length)) != std::string::npos) {
        s->replace(old_text_start_pos, old_text_length, new_text);
        total_string_length += new_text_length - old_text_length;
    }

    return *s;
}


std::string &RemoveChars(const std::string &remove_set, std::string * const s) {
    std::string result;
    result.reserve(s->size());

    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (remove_set.find(*ch) == std::string::npos)
            result += *ch;
    }

    return *s = result;
}


std::string &RemoveNotChars(const std::string &preserve_set, std::string * const s) {
    std::string result;
    result.reserve(s->size());

    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (preserve_set.find(*ch) != std::string::npos)
            result += *ch;
    }

    return *s = result;
}


bool IsUnsignedNumber(const std::string &s) {
    if (s.empty())
        return false;
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch)
        if (not isdigit(*ch))
            return false;

    return true;
}


bool IsUnsignedDecimalNumber(const std::string &s) {
    if (s.empty())
        return false;

    bool decimal_point_seen(false), at_leat_one_digit_seen(false);
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (likely(isdigit(*ch)))
            at_leat_one_digit_seen = true;
        else if (*ch == '.') {
            if (unlikely(decimal_point_seen))
                return false;
            decimal_point_seen = true;
        } else
            return false;
    }

    return at_leat_one_digit_seen;
}


// ISO8859_15ToUTF8 -- Converts a string encoded in Latin-9 (ISO_8859_15) into a string encoded
//                     in UTF-8. Note that no byte-order-marks (BOMs) are added to the output string.
//
std::string ISO8859_15ToUTF8(const std::string &text) {
    std::string result;
    result.reserve(2*text.length());
    for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch) {
        if (unlikely(*ch == '\xA4')) // Euro sign.
            result += "\xE2\x82\xAC";
        else if (unlikely(*ch == '\xA6')) // Latin capital letter S with caron.
            result += "\xC5\xA0";
        else if (unlikely(*ch == '\xA8')) // Latin small letter S with caron.
            result += "\xC5\xA1";
        else if (unlikely(*ch == '\xB4')) // Latin capital letter Z with caron.
            result += "\xC5\xBD";
        else if (unlikely(*ch == '\xB8')) // Latin small letter Z with caron.
            result += "\xC5\xBE";
        else if (unlikely(*ch == '\xBC')) // Latin capital ligature OE.
            result += "\xC5\x92";
        else if (unlikely(*ch == '\xBD')) // Latin small ligature OE.
            result += "\xC5\x93";
        else if (unlikely(*ch == '\xBE')) // Latin capital letter Y with diaeresis.
            result += "\xC5\xB8";
        else if (unlikely((*ch & 0x80u) == 0x80u)) { // Here we handle characters that have their high bit set.
            result += static_cast<char>(0xC0u | (static_cast<unsigned char>(*ch) >> 6u));
            result += static_cast<char>(0x80u | (static_cast<unsigned char>(*ch) & 0x3Fu));
        } else
            result += *ch;
    }

    return result;
}


namespace {


char CombineLetterAndDiacriticalMark(const char letter, const char diacritical_mark, const char unknown_char) {
    if (diacritical_mark == '\xA8') {
        switch (letter) {
        case 'A':
            return '\xC4';
        case 'a':
            return '\xE4';
        case 'E':
            return '\xCB';
        case 'e':
            return '\xEB';
        case 'I':
            return '\xCF';
        case 'i':
            return '\xEF';
        case 'O':
            return '\xD6';
        case 'o':
            return '\xF6';
        case 'U':
            return '\xDC';
        case 'u':
            return '\xFC';
        case 'y':
            return '\xFF';
        default:
            return unknown_char;
        }
    }

    if (diacritical_mark == '\xB4') {
        switch (letter) {
        case 'A':
            return '\xC1';
        case 'a':
            return '\xE1';
        case 'E':
            return '\xC9';
        case 'e':
            return '\xE9';
        case 'I':
            return '\xCD';
        case 'i':
            return '\xED';
        case 'O':
            return '\xD3';
        case 'o':
            return '\xF3';
        case 'U':
            return '\xDA';
        case 'u':
            return '\xFA';
        case 'Y':
            return '\xDD';
        case 'y':
            return '\xFD';
        default:
            return unknown_char;
        }
    }

    return unknown_char;
}


// ExtractDiacriticalMark -- attempts to extract a diacritical mark starting at "ch."  If this function fails, it
// returns "end", otherwise it returns "ch + 1".
//
std::string::const_iterator ExtractDiacriticalMark(std::string::const_iterator ch,
                                                   const std::string::const_iterator &end,
                                                   char * const diacritical_mark)
{
    if (unlikely(*ch != '\xC2'))
        return end;

    if (unlikely(++ch == end))
        return end;

    if (*ch == '\xA8' or *ch == '\xB4') {
        *diacritical_mark = *ch;
        return ch;
    }

    return end;
}


} // unnamed namespace


std::string UTF8ToISO8859_15(const std::string &text, const char unknown_char, const bool use_overlap_tokens,
                             const unsigned char overlap_token)
{
    std::string result;
    result.reserve(text.length());
    bool last_code_was_an_overlap_token(false);
    for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch) {
        if (use_overlap_tokens and unlikely(static_cast<unsigned char>(*ch) == overlap_token)) {
            // No string must start with an overlap token!
            if (unlikely(ch == text.begin()))
                throw std::runtime_error("in StringUtil::UTF8ToISO8859_15: found an overlap token at the start of a "
                                         "UTF-8 string!");

            last_code_was_an_overlap_token = true;
            continue;
        }

        if (unlikely(last_code_was_an_overlap_token)) {
            last_code_was_an_overlap_token = false;
            if (IsAsciiLetter(*(ch - 2))) {
                char diacritical_mark(0); // Initialised to shut up a buggy compiler!
                if (ExtractDiacriticalMark(ch, text.end(), &diacritical_mark) != text.end()) {
                    const char next_ch(CombineLetterAndDiacriticalMark(*(ch - 2), diacritical_mark, unknown_char));
                    if (next_ch != unknown_char)
                        result[result.size() - 1] = next_ch;
                    else if (unknown_char != '\0')
                        result += unknown_char;

                    ++ch;
                    if (unlikely(ch == text.end()))
                        return result;
                } else {
                    if (*ch == '\xC2') {
                        if (unknown_char != '\0')
                            result[result.size() - 1] = unknown_char;

                        ++ch;
                        if (ch != text.end())
                            ++ch;
                        if (unlikely(ch == text.end()))
                            return result;
                    } else if (unknown_char != '\0')
                        result += unknown_char;
                }
            } else if (IsAsciiLetter(*ch)) {
                const char next_ch(CombineLetterAndDiacriticalMark(*ch, *(ch - 2), unknown_char));
                if (next_ch != unknown_char)
                    result += next_ch;
                else if (unknown_char != '\0')
                    result += unknown_char;
            } else if (unknown_char != '\0')
                result += unknown_char;
            continue;
        }

        if (likely((static_cast<unsigned char>(*ch) & 0x80) == 0)) // US-ASCII subset.
            result += *ch;
        else if (unlikely(*ch == '\xC2')) { // We have a special double-byte sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            } else if (*ch == '\xA8' or *ch == '\xB4') {
                if (unlikely(ch + 1 == text.end()) or not use_overlap_tokens) {
                    if (unknown_char != '\0')
                        result += unknown_char;
                }
            } else if (*ch != '\0')
                result += *ch;
        } else if (unlikely(*ch == '\xC5')) { // We have a special double-byte sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            switch (*ch) {
            case '\xA0': // Latin small letter S with caron.
                result += '\xA6';
                break;
            case '\xA1': // Latin small letter S with caron.
                result += '\xA8';
                break;
            case '\xBD': // Latin capital letter Z with caron.
                result += '\xB4';
                break;
            case '\xBE': // Latin small letter Z with caron.
                result += '\xB8';
                break;
            case '\x92': // Latin capital ligature OE.
                result += '\xBC';
                break;
            case '\x93': // Latin small ligature OE.
                result += '\xBD';
                break;
            case '\xB8': // Latin capital letter Y with diaeresis.
                result += '\xBE';
                break;
            default:
                if (unknown_char != '\0')
                    result += unknown_char;
            }
        } else if (unlikely(*ch == '\xCC')) { // Deal with composing diacritical marks.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            // Paranoia test:
            if (unlikely(result.empty())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                continue;
            }

            switch (*ch) {
            case '\x81': // Acute.
                switch (result[result.size() - 1]) {
                case 'e':
                    result[result.size() - 1] = '\xE9';
                    break;
                }
                break;
            default:
                // If we get here we have a diacritical mark that we can't handle and we just drop it!
                break;
            }
        } else if (unlikely(*ch == '\xCE')) { // We have a special double-byte sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            switch (*ch) {
            case '\xB3': // Greek lowercase gamma.
                result += "gamma";
                break;
            case '\x93': // Greek uppercase gamma.
                result += "Gamma";
                break;
            default:
                if (unknown_char != '\0')
                    result += unknown_char;
            }
        } else if (unlikely(*ch == '\xCF')) { // We have a special double-byte sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            switch (*ch) {
            case '\x90': // Greek lowercase beta.
                result += '\xDF';
                break;
            default:
                if (unknown_char != '\0')
                    result += unknown_char;
            }
        } else if (unlikely(*ch == '\xE2')) { // We have a triple-byte sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            char second_byte(*ch);

            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            if (second_byte == '\x82' and *ch == '\xAC') // Euro sign.
                result += '\xA4';
            else if (second_byte == '\x80' and *ch == '\x9C') // Left double quotation mark.
                result += '"';
            else if (second_byte == '\x80' and *ch == '\x9D') // Right double quotation mark.
                result += '"';
            else if (second_byte == '\x80' and *ch == '\x98') // Left single quotation mark.
                result += '`';
            else if (second_byte == '\x80' and *ch == '\x99') // Right single quotation mark.
                result += '\'';
            else if (second_byte == '\x80' and *ch == '\xA0') // Dagger.
                result += '+';
            else if (second_byte == '\x80' and *ch == '\x93') // Some kind of hyphen.
                result += '-';
            else if (second_byte == '\x80' and *ch == '\x94') // Em-dash?
                result += "--";
            else if (second_byte == '\x88' and *ch == '\x92') // Another kind of hyphen?
                result += '-';
            else if (second_byte == '\x88' and *ch == '\x88') // Another kind of hyphen?
                result += '-';
            else if (second_byte == '\x88' and *ch == '\xBC') // A twiddle.
                result += '~';
            else if (second_byte == '\x88' and *ch == '\x97') // Some kind of asterisk.
                result += '*';
            else if (unknown_char != '\0')
                result += unknown_char;
        } else if (unlikely(*ch == '\xEF')) { // We have a triple-byte ligature sequence.
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            char second_byte(*ch);

            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            if (second_byte == '\xAC' and *ch == '\x83') // "ffi" ligature.
                result += "ffi";
            else if (second_byte == '\xAC' and *ch == '\x82') // "fl" ligature.
                result += "fl";
            else if (second_byte == '\xAC' and *ch == '\x81') // "fi" ligature.
                result += "fi";
            else if (unknown_char != '\0')
                result += unknown_char;
        } else if ((static_cast<unsigned char>(*ch) & 0xC0u) == 0xC0u) { // We have a common double-byte sequence.
            unsigned char next_ch((*ch & 0x03u) << 6u);
            ++ch;
            if (unlikely(ch == text.end())) {
                if (unknown_char != '\0')
                    result += unknown_char;
                return result;
            }

            next_ch |= static_cast<unsigned char>(*ch) & 0x3Fu;
            result += static_cast<char>(next_ch);
        } else if (unknown_char != '\0') // Hopefully the common case.
            result += unknown_char;
    }

    if (unlikely(last_code_was_an_overlap_token and unknown_char != '\0'))
        result += unknown_char;

    return result;
}


bool IsPossiblyUTF8(const std::string &text) {
    for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch) {
        // Multibyte character?
        if (static_cast<unsigned char>(*ch) & 0x80u) {
            // Doublebyte character?
            if ((static_cast<unsigned char>(*ch) & 0xE0u) == 0xC0u) {
                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;
            }

            // Tripplebyte character?
            else if ((static_cast<unsigned char>(*ch) & 0xF0u) == 0xE0u) {
                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;

                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;
            }

            // Quadruplebyte character?
            else if ((static_cast<unsigned char>(*ch) & 0xF8u) == 0xF0u) {
                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;

                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;

                ++ch;
                if (unlikely(ch == text.end()))
                    return false;
                else if ((static_cast<unsigned char>(*ch) & 0xC0u) != 0x80u)
                    return false;
            }

            // We have non-UTF-8 data:
            else
                return false;
        }
    }

    return true;
}


// The following mapping of the 256 character codes is used by SanitizeText.  It leaves most characters
// unchanged except for replacing a non-breaking space (0xA0) with a regular space (0x20) and various
// characters appearing in the range 0x80-0x9F.  In that range it assumes that characters are from the
// Windows 1250 character set and it attempts to map as many as possible to similar ISO 8859-15
// (a.k.a. Latin-9) characters.
const char CHAR_MAP[] = {
    '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F',
    '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
    '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', '\x28', '\x29', '\x2A', '\x2B', '\x2C', '\x2D', '\x2E', '\x2F',
    '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37', '\x38', '\x39', '\x3A', '\x3B', '\x3C', '\x3D', '\x3E', '\x3F',
    '\x40', '\x41', '\x42', '\x43', '\x44', '\x45', '\x46', '\x47', '\x48', '\x49', '\x4A', '\x4B', '\x4C', '\x4D', '\x4E', '\x4F',
    '\x50', '\x51', '\x52', '\x53', '\x54', '\x55', '\x56', '\x57', '\x58', '\x59', '\x5A', '\x5B', '\x5C', '\x5D', '\x5E', '\x5F',
    '\x60', '\x61', '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', '\x68', '\x69', '\x6A', '\x6B', '\x6C', '\x6D', '\x6E', '\x6F',
    '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', '\x78', '\x79', '\x7A', '\x7B', '\x7C', '\x7D', '\x7E', '\x7F',

    // Mappings from Windows 1250 start.
    '\xA4', '\x00', '\x27', '\x00', '\x22', '\x00', '\x00', '\x00', '\x00', '\x00', '\xA6', '\x3C', '\x53', '\x54', '\xB4', '\x5A',
    '\x00', '\x60', '\x27', '\x22', '\x22', '\x00', '\x2D', '\x2D', '\x00', '\x00', '\xA8', '\x3E', '\x73', '\x74', '\xB8', '\x7A',
    // Mappings from Windows 1250 end.

    '\x20', '\xA1', '\xA2', '\xA3', '\xA4', '\xA5', '\xA6', '\xA7', '\xA8', '\xA9', '\xAA', '\xAB', '\xAC', '\xAD', '\xAE', '\xAF',
    '\xB0', '\xB1', '\xB2', '\xB3', '\xB4', '\xB5', '\xB6', '\xB7', '\xB8', '\xB9', '\xBA', '\xBB', '\xBC', '\xBD', '\xBE', '\xBF',
    '\xC0', '\xC1', '\xC2', '\xC3', '\xC4', '\xC5', '\xC6', '\xC7', '\xC8', '\xC9', '\xCA', '\xCB', '\xCC', '\xCD', '\xCE', '\xCF',
    '\xD0', '\xD1', '\xD2', '\xD3', '\xD4', '\xD5', '\xD6', '\xD7', '\xD8', '\xD9', '\xDA', '\xDB', '\xDC', '\xDD', '\xDE', '\xDF',
    '\xE0', '\xE1', '\xE2', '\xE3', '\xE4', '\xE5', '\xE6', '\xE7', '\xE8', '\xE9', '\xEA', '\xEB', '\xEC', '\xED', '\xEE', '\xEF',
    '\xF0', '\xF1', '\xF2', '\xF3', '\xF4', '\xF5', '\xF6', '\xF7', '\xF8', '\xF9', '\xFA', '\xFB', '\xFC', '\xFD', '\xFE', '\xFF',
};


bool SanitizeText(std::string * const text) {
    std::string sanitized_text;
    sanitized_text.reserve(text->length());
    bool changed(false);

    for (std::string::const_iterator ch(text->begin()); ch != text->end(); ++ch) {
        char new_ch = CHAR_MAP[static_cast<unsigned char>(*ch)];
        if (unlikely(new_ch != *ch))
            changed = true;
        if (likely(isprint(new_ch) or isspace(new_ch)))
            sanitized_text += new_ch;
        else /* skip bad character */
            changed = true;
    }

    if (changed)
        *text = sanitized_text;

    return changed;
}


// WordWrap -- Wrap long lines to a fixed number of characters.
//
std::string WordWrap(const std::string &text, const unsigned target_length) {
    std::string new_text("");
    std::string::size_type line_start(0), line_end, spaceAt;

    while(line_start != std::string::npos and line_start < text.length()) {
        line_end = text.find("\n", line_start);

        // If this was the last line, we're done
        if (line_end == std::string::npos) {
            new_text += text.substr(line_start);
            return new_text;
        }

        // If this is a long line, split it
        if (line_end - line_start + 1 > target_length) {
            spaceAt = text.rfind(" ", line_start + target_length);
            if (spaceAt == std::string::npos or spaceAt < line_start)
                spaceAt = text.find(" ", line_start);
            if (spaceAt == std::string::npos)
                spaceAt = text.length() - 1;
            line_end = (line_end < spaceAt ? line_end : spaceAt);

            new_text += text.substr(line_start, line_end - line_start + 1) + "\n";
        }
        // If it is a short line, just add it
        else
            new_text += text.substr(line_start, line_end - line_start + 1);

        line_start = line_end + 1;

    }
    return new_text;
}


size_t strlcpy(char * const dest, const char * const src, const size_t dest_size) {
    const size_t src_len = __builtin_strlen(src);
    if (dest_size == 0)
        return src_len;

    size_t len = src_len;
    if (len >= dest_size)
        len = dest_size - 1;

    __builtin_memcpy(dest, src, len);
    dest[len] = '\0';

    return src_len;
}


size_t strlcat(char * const dest, const char * const src, const size_t dest_size) {
    const size_t src_len = __builtin_strlen(src);
    if (dest_size == 0)
        return src_len;

    const size_t dest_len = __builtin_strlen(dest);
    assert(dest_len < dest_size);

    size_t len = src_len;
    if (len + dest_len >= dest_size)
        len = dest_size - 1 - dest_len;

    __builtin_memcpy(dest + dest_len, src, len);
    dest[dest_len + len] = '\0';

    return src_len + dest_len;
}


bool IsAlphanumeric(const std::string &s) {
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (not isalnum(*ch))
            return false;
    }

    return true;
}


unsigned EditDistance(const std::string &s1, const std::string &s2) {
    const unsigned MAX_STACK_TABLEAU_SIZE = 1024; // Cutoff for allocation on stack.
    const unsigned TABLEAU_SIZE = s1.size() * s2.size();
    unsigned *distances;
    if (TABLEAU_SIZE > MAX_STACK_TABLEAU_SIZE)
        distances = new unsigned[TABLEAU_SIZE];
    else
        distances = reinterpret_cast<unsigned *>(::alloca(TABLEAU_SIZE * sizeof(unsigned)));

    std::string::size_type distance(0);

    if (TABLEAU_SIZE > MAX_STACK_TABLEAU_SIZE)
        delete [] distances;

    return distance;
}


std::string LongestCommonSubstring(const std::string &s1, const std::string &s2) {
    if (s1 == s2)
        return s2;

    std::string longest_common_substring;
    unsigned current_length = 0;

    for (unsigned i = 0; i < s1.length(); ++i) {
        for (unsigned j = 0; j < s2.length(); ++j) {
            if (s2[j] == s1[i]) {
                ++current_length;

                // Scan for sequential match:
                for (unsigned k = 1; k < s2.length() and k < s1.length(); ++k) {
                    if (s2[j + k] == s1[i + k])
                        ++current_length;
                    else
                        break;
                }

                if(current_length > longest_common_substring.length())
                    longest_common_substring = s2.substr(j, current_length);

                current_length = 0;
            }
        }
    }

    return longest_common_substring;
}


std::string Md5(const std::string &s) {
    char cryptographic_hash[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char *>(s.c_str()), s.length(),
        reinterpret_cast<unsigned char *>(cryptographic_hash));

    return std::string(cryptographic_hash, MD5_DIGEST_LENGTH);
}


uint64_t Md5As64Bits(const std::string &s) {
    uint64_t cryptographic_hash[MD5_DIGEST_LENGTH / sizeof(uint64_t)];
    MD5(reinterpret_cast<const unsigned char *>(s.c_str()), s.length(),
        reinterpret_cast<unsigned char *>(cryptographic_hash));

    uint64_t folded_hash(cryptographic_hash[0]);
    for (unsigned i(1); i < (MD5_DIGEST_LENGTH / sizeof(uint64_t)); ++i)
        folded_hash ^= cryptographic_hash[i];
    return folded_hash;
}


std::string Sha1(const std::string &s) {
    char cryptographic_hash[SHA_DIGEST_LENGTH];
    ::SHA1(reinterpret_cast<const unsigned char *>(s.c_str()), s.length(),
           reinterpret_cast<unsigned char *>(cryptographic_hash));

    return std::string(cryptographic_hash, SHA_DIGEST_LENGTH);
}


size_t Sha1Hash(const std::string &s) {
    constexpr size_t remainder(SHA_DIGEST_LENGTH - (SHA_DIGEST_LENGTH / (sizeof(size_t))) * sizeof(size_t));
    char cryptographic_hash[SHA_DIGEST_LENGTH + (remainder == 0 ? 0 : sizeof(size_t) - remainder)];
    if (remainder != 0)
        __builtin_memset(cryptographic_hash + SHA_DIGEST_LENGTH, '\0', remainder);
    ::SHA1(reinterpret_cast<const unsigned char *>(s.c_str()), s.length(),
           reinterpret_cast<unsigned char *>(cryptographic_hash));

    size_t hash(0);
    for (const char *cp = cryptographic_hash; cp < cryptographic_hash + SHA_DIGEST_LENGTH + remainder;
         cp += sizeof(size_t))
    {
        size_t temp;
        __builtin_memcpy(&temp, cp, sizeof temp);
        hash ^= temp;
    }

    return hash;
}


#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8) + (uint32_t)(((const uint8_t *)(d))[0]))


uint32_t SuperFastHash(const char * data, unsigned len) {
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 or data == nullptr)
        return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (; len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
    case 3: hash += get16bits (data);
        hash ^= hash << 16;
        hash ^= data[sizeof (uint16_t)] << 18;
        hash += hash >> 11;
        break;
    case 2: hash += get16bits (data);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;
    case 1: hash += *data;
        hash ^= hash << 10;
        hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}


uint32_t Adler32(const char * const s, const size_t s_length) {
    const uint32_t MOD_ADLER(65521u);
    const uint8_t *data(reinterpret_cast<const uint8_t * const>(s)); // Pointer to the data to be summed.
    size_t len(s_length);                                            // Length in bytes.
    uint32_t low(1), high(0);

    while (len) {
        size_t tlen = len > 5550 ? 5550 : len;
        len -= tlen;
        do {
            low += *data++;
            high += low;
        } while (--tlen);
        low  = (low & 0xffffu)  + (low >> 16)  * (65536u - MOD_ADLER);
        high = (high & 0xffffu) + (high >> 16) * (65536u - MOD_ADLER);
    }

    // It can be shown that "low" <= 0x1013a here, so a single subtract will do:
    if (low >= MOD_ADLER)
        low -= MOD_ADLER;

    // It can be shown that "high" can reach 0xffef1 here:
    high = (high & 0xffffu) + (high >> 16) * (65536u - MOD_ADLER);

    if (high >= MOD_ADLER)
        high -= MOD_ADLER;

    return (high << 16) | low;
}


std::string &Escape(const char escape_char, const char * const chars_to_escape, std::string * const s) {
    if (unlikely(s->empty()))
        return *s;

    std::string escaped_string;
    escaped_string.reserve(s->length());

    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (std::strchr(chars_to_escape, *ch) != nullptr or *ch == escape_char)
            escaped_string += escape_char;
        escaped_string += *ch;
    }

    return (*s = escaped_string);
}


std::string &Escape(const char escape_char, const char char_to_escape, std::string * const s) {
    if (unlikely(s->empty()))
        return *s;

    std::string escaped_string;
    escaped_string.reserve(s->length());

    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (*ch == char_to_escape or *ch == escape_char)
            escaped_string += escape_char;
        escaped_string += *ch;
    }

    return (*s = escaped_string);
}


bool SplitOnString(const std::string &s, const std::string &separator, std::string * const part1,
                   std::string * const part2,
                   const bool allow_empty_parts)
{
    const std::string::size_type separator_pos(s.find(separator));
    if (separator_pos == std::string::npos)
        return false;

    *part1 = s.substr(0, separator_pos);
    if (part1->empty() and not allow_empty_parts)
        return false;

    *part2 = s.substr(separator_pos + separator.length());
    if (part2->empty() and not allow_empty_parts)
        return false;

    return true;
}


bool SplitOnStringThenTrim(const std::string &s, const std::string &separator, const std::string &trim_chars,
                           std::string * const part1, std::string * const part2, const bool allow_empty_parts)
{
    const std::string::size_type separator_pos(s.find(separator));
    if (separator_pos == std::string::npos)
        return false;

    *part1 = s.substr(0, separator_pos);
    Trim(trim_chars, part1);
    if (part1->empty() and not allow_empty_parts)
        return false;

    *part2 = s.substr(separator_pos + separator.length());
    Trim(trim_chars, part2);
    if (part2->empty() and not allow_empty_parts)
        return false;

    return true;
}


// RemoveDuplicatesFromList -- Remove duplicate values from a list.
//
void RemoveDuplicatesFromList(std::list<std::string> * const values) {
    // Sanity check:
    if (values->empty())
        return;

    // The set of values we have seen:
    std::set<std::string> seen;

    // Remove unwanted phrases:
    std::list<std::string>::iterator value(values->begin());
    while (value != values->end()) {
        std::string lowercase_value(*value);
        StringUtil::ToLower(&lowercase_value);

        if (seen.find(lowercase_value) != seen.end())
            value = values->erase(value);
        else {
            seen.insert(lowercase_value);
            ++value;
        }
    }
}


// RemoveHead -- Remove front of string up to a certain delimiter and return the removed part.
//               delimiter_string removed completely.
//
std::string ExtractHead(std::string * const tail, const std::string &delimiter_string,
                        const std::string::size_type start)
{
    if (unlikely(delimiter_string.empty()))
        return "";

    if (tail->empty())
        return "";

    // Search for first occurrence of delimiter that appears after start
    std::string::size_type delimiter_pos = tail->find(delimiter_string, 0);

    if (delimiter_pos == std::string::npos)
        return "";

    // Get the head by extracting from start up to delimiter_pos:
    std::string head = tail->substr(start, delimiter_pos);

    *tail = tail->substr(delimiter_pos + delimiter_string.length());

    return head;
}


// SplitListValues -- Split any list entry that includes the delimiter into multiple values.
//
void SplitListValues(std::list<std::string> * const values, const std::string &delimiter) {
    // Sanity check:
    if (values->empty())
        return;

    // Join the list into one string, then split it again:
    std::string joined_list;
    StringUtil::Join(*values, delimiter, &joined_list);
    StringUtil::Split(joined_list, delimiter, values, /* suppress_empty_components = */ false);
}


std::string GetPrintableChars() {
    static std::string printables;
    static bool initialised(false);
    if (not initialised) {
        for (int ch = CHAR_MIN; ch <= CHAR_MAX; ++ch)
            if (isprint(ch))
                printables += static_cast<char>(ch);
        initialised = true;
    }

    return printables;
}


std::string GetNonprintableChars() {
    static std::string nonprintables;
    static bool initialised(false);
    if (not initialised) {
        for (int ch = CHAR_MIN; ch <= CHAR_MAX; ++ch)
            if (not isprint(ch))
                nonprintables += static_cast<char>(ch);
        initialised = true;
    }

    return nonprintables;
}


std::string GetPunctuationChars() {
    static bool initialized(false);
    static std::string punctuation;
    if (not initialized) {
        for (int ch(CHAR_MIN); ch <= CHAR_MAX; ++ch) {
            if (ispunct(ch))
                punctuation += static_cast<char>(ch);
        }
    }

    return punctuation;
}


std::string &CapitalizeWord(std::string * const word) {
    std::string::iterator ch(word->begin());
    if (likely(ch != word->end())) {
        *ch = toupper(*ch);
        for (++ch; ch != word->end(); ++ch)
            *ch = tolower(*ch);
    }

    return *word;
}


namespace {


// IsInitialWordChar -- returns true if "ch" is considered to be the initial character of a "word".  ("Words" in this
//                      context include numbers).  Helper function for StringUtil::InitialCapsWords.
//
inline bool IsInitialWordChar(const char ch) {
    return StringUtil::IsAlphanumeric(ch);
}


// IsWordChar -- returns true if "ch" is considered to be a character of a "word".  ("Words" in this context include
//               numbers).  Helper function for StringUtil::InitialCapsWords.
//
inline bool IsWordChar(const char ch)
{
        return StringUtil::IsAlphanumeric(ch) or ch == '-' or ch == '\'';
}


} // unnamed namespace


std::string InitialCapsWords(const std::string &text) {
    std::string retval;
    retval.reserve(text.size());

    bool in_word(false);
    unsigned hyphen_count(0);
    for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch) {
        if (in_word) {
            if (*ch == '-')
                ++hyphen_count;
            retval += *ch;
            if (hyphen_count > 1) {
                hyphen_count = 0;
                in_word = false;
            } else
                in_word = IsWordChar(*ch);
        } else if (IsInitialWordChar(*ch)) {
            in_word = true;
            retval += toupper(*ch);
        } else
            retval += *ch;
    }

    return retval;
}


std::string CommonPrefix(const std::string &s1, const std::string &s2) {
    std::string common_prefix;
    common_prefix.reserve(std::min(s1.size(), s2.size()));

    std::string::const_iterator ch1(s1.begin()), ch2(s2.begin());
    for (/* Empty! */; ch1 != s1.end() and ch2 != s2.end() and *ch1 == *ch2; ++ch1, ++ch2)
        common_prefix += *ch1;

    return common_prefix;
}


std::string CommonSuffix(const std::string &s1, const std::string &s2) {
    std::string common_suffix;
    common_suffix.reserve(std::min(s1.size(), s2.size()));

    std::string::const_reverse_iterator ch1(s1.rbegin()), ch2(s2.rbegin());
    for (/* Empty! */; ch1 != s1.rend() and ch2 != s2.rend() and *ch1 == *ch2; ++ch1, ++ch2)
        common_suffix += *ch1;

    std::reverse(common_suffix.begin(), common_suffix.end());
    return common_suffix;
}


size_t CharCount(const char *s, const char count_char) {
    size_t count(0);
    for (/* Empty. */; *s != '\0'; ++s) {
        if (*s == count_char)
            ++count;
    }

    return count;
}


char *FastFormat(void *buffer, const size_t max_length, const char *format, ...) {
    char *output = static_cast<char *>(buffer);

    va_list args;
    va_start(args, format);
    const int written = ::vsnprintf(output, max_length, format, args);
    va_end(args);

    // If there was insufficient room, throw
    if (written >= static_cast<int>(max_length))
        throw std::runtime_error("in StringUtil::FastFormat: Error formatting std::string object!");

    return output;
}


std::string Format(const char *format, ...){
    size_t bufsize(1024);

    for (;;) {

        char * const buffer(reinterpret_cast<char *>(::alloca(bufsize)));

        va_list args;
        va_start(args, format);
        const int written = ::vsnprintf(buffer, bufsize, format, args);
        va_end(args);

        // If there was insufficient room, set size to what is needed and try again
        // if vsnprintf fails it returns the number of bytes NEEDED so we can try again and succeed
        if (written >= static_cast<int>(bufsize)) {
            bufsize = written + 1;
            continue;
        }

        if (written < 0)
            throw std::runtime_error("in StringUtil::Format: Error formatting std::string object.");

        /**
         * Returning a local pointer? Not really. std::string will get constructed from this (return
         * value optimization).
         */
        return buffer;
    }
}


bool IsAlphabetic(const std::string &test_string) {
    for (std::string::const_iterator ch(test_string.begin()); ch != test_string.end(); ++ch) {
        if (not isalpha(*ch))
            return false;
    }

    return true;
}


std::string::size_type FindAnyOf(const std::string &test_string, const std::string &match_set) {
    for (std::string::const_iterator ch(test_string.begin()); ch != test_string.end(); ++ch) {
        const std::string::size_type match_pos(match_set.find(*ch));
        if (match_pos != std::string::npos)
            return ch - test_string.begin();
    }

    return std::string::npos;
}


std::string::size_type RFindAnyOf(const std::string &test_string, const std::string &match_set)
{
        for (std::string::const_reverse_iterator ch(test_string.rbegin()); ch != test_string.rend(); ++ch) {
                const std::string::size_type match_pos(match_set.find(*ch));
                if (match_pos != std::string::npos)
                        return test_string.rend() - ch - 1;
        }

        return std::string::npos;
}


std::string::size_type NthWordByteOffset(const std::string &target, const size_t nth,
                                         const std::string &word_separator_characters)
{
    std::string::size_type word_offset(0);

    for (std::string::const_iterator index(target.begin()); index != target.end(); ++index) {
        // No new word? Keep iterating through text:
        if (word_separator_characters.find_first_of(*index) == std::string::npos)
            continue;

        // Walk through all the contiguous word separator characters. This is NOT a new word:
        for (++index; index != target.end(); ++index)
            if (word_separator_characters.find_first_of(*index) == std::string::npos)
                break;

        // Reached end of string => return npos:
        if (index == target.end())
            return std::string::npos;

        // Aha, found a word separator.  Increment our word_offset and check if we are done:
        if (word_offset >= nth)
            return index - target.begin();

    }

    return std::string::npos;
}


std::string Context(const std::string &text, const std::string::size_type offset, const std::string::size_type length)
{
    std::string::size_type start = std::max<int>(offset - (length / 2), 0);
    std::string::size_type end = std::min<int>(offset + (length / 2), text.size());
    std::string temp(text.substr(start, end - start));
    return CollapseWhitespace(&temp);
}


std::string ExtractSensibleSubphrase(const std::string &source_text, const std::string &delimiters,
                                     const unsigned minimum, const unsigned maximum)
{
    // If the source_text is smaller than our minimum requirement, consider that as one of the conditions for a "good phrase".
    if (minimum > source_text.size())
        return source_text;

    // ignore any text beyond maximum length by working with a substring no longer than maximum.
    const std::string work_text(source_text.substr(0, maximum));

    const char *start_position(work_text.c_str() + minimum);

    // If one of our delimiters is found, split the string at that point
    for (const char *delimiter = delimiters.c_str(); *delimiter; ++delimiter) {
        const char *found_at = ::strchr(start_position, *delimiter);
        if (found_at == nullptr) // delimiter doesn't exist in this text? try next delimiter.
            continue;

        // Aha! We found a delimiter.
        if (::strchr(".?!", *found_at)) // If we are a sentence delimiter, include us in the subphrase.
            ++found_at;

        return work_text.substr(0, found_at - start_position + minimum);
    }

    // No delimiter found, return the entire work_text.
    return work_text;
}


std::string CStyleEscape(const char ch) {
    std::string escaped_text;
    switch (ch) {
    case '\'':
        escaped_text += '\\';
        escaped_text += '\'';
        break;
    case '\n':
        escaped_text += '\\';
        escaped_text += 'n';
        break;
    case '\t':
        escaped_text += '\\';
        escaped_text += 't';
        break;
    case '\b':
        escaped_text += '\\';
        escaped_text += 'b';
        break;
    case '\r':
        escaped_text += '\\';
        escaped_text += 'r';
        break;
    case '\f':
        escaped_text += '\\';
        escaped_text += 'f';
        break;
    case '\v':
        escaped_text += '\\';
        escaped_text += 'v';
        break;
    case '\\':
        escaped_text += '\\';
        escaped_text += '\\';
        break;
    case '\a':
        escaped_text += '\\';
        escaped_text += 'a';
        break;
    default:
        if (isprint(ch))
            escaped_text += ch;
        else {
            char buf[10 + 1];
            std::sprintf(buf, "\\%03o", static_cast<unsigned>(static_cast<unsigned char>(ch)));
            escaped_text += buf;
        }
    }

    return escaped_text;
}


std::string CStyleEscape(const std::string &unescaped_text) {
    std::string escaped_text;
    escaped_text.reserve(unescaped_text.size() * 2);
    for (std::string::const_iterator ch(unescaped_text.begin()); ch != unescaped_text.end(); ++ch) {
        switch (*ch) {
        case '"':
            escaped_text += '\\';
            escaped_text += '"';
            break;
        case '\n':
            escaped_text += '\\';
            escaped_text += 'n';
            break;
        case '\t':
            escaped_text += '\\';
            escaped_text += 't';
            break;
        case '\b':
            escaped_text += '\\';
            escaped_text += 'b';
            break;
        case '\r':
            escaped_text += '\\';
            escaped_text += 'r';
            break;
        case '\f':
            escaped_text += '\\';
            escaped_text += 'f';
            break;
        case '\v':
            escaped_text += '\\';
            escaped_text += 'v';
            break;
        case '\\':
            escaped_text += '\\';
            escaped_text += '\\';
            break;
        case '\a':
            escaped_text += '\\';
            escaped_text += 'a';
            break;
        default:
            if (isprint(*ch))
                escaped_text += *ch;
            else {
                char buf[10 + 1];
                std::sprintf(buf, "\\%03o", static_cast<unsigned>(static_cast<unsigned char>(*ch)));
                escaped_text += buf;
            }
        }
    }

    return escaped_text;
}


char CStyleUnescape(const char c) {
    switch (c) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'b':
        return '\b';
    case 'r':
        return '\r';
    case 'f':
        return '\f';
    case 'v':
        return '\v';
    case 'a':
        return '\a';
    case '\\':
        return '\\';
    default:
        return c;
    }
}


std::string CStyleUnescape(const std::string &escaped_text) {
    std::string unescaped_text;
    for (std::string::const_iterator ch(escaped_text.begin()); ch != escaped_text.end(); ++ch) {
        if (*ch == '\\') {
            ++ch;
            if (unlikely(ch == escaped_text.end()))
                throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of escaped string (1)!");
            if (*ch == 'x' or *ch == 'X') { // Hexadecimal escape.
                ++ch;
                if (unlikely(ch == escaped_text.end()))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of escaped string (2)!");
                ++ch;
                if (unlikely(ch == escaped_text.end()))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of hex escape (1)!");
                std::string hex_digits(1, *ch);
                ++ch;
                if (unlikely(ch == escaped_text.end()))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of hex escape (2)!");
                hex_digits += *ch;
                unsigned char_value;
                if (unlikely(not StringUtil::ToUnsigned(hex_digits, &char_value, 16)))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: bad hex escape!");
                unescaped_text += static_cast<char>(char_value);
            } else if (*ch >= '0' and *ch <= '7') { // Octal escape.
                std::string octal_digits(1, *ch);
                ++ch;
                if (unlikely(ch == escaped_text.end()))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of octal escape (2)!");
                octal_digits += *ch;
                ++ch;
                if (unlikely(ch == escaped_text.end()))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unexpected end of octal escape (3)!");
                octal_digits += *ch;
                unsigned char_value;
                if (unlikely(not StringUtil::ToUnsigned(octal_digits, &char_value, 8)))
                    throw std::runtime_error("in StringUtil::CStyleUnescape: bad octal escape (\\0" + octal_digits
                                             + ")!");
                unescaped_text += static_cast<char>(char_value);
            } else {
                switch (*ch) {
                case 'n':
                    unescaped_text += '\n';
                    break;
                case 't':
                    unescaped_text += '\t';
                    break;
                case 'b':
                    unescaped_text += '\b';
                    break;
                case 'r':
                    unescaped_text += '\r';
                    break;
                case 'f':
                    unescaped_text += '\f';
                    break;
                case 'v':
                    unescaped_text += '\v';
                    break;
                case 'a':
                    unescaped_text += '\a';
                    break;
                case '\\':
                    unescaped_text += '\\';
                    break;
                case '\'':
                    unescaped_text += '\'';
                    break;
                case '"':
                    unescaped_text += '"';
                    break;
                default:
                    throw std::runtime_error("in StringUtil::CStyleUnescape: unknown escape '\\" + CStyleEscape(*ch)
                                             + "' (2)!");
                }
            }
        }
        else
            unescaped_text += *ch;
    }

    return unescaped_text;
}


std::string Chomp(std::string * const line) {
    if (not line->empty()) {
        // Remove a trailing linefeed, if present:
        if ((*line)[line->size() - 1] == '\n')
            line->resize(line->size() - 1);

        // Remove a trailing carriage return, if present:
        if (not line->empty()) {
            if ((*line)[line->size() - 1] == '\r')
                line->resize(line->size() - 1);
        }
    }

    return *line;
}


bool ConsistsOf(const std::string &s, const std::set<char> &set) {
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (set.find(*ch) == set.end())
            return false;
    }

    return true;
}


bool ContainsAtLeastOneLowercaseLetter(const std::string &s) {
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (islower(*ch))
            return true;
    }

    return false;
}


static inline bool CaseInsensitiveEqual(const char ch1, const char ch2) {
    return std::toupper(ch1) == std::toupper(ch2);
}


size_t FindCaseInsensitive(const std::string &haystack, const std::string &needle) {
    const auto iter(std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                CaseInsensitiveEqual));
    return iter == haystack.end() ? std::string::npos : iter - haystack.begin();
}


std::string Filter(const std::string &source, const std::string &remove_set) {
    std::string filtered_string;
    filtered_string.reserve(source.size());

    for (const char ch : source) {
        if (remove_set.find(ch) == std::string::npos)
            filtered_string += ch;
    }

    return filtered_string;
}


std::string &ReplaceSection(std::string * const s, const size_t start_index, const size_t section_length,
                            const std::string &replacement)
{
    if (unlikely(start_index + section_length > s->length()))
        throw std::out_of_range("in StringUtil::ReplaceSection: impossible replacement request!");

    return *s = s->substr(0, start_index) + replacement + s->substr(start_index + section_length);
}


std::string ISO8859_15ToUTF8(const char &latin9_char) {
    if (unlikely(latin9_char == '\xA4')) // Euro sign.
        return "\xE2\x82\xAC";
    else if (unlikely(latin9_char == '\xA6')) // Latin capital letter S with caron.
        return "\xC5\xA0";
    else if (unlikely(latin9_char == '\xA8')) // Latin small letter S with caron.
        return "\xC5\xA1";
    else if (unlikely(latin9_char == '\xB4')) // Latin capital letter Z with caron.
        return "\xC5\xBD";
    else if (unlikely(latin9_char == '\xB8')) // Latin small letter Z with caron.
        return "\xC5\xBE";
    else if (unlikely(latin9_char == '\xBC')) // Latin capital ligature OE.
        return "\xC5\x92";
    else if (unlikely(latin9_char == '\xBD')) // Latin small ligature OE.
        return "\xC5\x93";
    else if (unlikely(latin9_char == '\xBE')) // Latin capital letter Y with diaeresis.
        return "\xC5\xB8";
    else if (unlikely((latin9_char & 0x80u) == 0x80u)) { // Here we handle characters that have their high bit set.
        std::string result;
        result += static_cast<char>(0xC0u | (static_cast<unsigned char>(latin9_char) >> 6u));
        result += static_cast<char>(0x80u | (static_cast<unsigned char>(latin9_char) & 0x3Fu));
        return result;
    }

    return std::string(1, latin9_char);
}



} // namespace StringUtil
