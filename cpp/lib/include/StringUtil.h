/** \file    StringUtil.h
 *  \brief   Declarations for Infomine string utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Artur Kedzierski
 *  \author  Wagner Truppel
 *  \author  Walt Howard
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2002-2004 Dr. Johannes Ruscheinski.
 *  Copyright 2015 Universitätsbibliothek Tübingen
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

#ifndef STRING_UTIL_H
#define STRING_UTIL_H


#include <algorithm>
#include <set>
#include <string>
#include <list>
#include <stdexcept>
#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <inttypes.h>
#include "Compiler.h"


#ifndef BITSPERBYTE
#       define BITSPERBYTE      8
#endif


// Avoid a circular header inclusion problem!
namespace MiscUtil {


const std::set<char> &GetWhiteSpaceSet();


} // namespace MiscUtil


/** \brief  A version of strndup that uses stack memory. Stack is instantly allocated so there is no performance issue.
 *  \param  source          The source from which to dupicate.
 *  \param  max_size        The maximum amount of string we will be copying.
 *  \note   This must be a macro. If it were a function, its return would destroy the very stack frame upon which
 *          we allocated the new string copy.
 *  \note   I wrap this because allocating on the stack is not guaranteed to not overflow the stack. Two solutions are
 *          1) Compile just this function with stack guards
 *          2) Calculate stack usage at runtime and prevent overflow
 *          Neither of these have an immediate implementation solution but I'm leaving it open for the future.
 */
#define Strndupa(source, max_size) strndupa(source, max_size);
#define Strdupa(source) strdupa(source);


/** \namespace  StringUtil
 *  \brief      Various string processing functions.
 */
namespace StringUtil {


#if defined(__linux__)
const std::string IVIA_STANDARD_LOCALE("en_US.UTF-8");
#elif defined(__APPLE__)
const std::string IVIA_STANDARD_LOCALE("en_US.UTF-8");
#else
#      error Your OS is not supported!
#endif
const std::string EmptyString;
const std::string WHITE_SPACE(" \t\n\v\r\f\xA0");


/** \brief  Convert a string to lowercase (modifies its argument). */
std::string ToLower(std::string * const s);


/** \brief  Convert a string to lowercase (does not modify its agrument). */
std::string ToLower(const std::string &s);


/** \brief  Convert a C-style string to lowercase. */
inline char *strlower(char *s)
{
        char *ch = s;
        while (*ch != '\0') {
                *ch = static_cast<char>(tolower(*ch));
                ++ch;
        }

        return s;
}


/** \brief  Convert a string to uppercase (modifies its argument). */
std::string ToUpper(std::string * const s);


/** \brief  Convert a string to uppercase (does not modify its argument). */
std::string ToUpper(const std::string &s);


/** \brief   Determines if a string consists of all uppercase letters or not.
 *  \param   s  The string to analyze.
 *  \return  True if "s" is non-empty and consists only of uppercase letters, false otherwise.
 *  \warning Please note that the behaviour of this function is locale depedent.
 */
bool IsAllUppercase(const std::string &s);


/** \brief   Determines if a string consists of all lowercase letters or not.
 *  \param   s  The string to analyze.
 *  \return  True if "s" is non-empty and consists only of lowercase letters, false otherwise.
 *  \warning Please note that the behaviour of this function is locale depedent.
 */
bool IsAllLowercase(const std::string &s);


/** \brief   Determines if a string starts with an uppercase letter followed by one or more all lowercase letters or not.
 *  \param   s  The string to analyze.
 *  \return  True if "s" has a length of >= 2, starts with an uppercase letter and is followed by one or more lowercase letters, false
 *           otherwise.
 *  \warning Please note that the behaviour of this function is locale depedent.
 */
bool IsInitialCapsString(const std::string &s);


/** \brief  Returns true if "ch" is a whitespace character.
 *  \note   Whether a character is considered to be a whitespace character is determined by isspace() and comparison against '\xA0' which is typically the
 *          encoding for a hard space and strangly not included in isspace() for Latin-1 or Latin-9 or even the default C locale.
 */
inline bool IsWhitespace(const char ch)
{
        return isspace(ch) or ch == '\xA0' /* hard space */;
}


/** \brief  Returns true if every character in "s" is a whitespace character.
 *  \note   Whether a character is considered to be a whitespace character is determined by isspace() and comparison against '\xA0' which is typically the
 *          encoding for a hard space and strangly not included in isspace() for Latin-1 or Latin-9 or even the default C locale.
 */
bool IsWhitespace(const std::string &s);


/** \brief  Converts an unsigned number to a hex character.
 *  \param  nibble  An unsigned number that must be in the range [0..15].
 *  \return The hexadecimal character representation of nibble.
 *  \note   If "u" is not in the range [0..15] this function throws an exception.
 */
char ToHex(const unsigned nibble);


/** \brief   Remove all occurences of a character from the end of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string RightTrim(std::string * const s, char trim_char=' ');


/** \brief   Remove all occurences of a character from the end of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string RightTrim(const std::string &s, char trim_char=' ');


/** \brief   Remove all occurences of a set of characters from the end of a string.
 *  \param   s         The string to trim.
 *  \param   trim_set  The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string RightTrim(const std::string &trim_set, std::string * const s);


/** \brief   Remove all occurences of a set of characters from the end of a string.
 *  \param   s         The string to trim.
 *  \param   trim_set  The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string RightTrim(const std::string &s, const std::string &trim_set);


/** \brief   Remove all occurences of a character from the end of a string. */
char *strrtrim(char *s, char trim_char=' ');


/** \brief   Remove all occurences of a character from the beginning of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string LeftTrim(std::string * const s, char trim_char=' ');


/** \brief   Remove all occurences of a character from the beginning of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string LeftTrim(const std::string &s, char trim_char=' ');


/** \brief   Remove all occurences of a set of characters from the beginning of a string.
 *  \param   s          The string to trim.
 *  \param   trim_set   The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string LeftTrim(const std::string &trim_set, std::string * const s);


/** \brief   Remove all occurences of a set of characters from the beginning of a string.
 *  \param   s          The string to trim.
 *  \param   trim_set   The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string LeftTrim(const std::string &trim_set, const std::string &s);


/** \brief   Remove all occurences of a character from either end of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string Trim(std::string * const s, char trim_char=' ');


/** \brief   Remove all occurences of a character from either end of a string.
 *  \param   s          The string to trim.
 *  \param   trim_char  The character to remove.
 *  \return  The trimmed string.
 */
std::string Trim(const std::string &s, char trim_char=' ');


/** \brief   Remove all occurences of a set of characters from either end of a string.
 *  \param   s         The string to trim.
 *  \param   trim_set  The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string Trim(const std::string &trim_set, std::string * const s);


/** \brief   Remove all occurences of a set of characters from either end of a string.
 *  \param   s          The string to trim.
 *  \param   trim_set  The set of characters to remove.
 *  \return  The trimmed string.
 */
std::string Trim(const std::string &trim_set, const std::string &s);


/** \brief   Remove all occurences of whitespace characters from either end of a string.
 *  \param   s         The string to trim.
 *  \return  The trimmed string.
 */
inline std::string TrimWhite(std::string * const s)
{
        return Trim(WHITE_SPACE, s);
}


/** \brief   Remove all occurences of whitespace characters from either end of a string.
 *  \param   s         The string to trim.
 *  \return  The trimmed string.
 */
inline std::string TrimWhite(const std::string &s)
{
        std::string temp_s(s);
        return TrimWhite(&temp_s);
}


/** \brief   Remove all occurences of whitespace characters from either end of a string.
 *  \param   s         The string to trim.
 *  \return  The trimmed string.
 */
inline std::string TrimWhite(const char * const s)
{
        return TrimWhite(std::string(s));
}


/** \brief  Convert a (signed) long long to a string.
 *  \param  n              The number to convert.
 *  \param  radix          The base to use for the resulting string representation.
 *  \param  width          If this is < 0 then pad resulting string up to width on the left, if this is > 0 then pad resulting string up to width on the right.
 *                         Default is 0 (do not pad).
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that
 *                         are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
std::string ToString(long long n, const unsigned radix = 10, const int width = 0, const char grouping_char = '\0', const unsigned grouping_size = 3);


/** \brief  Convert a (signed) long to a string */
std::string ToString(long n, const unsigned radix = 10, const int width = 0);


/** \brief  Convert an int to a string */
inline std::string ToString(int n, const unsigned radix = 10, const int width = 0)
{
        return ToString(static_cast<long>(n), radix, width);
}


/** \brief  Convert a short int to a string */
inline std::string ToString(short n, const unsigned radix = 10, const int width = 0)
{
        return ToString(static_cast<long>(n), radix, width);
}


/** \brief  Convert an unsigned long long to a string.
 *  \param  n              Number to be converted.
 *  \param  radix          Base for conversion.  (Typically 10, 16 or 8.)
 *  \param  width          Min. width of converted string.
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
std::string ToString(unsigned long long n, const unsigned radix = 10, const int width = 0,
                     const char grouping_char = '\0', const unsigned grouping_size = 3);


/** \brief  Convert a pointer to a string.
 *  \param  n              The pointer to convert.
 *  \param  radix          The base to use for the resulting string representation.
 *  \param  width          If this is < 0 then pad resulting string up to width on the left, if this is > 0 then pad resulting string up to width on the
 *                         right.  Default is 0 (do not pad).
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
template<typename Type> inline std::string ToString(const Type * const ptr, const unsigned radix = 16, const int width = 0,
                                                    const char grouping_char = '\0', const unsigned grouping_size = 2)
{
        return ToString((unsigned long long)(ptr), radix, width, grouping_char, grouping_size);
}


/** \brief  Convert an unsigned long to a string.
 *  \param  n      The unsigned long to convert to a string.
 *  \param  radix  The number base of the number to be converted.
 *  \param  width  Pad up to this width with spaces.  If width is positive the number will be right-justified, else it
 *                 will be left-justified.
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that
 *                         are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
inline std::string ToString(unsigned long n, const unsigned radix = 10, const int width = 0,
                            const char grouping_char = '\0', const unsigned grouping_size = 3)
        { return ToString(static_cast<unsigned long long>(n), radix, width, grouping_char, grouping_size); }


/** \brief  Convert an unsigned int to a string.
 *  \param  n      The unsigned long to convert to a string.
 *  \param  radix  The number base of the number to be converted.
 *  \param  width  Pad up to this width with spaces.  If width is positive the number will be right-justified, else it
 *                 will be left-justified.
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that
 *                         are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
inline std::string ToString(unsigned n, const unsigned radix = 10, const int width = 0,
                            const char grouping_char = '\0', const unsigned grouping_size = 3)
        { return ToString(static_cast<unsigned long long>(n), radix, width, grouping_char, grouping_size); }


/** \brief  Convert an unsigned short int to a string.
 *  \param  n              The unsigned long to convert to a string.
 *  \param  radix          The number base of the number to be converted.
 *  \param  width          Pad up to this width with spaces.  If width is positive the number will be right-justified,
 *                         else it will be left-justified.
 *  \param  grouping_char  If non-NUL, this character will be inserted after each group of "grouping_size" characters
 *                         starting at the end of the string.  Should this rule result in a leading character position
 *                         it will be suppressed at that position.
 *  \param  grouping_size  Used if grouping_char is non-NUL.  Indicates the size of a group of output characters that
 *                         are separated by "grouping_char's."
 *  \return A string representation for "n."
 */
inline std::string ToString(unsigned short n, const unsigned radix = 10, const int width = 0,
                            const char grouping_char = '\0', const unsigned grouping_size = 3)
{
        return ToString(static_cast<unsigned long long>(n), radix, width, grouping_char, grouping_size);
}


/** \brief  Convert a double to a string.
 * \param   n          The number to convert.
 * \param   precision  The number of decimial places to show.
 * \return  A string representing the number.
 */
std::string ToString(const double n, const unsigned precision = 5);


/** Converts "s" (a memory block) to a string consisting of hexadecimal numbers (one per nibble). */
std::string ToHexString(const std::string &s);


std::string ToHexString(uint32_t u32);


/** Converts "data" to a string consisting of hexadecimal numbers (one per nibble). */
inline std::string ToHexString(const void * const data, size_t data_size)
{ return ToHexString(std::string(reinterpret_cast<const char * const>(data), data_size)); }


/** Returns a binary nibble corresponding to "ch". */
unsigned char FromHex(const char ch);


/** \brief  Converts "hex_string" from a string consisting of hexadecimal numbers (one per nibble).
 *  \param  hex_string  The string that will be converted.
 *  \return The converted string.
 *  \note   "hex_string" must contain an even number of characters!
 */
std::string FromHexString(const std::string &hex_string);


/** \brief   Convert a string into a number.
 *  \param   s     The string to convert.
 *  \param   n     A pointer to the number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToNumber(const std::string &s, int * const n, const unsigned base = 10);


/** \brief   Convert a string into a number.
 *  \param   s     The string to convert.
 *  \param   n     A pointer to the number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToNumber(const std::string &s, long * const n, const unsigned base = 10);


/** \brief   Convert a string into a number.
 *  \param   s     The string to convert.
 *  \param   n     A pointer to the unsigned number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToNumber(const std::string &s, unsigned * const n, const unsigned base = 10);


/** \brief   Convert a string into a number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
long ToNumber(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into a short unsigned number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
unsigned short ToUnsgnedShort(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into a short unsigned number.
 *  \param   s     The string to convert.
 *  \param   n     Number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToUnsignedShort(const std::string &s, unsigned short * const n, const unsigned base = 10);


/** \brief   Convert a string into an unsigned number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
unsigned ToUnsigned(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into an unsigned number.
 *  \param   s     The string to convert.
 *  \param   n     Number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToUnsigned(const std::string &s, unsigned * const n, const unsigned base = 10);


/** \brief   Convert a string into a long unsigned number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
unsigned long ToUnsignedLong(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into a long unsigned number.
 *  \param   s     The string to convert.
 *  \param   n     Number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToUnsignedLong(const std::string &s, unsigned long * const n, const unsigned base = 10);


/** \brief   Convert a string into a long long unsigned number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
unsigned long long ToUnsignedLongLong(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into a long long unsigned number.
 *  \param   s     The string to convert.
 *  \param   n     Number that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToUnsignedLongLong(const std::string &s, unsigned long long * const n, const unsigned base = 10);


/** \brief   Convert a string into a uint64_t number.
 *  \param   s     The string to convert.
 *  \param   n     Pointer to a uint64_t that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToUInt64T(const std::string &s, uint64_t * const n, const unsigned base = 0);


/** \brief   Convert a string into a uint64_t number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
uint64_t ToUInt64T(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string into an int64_t number.
 *  \param   s     The string to convert.
 *  \param   n     Pointer to an int64_t that will hold the result.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  true if the conversion was successful, false otherwise.
 */
bool ToInt64T(const std::string &s, int64_t * const n, const unsigned base = 0);


/** \brief   Convert a string into a uint64_t number.
 *  \param   s     The string to convert.
 *  \param   base  The base of the string representation.  A value of "0" has the special meaning that if the string
 *                 starts with a "0" followed by one or more digits the base is assumed to be 8, if the string starts
 *                 with "0x" the base is assumed to be 16, in all other cases the base is assumed to be 10.
 *  \return  The converted number.
 *
 *  If the input is not comprised solely of digits (except for base "0" and a leading "0x"), then this function will
 *  generate an error.  (Unlike the other version, which will simply return false.)
 */
int64_t ToInt64T(const std::string &s, const unsigned base = 10);


/** \brief   Convert a string to a double-precision number.
 *  \param   s     The string to convert.
 *
 *  \param   n     Number that will hold the result.
 *
 *  \return  The converted number.
 *
 *  \note If the input is not well-formed, then this function will generate an error.
 */
bool ToDouble(const std::string &s, double * const n);


/** \brief   Convert a string to a double-precision number.
 *  \param   s     The string to convert.
 *  \return  The converted number.
 *
 *  If the input is not well-formed, then this function will generate an error.  (Unlike the other version, which will
 *  simply return false.)
 */
double ToDouble(const std::string &s);


/** \brief   Convert a string to a float-precision number.
 *  \param   s     The string to convert.
 *  \param   n     Number that will hold the result.
 *  \return  True if the conversion was successfyl, otherwise false.
 */
bool ToFloat(const std::string &s, float * const n);


/** \brief   Convert a string to a float-precision number.
 *  \param   s     The string to convert.
 *  \return  The converted number.
 *
 *  If the input is not well-formed, then this function will generate an error.  (Unlike the other version, which will
 *  simply return false.)
 */
float ToFloat(const std::string &s);


/** \brief  Converts a string to a boolean value.
 *  \param  value  Must be one of "true", "false", "yes", "no", "on" or "off".
 *  \param  b      Where the value of "value" is being returned.
 *  \return True if "value" equals one of the recognized strings, otherwise false.
 *  \note   The capitalisation of the recognised strings does not matter.
 */
bool ToBool(const std::string &value, bool * const b);


/** \brief  Converts a string to a boolean value.
 *  \param  value  Must be one of "true", "false", "yes", "no", "on" or "off".
 *  \return The converted value.
 *  \note   The capitalisation of the recognised strings does not matter.  If the conversion fails this function throws
 *          an exception.
 */
bool ToBool(const std::string &value);


/** \brief   Escape a specified character.
 *  \param   escape_char      The character to use as an escape character e.g. backslash.
 *  \param   char_to_escape   The character to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The escape character itself will always be escaped!
 */
std::string &Escape(const char escape_char, const char char_to_escape, std::string * const s);


/** \brief    Escape a specified character.
 *  \param    escape_char     The character to use as an escape character e.g. backslash.
 *  \param    char_to_escape  The set of characters to escape.
 *  \param    s               The string to be processed.
 *  \return   A copy of the modified "s" after the call.
 *  \warning  The escape character itself will always be escaped!
 */
inline std::string Escape(const char escape_char, const char char_to_escape, const std::string &s)
{
        std::string modified_s(s);
        return Escape(escape_char, char_to_escape, &modified_s);
}


/** \brief   Escape a specified set of characters.
 *  \param   escape_char      The character to use as an escape character e.g. backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The escape character itself will always be escaped!
 */
std::string &Escape(const char escape_char, const char * const chars_to_escape, std::string * const s);


/** \brief   Escape a specified set of characters.
 *  \param   escape_char      The character to use as an escape character e.g. backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The escape character itself will always be escaped!
 */
inline std::string &Escape(const char escape_char, const std::string &chars_to_escape, std::string * const s)
        { return Escape(escape_char, chars_to_escape.c_str(), s); }


/** \brief    Escape a specified set of characters.
 *  \param    escape_char      The character to use as an escape character e.g. backslash.
 *  \param    chars_to_escape  The set of characters to escape.
 *  \param    s                The string to be processed.
 *  \return   A copy of the modified "s" after the call.
 *  \warning  The escape character itself will always be escaped!
 */
inline std::string Escape(const char escape_char, const char * const chars_to_escape, const std::string &s)
{
        std::string modified_s(s);
        return Escape(escape_char, chars_to_escape, &modified_s);
}


/** \brief    Escape a specified set of characters.
 *  \param    escape_char      The character to use as an escape character e.g. backslash.
 *  \param    chars_to_escape  The set of characters to escape.
 *  \param    s                The string to be processed.
 *  \return   A copy of the modified "s" after the call.
 *  \warning  The escape character itself will always be escaped!
 */
inline std::string Escape(const char escape_char, const std::string &chars_to_escape, const std::string &s)
{
        std::string modified_s(s);
        return Escape(escape_char, chars_to_escape, &modified_s);
}


/** \brief   Escape a specified character with a backslash.
 *  \param   char_to_escape   The character to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped.
 */
inline std::string &BackslashEscape(const char char_to_escape, std::string * const s)
        { return Escape('\\', char_to_escape, s); }


/** \brief   Escape a specified character with a backslash.
 *  \param   char_to_escape   The character to escape.
 *  \param   s                The string to be processed.
 *  \return  A copy of the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped!
 */
inline std::string BackslashEscape(const char char_to_escape, const std::string &s)
{
        std::string modified_s(s);
        return Escape('\\', char_to_escape, &modified_s);
}


/** \brief   Escape a specified set of characters with a backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped!
 */
inline std::string &BackslashEscape(const char * const chars_to_escape, std::string * const s)
        { return Escape('\\', chars_to_escape, s); }


/** \brief   Escape a specified set of characters with a backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed (will be modified in place).
 *  \return  A reference to the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped!
 */
inline std::string &BackslashEscape(const std::string &chars_to_escape, std::string * const s)
        { return Escape('\\', chars_to_escape, s); }


/** \brief   Escape a specified set of characters with a backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed.
 *  \return  A reference to the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped!
 */
inline std::string BackslashEscape(const char * const chars_to_escape, const std::string &s)
         { return Escape('\\', chars_to_escape, s); }


/** \brief   Escape a specified set of characters with a backslash.
 *  \param   chars_to_escape  The set of characters to escape.
 *  \param   s                The string to be processed.
 *  \return  A reference to the modified "s" after the call.
 *  \note    The backslash character itself will always be escaped!
 */
inline std::string BackslashEscape(const std::string &chars_to_escape, const std::string &s)
        { return Escape('\\', chars_to_escape, s); }


/** \brief  Remove and return the first part of a string. "delimiter_string" tells where the split is to occur. "target"
            string is altered with the "Head" removed. The Head is returned. The delimiter_string will also be removed.
 *  \param  target            The string to operate upon. It will be altered if the delimiter_string exists.
 *  \param  delimiter_string  The string to split around. Will not be kept in either the returned "Head", or the "Tail".
 *  \param  start             The starting offset within the target. Defaults to 0.
 *
 *  \return The Head of the string. The text that came before the delimiter_string. Empty string if delimiter does not
            exist in the target.
    \note   Works in situations similar to where strtok() used to be used.
 */
std::string ExtractHead(std::string * const target, const std::string &delimiter_string = " ", std::string::size_type start = 0);


/** \brief  Split a string around a delimiter string.
 *  \param  source                     The string to split.
 *  \param  delimiter_string           The string to split around.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *
 *  Splits "source" around the characters in "field_separators" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 *
 *  \return The number of extracted "fields".
 */
template<typename InsertableContainer> unsigned Split(const std::string &source, const std::string &delimiter_string,
                                                      InsertableContainer * const container,
                                                      const bool suppress_empty_components = true)
{
        if (unlikely(delimiter_string.empty()))
                throw std::runtime_error("in StringUtil::Split: empty delimited string!");

        container->clear();
        if (source.empty())
              return 0;

        std::string::size_type start = 0;
        std::string::size_type next_delimiter = 0;
        unsigned count = 0;

        while (next_delimiter != std::string::npos) {
                // Search for first occurence of delimiter that appears after start
                next_delimiter = source.find(delimiter_string, start);

                // Add the field starting at start and ending at next_delimiter
                if (next_delimiter == std::string::npos) {
                        if (not suppress_empty_components or start < source.length())
                                container->insert(container->end(), source.substr(start));
                        ++count;
                }
                else if (next_delimiter > start) {
                        if (not suppress_empty_components or start < next_delimiter)
                                container->insert(container->end(), source.substr(start, next_delimiter - start));
                        ++count;
                }

                // Move the start pointer along the array
                if (next_delimiter != std::string::npos)
                        start = next_delimiter + delimiter_string.length();
                if (start >= source.length())
                        next_delimiter = std::string::npos;
        }

        return count;
}


/** \brief  Splits a string into two component strings.
 *  \param  s                  The string to split.
 *  \param  separator          The string separating the substrings.
 *  \param  part1              The string leading up to the "separator" string.
 *  \param  part2              The string following the separator string.
 *  \param  allow_empty_parts  If true, "part1" and/or "part2" may be empty, otherwise not.
 *  \return True if we found the separator and successfully extracted the two parts, otherwise false.
 */
bool SplitOnString(const std::string &s, const std::string &separator, std::string * const part1, std::string * const part2,
                   const bool allow_empty_parts = false);


/** \brief  Split a string around a delimiter.
 *  \param  source                     The string to split.
 *  \param  delimiter                  The character to split around.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *  \return The number of extracted "fields".
 *
 *  Splits "source" around the character in "delimiter" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 */
template<typename InsertableContainer> unsigned Split(const std::string &source, const char delimiter,
                                                      InsertableContainer * const container,
                                                      const bool suppress_empty_components = true)
{
        container->clear();
        if (source.empty())
              return 0;

        std::string::size_type start = 0;
        std::string::size_type next_delimiter = 0;
        unsigned count = 0;

        while (next_delimiter != std::string::npos) {
                // Search for first occurence of delimiter that appears after start:
                next_delimiter = source.find(delimiter, start);

                // Add the field starting at start and ending at next_delimiter:
                if (next_delimiter == std::string::npos) {
                        if (not suppress_empty_components or start < source.length())
                                container->insert(container->end(), source.substr(start));
                        ++count;
                }
                else if (next_delimiter > start) {
                        if (not suppress_empty_components or start < next_delimiter)
                                container->insert(container->end(), source.substr(start, next_delimiter - start));
                        ++count;
                }

                // Move the start pointer along the string:
                if (next_delimiter != std::string::npos)
                        start = next_delimiter + 1;
                if (start >= source.length())
                        next_delimiter = std::string::npos;
        }

        return count;
}


/** \brief  Split a string around any delimiter as specified by a set.
 *  \param  source                     The string to split.
 *  \param  delimiters                 The characters to split around.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *  \return The number of extracted "fields".
 *
 *  Splits "source" around the character in "delimiter" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 */
template<typename InsertableContainer> unsigned Split(const std::string &source, const std::set<char> &delimiters,
                                                      InsertableContainer * const container, const bool suppress_empty_components = true)
{
        container->clear();
        if (source.empty())
              return 0;

        std::string::size_type start = 0;
        std::string::size_type next_delimiter = 0;
        unsigned count = 0;

        while (next_delimiter != std::string::npos) {
                // Search for first occurence a delimiter that appears after start:
                next_delimiter = std::string::npos;
                for (std::set<char>::const_iterator delimiter(delimiters.begin()); delimiter != delimiters.end(); ++delimiter) {
                        const std::string::size_type next_delimiter_candidate(source.find(*delimiter, start));
                        if (next_delimiter_candidate != std::string::npos and next_delimiter_candidate < next_delimiter)
                                next_delimiter = next_delimiter_candidate;
                }

                // Add the field starting at start and ending at next_delimiter:
                if (next_delimiter == std::string::npos) {
                        if (not suppress_empty_components or start < source.length())
                                container->insert(container->end(), source.substr(start));
                        ++count;
                }
                else if (next_delimiter > start) {
                        if (not suppress_empty_components or start < next_delimiter)
                                container->insert(container->end(), source.substr(start, next_delimiter - start));
                        ++count;
                }

                // Move the start pointer along the string:
                if (next_delimiter != std::string::npos)
                        start = next_delimiter + 1;
                if (start >= source.length())
                        next_delimiter = std::string::npos;
        }

        return count;
}


/** \brief  Split a string around any whitespace characters.
 *  \param  source                     The string to split.
 *  \param  container                  A list to return the resulting fields in.
 *  \param  suppress_empty_components  If true we will not return empty fields.
 *  \return The number of extracted "fields".
 *
 *  Splits "source" around the character in "delimiter" and return the resulting list of fields in "fields."
 *  Empty fields are returned in the list.
 */
template<typename InsertableContainer> inline unsigned WhiteSpaceSplit(const std::string &source, InsertableContainer * const container,
                                                                       const bool suppress_empty_components = true)
{
        return Split(source, MiscUtil::GetWhiteSpaceSet(), container, suppress_empty_components);
}


/** \brief  Split a string, then trim the component substrings.
 *  \param  s                     The string to split.
 *  \param  field_separator       The character to split around.
 *  \param  trim_chars            A set of characters to trim from each resulting substring.
 *  \param  container             A string container to hold the parts (e.g. std::list<std::string>).
 *  \param  suppress_empty_words  If true, we skip empty "words", otherwise we keep them.
 *  \return The number of extracted "words".
 *
 *  Splits "s" using any of the character in "field_separator" and then trims the resulting words.
 *
 *  \note
 *  The container must contain std::string objects.  It can have any container type that has the STL insert(interator, object) interface.  In other words, it can
 *  be std::list<std::string>, std::vector<std::string>, std::deque<std::string>, or if you require sorting std::set<std::string>. For non ordered containers
 *  the individual parts will be inserted at the end of the container.
 */
template<typename InsertableContainer> inline unsigned SplitThenTrim(const std::string &s, const std::string &field_separators,
                                                                     const std::string &trim_chars, InsertableContainer * const container,
                                                                     const bool suppress_empty_words = true)
{
        if (unlikely(field_separators.empty()))
                throw std::runtime_error("in StringUtil::SplitThenTrim: empty field separators string!");

        container->clear();
        unsigned count(0);
        std::string::const_iterator ch(s.begin());
        while (ch != s.end()) {
                // We have the start of a new word:
                std::string::const_iterator word_start(ch);

                // Look for valid chars that may make up a word:
                while (ch != s.end() and std::strchr(field_separators.c_str(), *ch) == nullptr)
                        ++ch;

                // Extract the word:
                std::string new_word(s.substr(word_start - s.begin(), ch - word_start));
                StringUtil::Trim(trim_chars, &new_word);

                if (not new_word.empty() or not suppress_empty_words) {
                        container->insert(container->end(), new_word);
                        ++count;
                }

                // Skip over separators:
                if (ch != s.end() and std::strchr(field_separators.c_str(), *ch) != nullptr)
                        ++ch;
        }

        return count;
}


/** \brief  Split a string, then trim the component substrings.
 *  \param  s                     The string to split.
 *  \param  field_separator       A delimiter character to split around.
 *  \param  trim_chars            A set of characters to trim from each resulting substring.
 *  \param  container                 A string container to hold the parts (e.g. std::list<std::string>).
 *  \param  suppress_empty_words  If true, we skip empty "words", otherwise we keep them.
 *
 *  Splits "s" using the character in "field_separator" and then trims the resulting words using characters in
 *  "trim_chars".  Returns the resulting list of words in "container."
 *
 *  \par
 *  The "words" must be a container that contains strings.  It can have any container type that matches the STL "Back
 *  Insertion Sequence".  In other words, it can be std::list<std::string>, std::vector<std::string> or
 *  std::deque<std::string>.
 *
 *  \return  The number of extracted "words".
 */
template<typename InsertableContainer> inline unsigned SplitThenTrim(const std::string &s, const char field_separator, const std::string &trim_chars,
                                                                     InsertableContainer * const container, const bool suppress_empty_words = true)
{
        container->clear();
        unsigned count = 0;
        std::string::const_iterator ch(s.begin());
        while (ch != s.end()) {
                // Skip over separators:
                while (ch != s.end() and *ch == field_separator)
                        ++ch;

                // Bail out if we're at the end:
                if (ch == s.end())
                        return count;

                // We have the start of a new word:
                std::string::const_iterator word_start(ch);

                // Look for valid chars that may make up a word:
                while (ch != s.end() and *ch != field_separator)
                        ++ch;

                // Extract the word:
                std::string new_word = s.substr(word_start - s.begin(), ch - word_start);
                StringUtil::Trim(trim_chars, &new_word);

                // Skip empty words?
                if (new_word.empty() and suppress_empty_words)
                        continue;

                container->insert(container->end(), new_word);
                ++count;
        }

        return count;
}


/** \brief  Split a string, then trim the component substrings' whitespace.
 *  \param  s                     The string to split.
 *  \param  field_separators      A set of delimiter characters to split around.
 *  \param  container             A string container to hold the parts (e.g. std::list<std::string>).
 *  \param  suppress_empty_words  If true, we skip empty "words", otherwise we keep them.
 *  \return The number of extracted "words".
 */
template<typename InsertableContainer> inline unsigned SplitThenTrimWhite(const std::string &s, const std::string &field_separators,
                                                                          InsertableContainer * const container, const bool suppress_empty_words = true)
{
        return SplitThenTrim(s, field_separators, WHITE_SPACE, container, suppress_empty_words);
}


/** \brief  Split a string, then trim the component substrings' whitespace.
 *  \param  s                     The string to split.
 *  \param  field_separators      A set of delimiter characters to split around.
 *  \param  container             A string container to hold the parts (e.g. std::list<std::string>).
 *  \param  suppress_empty_words  If true, we skip empty "words", otherwise we keep them.
 *  \return The number of extracted "words".
 */
template<typename InsertableContainer> inline unsigned SplitThenTrimWhite(const std::string &s, const char field_separator,
                                                                          InsertableContainer * const container, const bool suppress_empty_words = true)
{
        return SplitThenTrim(s, field_separator, WHITE_SPACE, container, suppress_empty_words);
}


/** \brief  Split a string, then trim the component substrings.
 *  \param  s                     The string to split.
 *  \param  separator             The string separating the substrings.
 *  \param  trim_chars            A set of characters to trim from each resulting substring.
 *  \param  container             A string container to hold the parts (e.g. std::list<std::string>).
 *  \param  suppress_empty_words  If true, we skip empty "words", otherwise we keep them.
 *
 *  Splits "s" using "separator" and then trims the resulting substrings using characters in "trim_chars".  Returns the
 *  resulting list of substrings in "substrings."
 *
 *  \par
 *  "substrings" must be a container that contains strings.  It can have any container type that matches the STL "Back
 *  Insertion Sequence".  In other substrings, it can be std::list<std::string>, std::vector<std::string> or
 *  std::deque<std::string>.
 *
 *  \return  The number of extracted "substrings" (empty substrings are not returned).
 */
template<typename InsertableContainer> unsigned SplitOnStringThenTrim(const std::string &s, const std::string &separator, const std::string &trim_chars,
                                                                      InsertableContainer * const container, const bool suppress_empty_words = true)
{
        container->clear();
        const std::string::size_type SEPARATOR_LENGTH(separator.length());
        unsigned count(0);

        // Split the string around the seperator, and trim each component:
        std::string::size_type old_start_pos(0), next_separator_start_pos;
        while ((next_separator_start_pos = s.find(separator, old_start_pos)) != std::string::npos) {

                // Get the next element and trim it:
                std::string new_substr(s.substr(old_start_pos, next_separator_start_pos - old_start_pos));
                StringUtil::Trim(trim_chars, &new_substr);

                if (not (new_substr.empty() and suppress_empty_words)) {
                        // The element is non-empty, keep it:
                        container->insert(container->end(), new_substr);
                        ++count;
                }

                // Go on to the next element:
                old_start_pos = next_separator_start_pos + SEPARATOR_LENGTH;
        }

        // Grab the last (only) component:
        if (old_start_pos < s.length()) {
                std::string new_substr(s.substr(old_start_pos));
                StringUtil::Trim(trim_chars, &new_substr);

                // Skip empty word?
                if (not (new_substr.empty() and suppress_empty_words)) {
                        container->insert(container->end(), new_substr);
                        ++count;
                }
        }

        return count;
}


/** \brief  Split a string, then trim the component substrings.
 *  \param  s                  The string to split.
 *  \param  separator          The string separating the substrings.
 *  \param  trim_chars         A set of characters to trim from each resulting substring.
 *  \param  part1              The string leading up to the "separator" string.
 *  \param  part2              The string following the separator string.
 *  \param  allow_empty_parts  If true, "part1" and/or "part2" may be empty, otherwise not.
 *  \return True if we found the separator and successfully extracted the two parts, otherwise false.
 */
bool SplitOnStringThenTrim(const std::string &s, const std::string &separator, const std::string &trim_chars, std::string * const part1,
                           std::string * const part2, const bool allow_empty_parts = false);


/** \brief  Split a string, then trim whitespace from the component substrings.
 *  \param  s                  The string to split.
 *  \param  separator          The string separating the substrings.
 *  \param  part1              The string leading up to the "separator" string.
 *  \param  part2              The string following the separator string.
 *  \param  allow_empty_parts  If true, "part1" and/or "part2" may be empty, otherwise not.
 *  \return True if we found the separator and successfully extracted the two parts, otherwise false.
 */
inline bool SplitOnStringThenTrimWhite(const std::string &s, const std::string &separator, std::string * const part1, std::string * const part2,
                                       const bool allow_empty_parts = false)
{
        return SplitOnStringThenTrim(s, separator, WHITE_SPACE, part1, part2, allow_empty_parts);
}


/** \brief This function unescapes, splits, then trims each field.
 *  \param  source       The source string.
 *  \param  separator    The character field separator.
 *  \param  escape_char  The escape character.
 *  \param  trim_chars   The characters to trim from each field.
 *  \param  container       A template container that supports the push_back member function.
 *  \return The number of fields extracted from source.
 */
template<typename InsertableContainer> unsigned UnescapeAndSplitThenTrim(const std::string &source, const char separator, const char escape_char,
                                                                         const std::string &trim_chars, InsertableContainer * const container)
{
        container->clear();
        if (source.empty())
                return 0;
        unsigned count(0);
        bool escaped(false);
        std::string current_field;
        for (std::string::const_iterator ch(source.begin()); ch != source.end(); ++ch) {
                if (escaped) {
                        escaped = false;
                        current_field += *ch;
                }
                else if (*ch == escape_char)
                        escaped = true;
                else if (*ch == separator) {
                        StringUtil::Trim(trim_chars, &current_field);
                        container->insert(container->end(), current_field);
                        current_field.clear();
                        ++count;
                }
                else
                        current_field += *ch;
        }
        StringUtil::Trim(trim_chars, &current_field);
        container->insert(container->end(), current_field);
        ++count;

        return count;
}


/** \brief  Join a "list" of words to form a single string.
 *  \param  source     The container of strings that are to be joined.
 *  \param  separator  The text to insert between the "source" elements.
 *  \param  dest       A string in which to return the combined result.
 *  \return The number of strings that were joined.
 *
 *  Joins a list of strings from "source" into a single long string, "dest".  The source can be pretty much any container
 *  of strings, and will typically be std::list<std::string> or std::vector<std::string>.  The string "separator" will be
 *  inserted between the elements.
 */
template<typename StringContainer> unsigned Join(const StringContainer &source, const std::string &separator, std::string * const dest)
{
        dest->clear();
        unsigned word_count(0);
        const unsigned SOURCE_SIZE(static_cast<unsigned>(source.size()));
        for (typename StringContainer::const_iterator i(source.begin()); i !=  source.end(); ++i) {
                *dest += *i;
                ++word_count;
                if (word_count < SOURCE_SIZE)
                        *dest += separator;
        }

        return SOURCE_SIZE;
}


/** \brief  Join words iterated by first to last to form a single string.
 *  \param  first      Iterator to the starting word of those to be combined.
 *  \param  last       Iterator one past the last word to be combined.
 *  \param  separator  Stick this between each word
 *  \return The resulting string.
 */
template <typename ForwardIterator> std::string Join(ForwardIterator first, ForwardIterator last, const std::string &separator)
{
        std::string resultant_string;
        while (first != last) {
                resultant_string += *first;
                ++first;
                if (first != last)
                        resultant_string += separator;
        }
        return resultant_string;
}


/** \brief  Join a "list" of words to form a single string.
 *  \param  source     The container of strings that are to be joined.
 *  \param  separator  The character to insert between the "source" elements.
 *  \param  dest       A string in which to return the combined result.
 *  \return The number of strings that were joined.
 *
 *  Joins a list of strings from "source" into a single long string, "dest".  The source can be pretty much any container
 *  of strings, and will typically be std::list<std::string> or std::vector<std::string>.  The "separator" character will
 *  be inserted between the elements.
 */
template<typename StringContainer> inline unsigned Join(const StringContainer &source, const char separator, std::string * const dest)
{
        std::string string_separator(1, separator);
        return Join(source, string_separator, dest);
}


/** \brief  Join a list of words to form a single string.
 *  \param  source_begin   The iterator that points to the beginning of the list.
 *  \param  source_end     The iterator that points to the end of the list.
 *  \param  separator      The text to insert between the list elements.
 *  \param  dest           A string in which to return the combined result.
 *
 *  Joins a list of strings from "source" into a single long string,
 *  "dest".  The source can be pretty much any container of strings,
 *  and will usually be std::list<std::string> or
 *  std::vector<std::string>.  The string "separator" is inserted
 *  between the elements.
 */
template<typename StringContainer> void Join(const StringContainer &source_begin, const StringContainer &source_end, const std::string &separator,
                                             std::string * const dest)
{
        *dest = "";
        unsigned word_count = 0;

        for (StringContainer i = source_begin; i !=  source_end; ++i) {
                *dest += *i;
                ++word_count;
                if (i != source_end)
                        *dest += separator;
        }
}


/** \brief  Join a list of words to form a single string and return that string
 *  \param  source         A container of strings.
 *  \param  separator      The text to insert between the list elements.
 *
 *  Joins a list of strings from "source" into a single long string and returns it.
 */
template<typename StringContainer> inline std::string Join(const StringContainer &source, const char separator)
{
        std::string dest;
        Join(source, separator, &dest);

        return dest;
}


/** \brief  Join a list of words to form a single string and return that string
 *  \param  source         A container of strings.
 *  \param  separator      The text to insert between the list elements.
 *
 *  Joins a list of strings from "source" into a single long string and returns it.
 */
template<typename StringContainer> inline std::string Join(const StringContainer &source, const std::string &separator)
{
        std::string dest;
        Join(source, separator, &dest);

        return dest;
}


/** \brief  Join a "list" of words to form a single string.
 *  \param  source       The container of strings that are to be joined.
 *  \param  separator    The text to insert between the list elements.
 *  \param  escape_char  The character to use as an escape character e.g. backslash.
 *  \param  dest         A string in which to return the combined result.
 *  \return The number of strings that were joined.
 *  \note   The escape character itself and the separator will always be escaped!
 *
 *  Joins a list of strings from "source" into a single long string, "dest".  The source can be pretty much any container of strings, and will usually be
 *  std::list<std::string> or std::vector<std::string>.  The string "separator" is inserted between the elements.
 */
template<typename StringContainer> unsigned EscapeAndJoin(const StringContainer &source, const char separator, const char escape_char,
                                                          std::string * const dest)
{
        std::string chars_to_escape;
        chars_to_escape += separator;

        dest->clear();
        unsigned word_count = 0;
        const unsigned SOURCE_SIZE(source.size());
        for (typename StringContainer::const_iterator i = source.begin(); i !=  source.end(); ++i) {
                *dest += Escape(escape_char, chars_to_escape, *i);
                ++word_count;
                if (word_count < SOURCE_SIZE)
                        *dest += separator;
        }

        return SOURCE_SIZE;
}


/** \brief  Join a "list" of words to form a single string.
 *  \param  source       The container of strings that are to be joined.
 *  \param  separator    The text to insert between the list elements.
 *  \param  escape_char  The character to use as an escape character e.g. backslash.
 *  \return The concatenated string.
 *  \note   The escape character itself and the separator will always be escaped!
 *
 *  Joins a list of strings from "source" into a single long string that it returns.  The source can be pretty much any
 *  container of strings, and will usually be std::list<std::string> or std::vector<std::string>.  The string "separator"
 *  is inserted between the elements.
 */
template<typename StringContainer> std::string EscapeAndJoin(const StringContainer &source, const char separator, const char escape_char)
{
        std::string dest;
        EscapeAndJoin(source, separator, escape_char, &dest);

        return dest;
}


template<typename StringContainer> unsigned UnescapeAndSplit(const std::string &mushed_data, const char separator,
                                                             StringContainer * const container,
                                                             const bool suppress_empty_elements = true,
                                                             const char escape_char = '\\')
{
        unsigned element_count(0);

        std::string element;
        bool escaped(false);
        for (std::string::const_iterator ch(mushed_data.begin()); ch != mushed_data.end(); ++ch) {
                if (escaped) {
                        element += *ch;
                        escaped = false;
                }
                else if (*ch == escape_char)
                        escaped = true;
                else if (*ch == separator) {
                        if (element.empty()) {
                                if (not suppress_empty_elements) {
                                        container->insert(container->end(), element);
                                        ++element_count;
                                }
                        }
                        else {
                                container->insert(container->end(), element);
                                element.clear();
                                ++element_count;
                        }
                }
                else
                        element += *ch;
        }

        if (not element.empty()) {
                container->insert(container->end(), element);
                ++element_count;
        }
        else if (not mushed_data.empty() and not suppress_empty_elements) {
                container->insert(container->end(), element);
                ++element_count;
        }

        return element_count;
}


/** \brief Remove duplicate string values from a list of strings.
 *  \param  values  The list from which duplicates are to be removed.
 *
 *  The first of each value will be left in the list, and any
 *  subsequent occurences removed.  The items that are not removed
 *  will not be reordered.
 */
void RemoveDuplicatesFromList(std::list<std::string> * const values);


/** \brief  Split any value in a list that includes the delimiter into multiple values.
 *  \param  values  The list whose values are to be split.
 *  \param  delimiter  The delimiter around which the values should be split.
 */
void SplitListValues(std::list<std::string> * const values, const std::string &delimiter = ";");


/** \brief   Comparison helper utility that skips over leading articles of various common languages.
 *  \param   text  The sentence fragment that may optionally start with an article.
 *  \return  The start of the text after any leading optional article and whitespace.
 * It will delete \\r\\n or \\n or \\n\\r
 */
const char *SkipLeadingArticle(const char *text);


/** Returns a pointer to the first alphanumeric character in "text" or a pointer to the terminating zero byte. */
const char *SkipNonAlphanumericChars(const char *text);


/** \fn     AlphaWordCompare(const char *lhs, const char *rhs)
 *  \brief  Compares "lhs" and "rhs" in a case-independent manner, skipping over leading spaces and/or articles.
 *  \param  lhs 1st string to compare.
 *  \param  rhs 2nd string to compare.
 *  \return An int analogous to strcmp(3).
 */
inline int AlphaWordCompare(const char *lhs, const char *rhs) {
    return ::strcasecmp(SkipNonAlphanumericChars(SkipLeadingArticle(lhs)),
                        SkipNonAlphanumericChars(SkipLeadingArticle(rhs)));
}


/** \fn     AlphaWordCompare(const std::string &lhs, const std::string &rhs)
 *  \brief  See AlphaWordCompare(const char *lhs, const char *rhs)
 */
inline int AlphaWordCompare(const std::string &lhs, const std::string &rhs)
        { return AlphaWordCompare(SkipLeadingArticle(lhs.c_str()), SkipLeadingArticle(rhs.c_str())); }


/** \brief  Erase the newline and carriage return characters from the end of the line.
 *  \param  line  The line of text where the characters will be deleted.
 *
 * It will delete \\r\\n or \\n or \\n\\r
 */
void RemoveTrailingLineEnd(char *line);


/** \brief  Erase the newline and carriage return characters from the end of the line */
void RemoveTrailingLineEnd(std::string * const line);


/** \brief  Erases any newline (\\n) characters from the end of the string.
 *  \return The address of the modified string "s".
 */
char *RemoveTrailingLineEnds(char * const line);


/** \brief  Erases any newline (\\n) characters from the end of the string.
 *  \return A reference to the modified string "s".
 */
std::string &RemoveTrailingLineEnds(std::string * const line);


/** \brief  Strips various marks (e.g. accents) from a string.
 *  \param  ansi_string  the address of the string that is to be stripped.
 *  \return A reference to the stripped string.
 */
std::string &AnsiToAscii(std::string * const ansi_string);


/** \brief  Replaces every occurance of a char with a new char
 *  \param  s         String to work on.
 *  \param  old_char  The character to be replaced.
 *  \param  new_char  The replacement character.
 *  \return A reference to the modified string "s".
 */
std::string &Map(std::string * const s, const char old_char, const char new_char);


/** \brief  Replaces every occurance of a char with a new char
 *  \param  s         String to work on.
 *  \param  old_char  The character to be replaced.
 *  \param  new_char  The replacement character.
 *  \return A copy of the modified string.
 */
inline std::string Map(const std::string &s, const char old_char, const char new_char)
{
        std::string s1(s);
        return Map(&s1, old_char, new_char);
}


/** \brief  Replaces every character in old_set with the corresponding character in new_set.
 *  \param  s        String to work on.
 *  \param  old_set  The characters that are to be replaced.
 *  \param  new_set  The replacement characters.
 *  \return A reference to the modified string "s".
 *  \note  There has to be exactly one character each in new_set for every character in old_set.
 */
std::string &Map(std::string * const s, const std::string &old_set, const std::string &new_set);


/** \brief  Replaces every character in old_set with the corresponding character in new_set.
 *  \param  s        String to work on.
 *  \param  old_set  The characters that are to be replaced.
 *  \param  new_set  The replacement characters.
 *  \return A copy of the modified string.
 *  \note  There has to be exactly one character each in new_set for every character in old_set.
 */
inline std::string Map(const std::string &s, const std::string &old_set, const std::string &new_set)
{
        std::string s1(s);
        return Map(&s1, old_set, new_set);
}


/** \brief  Collapses multiple occurrences of ch into a single occurrence.
 *  \param  s        string to modify.
 *  \param  scan_ch  The character to collapse.
 *  \return A reference to the modified string "s".
 */
std::string &Collapse(std::string * const s, char scan_ch = ' ');


/** \brief  Collapses multiple occurrences of whitespace into a single space.
 *  \param  s The input string that will be "collapsed."
 *  \return A reference to the modified string "s".
 */
std::string &CollapseWhitespace(std::string * const s);

inline std::string CollapseWhitespace(const std::string &s)
{
        std::string temp_s(s);
        return CollapseWhitespace(&temp_s);
}


/** \brief  Collapses multiple occurrences of whitespace into a single space and removes leading and trailing whitespace.
 *  \param  s The input string that will be "collapsed."
 *  \return A reference to the modified string "s".
 */
std::string &CollapseAndTrimWhitespace(std::string * const s);

inline std::string CollapseAndTrimWhitespace(const std::string &s)
{
        std::string temp_s(s);
        return CollapseAndTrimWhitespace(&temp_s);
}


/** \brief  Implements a wildcard matching function.
 *  \param  pattern     The wildcard pattern.
 *  \param  s           The string to be scanned.
 *  \param  ignore_case Whether to perform the scanning in a case-sensitive manner or not.
 *  \return Whether we had a successful match or not.
 *
 *  This function implements a regular expression pattern match.  '?'
 *  represents exactly one arbitrary character while '*' represents an
 *  arbitrary sequence of characters including the null sequence.
 *  Backslash '\' is the `escape' character and removes the special
 *  meaning of any following character.  '[' starts a character class
 *  (set). Therefore in order to match any of {'\','*','?','['} you
 *  have to preceed them with a single '\'.  Character classes are
 *  enclosed in square brackets and negated if the first character in
 *  the set is a caret '^'.  So in order to match any single decimal
 *  digit you may specify "[0123456789]" where the order of the digits
 *  doesn't matter.  In order to match any single character with the
 *  exception of a single decimal digit specify "[^0123456789]".
 *
 *  \par Copyright notice
 * (c© 1988,1990,2001 Johannes Ruscheinski. All Rights Reserved.  I
 * grant the exception that this function may be contained and used
 * with the rest of the INFOMINE library under the the same copyright
 * rules as the INFOMINE library if it is any version of the GPL (GNU
 * General Public License).  Johannes Ruscheinski Jul-27, 2001.
 *
 * \par Examples:
 *                              "fr5\?."         matches       "fr5?."
 * \par
 *                              "fred?2"         matches       "fred.2"
 * \par
 *                              "bob*"           matches       "bob.exe"
 * \par
 *                              "bill*"          matches       "bill"
 * \par
 *                              "joe?5"          doesn't match "joe15"
 * \par
 *                              "[A-Z]"          matches any single captial letter
 * \par
 *                              "cell[ABC].txt"  matches       "cellC.txt"
 * \par
 *                              "cell[^ABC].txt" doesn't match "cellA.txt"
 * \par
 *                              "cell[^0-9].txt" doesn't match "cell6.txt"
 * \par
 *                              "*"              matches everything
 * \par
 *                              "?A" matches any single character followed by 'A'
 */
bool Match(const char *pattern, const char *s, bool ignore_case = false) throw(std::exception);


/** \brief  Implements a wildcard matching function.
 *  \param  pattern      The wildcard pattern.
 *  \param  s            The string to be scanned.
 *  \param  ignore_case  Whether to perform the scanning in a case-sensitive manner or not.
 *  \return Whether we had a successful match or not.
 */
inline bool Match(const std::string &pattern, const std::string &s, bool ignore_case = false) throw(std::exception)
{
        return Match(pattern.c_str(), s.c_str(), ignore_case);
}


/** \brief  Allocate and return a new duplicate of a char* array.
 *  \param  s  String to duplicate.
 */
inline char *strnewdup(const char * const s)
{
        if (s == nullptr)
                return nullptr;

        size_t len = std::strlen(s);
        char *new_s = new char[len+1];
        return std::strcpy(new_s, s);
}

/** \brief Creates copy of a char* on the local stack. Fast and self destructing upon scope exit.
 *  \note This is potentially unsafe due to risk of stack overflow. However, all code compiled without stack checking enabled has the same
 *  issue.  In the future I'd like to add a stack available function to check for enough room before doing this. That function would be
 *  virtually instantaneous. All it has to do is subtract pointers.
*/
#define StrdupLocal(source) std::strcpy(static_cast<char*>(::alloca(std::strlen(source) + 1)), source)


/** \brief   Replaces one or all occurrences of "old_text" in "*s" with "new_text".
 *  \param   old_text  The text that should be replaced.
 *  \param   new_text  The text that should serve as replacement.
 *  \param   s         The string to work on.
 *  \param   global    If true all occurrences of "old_text" will be replaced with "new_text" otherwise
 *                     only the first occurrence will be replaced.
 *  \return  A reference to the modified string.
 */
std::string &ReplaceString(const std::string &old_text, const std::string &new_text, std::string * const s,
                           const bool global = true);


/** \brief   Replaces one or all occurrences of "old_text" in "s" with "new_text".
 *  \param   old_text  The text that should be replaced.
 *  \param   new_text  The text that should serve as replacement.
 *  \param   s         The string to work on (will not be modified).
 *  \param   global    If true all occurrences of "old_text" will be replaced with "new_text" otherwise
 *                     only the first occurrence will be replaced.
 *  \return  The modified string.
 */
inline std::string ReplaceString(const std::string &old_text, const std::string &new_text, const std::string &s,
                                 const bool global = true)
{
        std::string temp_s(s);
        return ReplaceString(old_text, new_text, &temp_s, global);
}


/** \brief   Converts all characters in chars_to_whiten into spaces (' ')
 *  \param   chars_to_whiten  The "set" of characters that will be converted to whitespace
 *  \param   s                A pointer to the string that is to be modified.
 *  \return  A reference to the modified string.
 */
std::string &WhitenChars(const std::string &chars_to_whiten, std::string * const s);


/** \brief   Removes all characters in a supplied set from a string.
 *  \param   remove_set  The "set" of characters that will be removed from "s".
 *  \param   s           A pointer to the string that is to be modified.
 *  \return  A reference to the modified string.
 */
std::string &RemoveChars(const std::string &remove_set, std::string * const s);


/** \brief   Removes all characters not in a supplied set from a string.
 *  \param   preserve_set  The "set" of characters that will not be removed from "s".
 *  \param   s             A pointer to the string that is to be modified.
 *  \return  A reference to the modified string.
 */
std::string &RemoveNotChars(const std::string &preserve_set, std::string * const s);


/** \brief   Checks whether "s" consists of decimal digits only.
 *  \param   s  The string to be tested.
 *  \return  True is "s" represents an unsigned integer number, false otherwise.
 *
 *  \note    Returns false for floating point numbers.
 */
bool IsUnsignedNumber(const std::string &s);


/** \brief   Checks whether "s" represents an unsigned integer or unsigned floating-point number.
 *  \param   s  The string to be tested.
 *  \return  True is "s" represents an unsigned integer or floating-point decimal number (no exponent allowed), false
 *           otherwise.
 */
bool IsUnsignedDecimalNumber(const std::string &s);


/** \brief   Converts ISO8859-15 (a.k.a. Latin-9) type strings to Unicode (UTF-8).
 *  \param   text  The ISO8859-15 string to convert.
 *  \return  The converted string (in UTF-8).
 */
std::string ISO8859_15ToUTF8(const std::string &text);


/** \brief  Converts as much Unicode (UTF-8) to ISO8859-15 as possible.
 *  \param  text                The UTF-8 string to convert.
 *  \param  unknown_char        The character to substitute if no ISO8859-15 equivalent exists.  Use a zero byte
 *                              ('\\0') if you want no substitute.
 *  \param  use_overlap_tokens  If true, "overlap_token" will be used to indicate that the unicode symbol just before
 *                              and just after the overlap token should be combined.  This is usually used to combine
 *                              standalone diacritical marks with their corresponding letters.
 *  \param  overlap_token       Only used if "use_overlap_token" is true.  This should be a byte code that can never
 *                              be (part or) a valid UTF-8 character sequence, in other words, it should start with at
 *                              least 5 1-bits.  Beware of confusion with byte-order markers!
 *  \return The converted string (in ISO8859-15).
 */
std::string UTF8ToISO8859_15(const std::string &text, const char unknown_char = '?',
                             const bool use_overlap_tokens = false,
                             const unsigned char overlap_token = 0xFFu);


/** \brief  Determines whether some text could conceivably be UTF-8 by looking for valid UTF-8 multibyte characters.
 *  \param  text  The string that we want to analyze.
 *  \return True if "text" represents a valid UTF-8 string, otherwise false.
 *  \note   This function does not check whether the string is in a canonical encoding and can't even guarantee whether
 *          the string is UTF-8.  All it guarantees is that the string is a possible UTF-8 string.  A better name for
 *          this function might be ContainsAtLeastOneUTF8MultibyteCharacterSequence().
 */
bool IsPossiblyUTF8(const std::string &text);


/** \brief  Removes any non-text characters from "text".
 *  \note   "Non-text" is defined as neither isspace nor isprint.  Furthermore non-breakable
 *          spaces (0xA0) are replaced by regular spaces (0x20).  We also attempt to map most
 *          of the codes in the range 0x80-0x9F (not used in Latin-1 or Latin-9) from Windows
 *          1250 to Latin-9 (same as Latin-1).
 *          Caution: the meaning of isprint and possibly isspace depends on the current locale!
 *  \return True if "text" has been changed else false.
 */
bool SanitizeText(std::string * const text);


/** \brief   "Word wrap" long lines to a given maximum length.
 *  \param   text           The text to wrap.
 *  \param   target_length  The desired line length.
 *  \return  A wrapped version of the text.
 */
std::string WordWrap(const std::string &text, const unsigned target_length = 72);


/** \brief  A saner version of strncpy(3).
 *  \param  dest       The destination string.
 *  \param  src        The source string.
 *  \param  dest_size  The size of "dest".
 *  \return strlen(src).
 *  \note   The result is always zero-terminated.  Buffer overflow can be checked as
 *          follows:
 *          \verbatim
 *                         if (StringUtil::strlcpy(dest, src, dest_size) >= dest_size)
 *                                 return -1;
 *          \endverbatim
 */
size_t strlcpy(char * const dest, const char * const src, const size_t dest_size);


/** \brief  A saner version of strncat(3).
 *  \param  dest       The destination string.
 *  \param  src        The source string.
 *  \param  dest_size  The size of "dest".
 *  \return strlen(dest) + strlen(src).
 *  \note   The result is always zero-terminated.  Buffer overflow can be checked as
 *          follows:
 *          \verbatim
 *                         if (StringUtil::strlcat(dest, src, dest_size) >= dest_size)
 *                                 return -1;
 *          \endverbatim
 */
size_t strlcat(char * const dest, const char * const src, const size_t dest_size);


/** \brief  Returns the number of alphanumeric chars in "text".
 *  \param  text The string whose alphanumeric length we'd like to claculate.
 *  \return The alphanumeric length of "text".
 */
inline size_t AlphanumericLength(const std::string &text)
{
        size_t length = 0;
        for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch)
                if (likely(isalnum(*ch)))
                        ++length;

        return length;
}


/** \brief  Returns the length of "text" not taking any whitespace
 *          characters into account.
 *  \param  text The string whose nonwhitespace length we'd like to claculate.
 *  \return The nonwhitespace length of "text".
 */
inline size_t NonWhitespaceLength(const std::string &text)
{
        size_t length = 0;
        for (std::string::const_iterator ch(text.begin()); ch != text.end(); ++ch)
                if (likely(not isspace(*ch)))
                        ++length;

        return length;
}


/** \brief  Tests whether "ch" is an ASCII lowercase letter or not.
 *  \param  ch  The character to test.
 *  \return True if "ch" is a lowercase ASCII letter, else returns false.
 */
inline bool IsLowercaseLetter(const char ch)
{
        // Caution: the following code assumes a character set where a-z are consecutive, e.g. ANSI or ASCII but not EBCDIC etc.
        return ch >= 'a' and ch <= 'z';
}


/** \brief  Tests whether "ch" is an ASCII lowercase letter or not.
 *  \param  ch  The character to test.
 *  \return True if "ch" is a lowercase ASCII letter, else returns false.
 */
inline bool IsLowercaseLetter(const int ch)
{
        // Caution: the following code assumes a character set where a-z are consecutive, e.g. ANSI or ASCII but not EBCDIC etc.
        return ch >= 'a' and ch <= 'z';
}


/** \brief  Returns what isalpha would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphabetic character in the "C" locale, else false.
 */
inline bool IsAsciiLetter(const char ch)
{
        // Caution: the following code assumes a character set where a-z, A-Z are consecutive, e.g. ANSI or ASCII
        //          but not EBCDIC etc.
        return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z');
}


/** \brief  Returns what isalpha would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphabetic character in the "C" locale, else false.
 */
inline bool IsAsciiLetter(const int ch)
{
        // Caution: the following code assumes a character set where a-z, A-Z are consecutive, e.g. ANSI or ASCII but not EBCDIC etc.
        return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z');
}


/** \brief  Returns what isalpha would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphabetic character in the "C" locale, else false.
 */
inline bool IsUppercaseAsciiLetter(const char ch)
{
        // Caution: the following code assumes a character set where A-Z are consecutive, e.g. ANSI or ASCII but not EBCDIC etc.
        return ch >= 'A' and ch <= 'Z';
}


/** \brief  Returns what isalpha would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphabetic character in the "C" locale, else false.
 */
inline bool IsUppercaseAsciiLetter(const int ch)
{
        // Caution: the following code assumes a character set where A-Z are consecutive, e.g. ANSI or ASCII but not EBCDIC etc.
        return ch >= 'A' and ch <= 'Z';
}


/** \brief  Returns what isdigit would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an numeric character in the "C" locale, else false.
 */
inline bool IsDigit(const char ch)
{
        // Caution: the following code assumes a character set where 0-9 are consecutive, e.g. ANSI, ASCII
        //          or EBCDIC etc.
        return ch >= '0' and ch <= '9';
}


/** \brief  Returns what isdigit would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an numeric character in the "C" locale, else false.
 */
inline bool IsDigit(const int ch)
{
        // Caution: the following code assumes a character set where 0-9 are consecutive, e.g. ANSI, ASCII
        //          or EBCDIC etc.
        return ch >= '0' and ch <= '9';
}


/** \brief  Returns what isalnum would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphanumeric character in the "C" locale, else false.
 */
inline bool IsAlphanumeric(const char ch)
{
        return IsAsciiLetter(ch) or IsDigit(ch);
}


/** \brief  Returns what isalnum would return in the "C" locale.
 *  \param  ch  The character to test.
 *  \return True if "ch" is an alphanumeric character in the "C" locale, else false.
 */
inline bool IsAlphanumeric(const int ch)
{
        return IsAsciiLetter(ch) or IsDigit(ch);
}


/** \brief   Test "s" for consisting entirely of alphanumeric characters or not.
 *  \param   s  The string to test.
 *  \return  True if all characters of "s" are alphanumeric in the currently selected locale, else false.
 *  \warning The empty string is considered to be alphanumeric!
 */
bool IsAlphanumeric(const std::string &s);


/** \brief   Does the given string start with the suggested prefix?
 *  \param   s            The string to test.
 *  \param   prefix       The prefix to test for.
 *  \param   ignore_case  If true, the match will be case-insensitive.
 *  \return  True if the string "s" equals or starts with the prefix "prefix."
 */
inline bool StartsWith(const std::string &s, const std::string &prefix, const bool ignore_case = false)
{
        return prefix.empty()
                or (s.length() >= prefix.length()
                    and (ignore_case ? (::strncasecmp(s.c_str(), prefix.c_str(), prefix.length()) == 0)
                         : (std::strncmp(s.c_str(), prefix.c_str(), prefix.length()) == 0)));
}


/** \brief   Does the given string end with the suggested suffix?
 *  \param   s            The string to test.
 *  \param   suffix       The suffix to test for.
 *  \param   ignore_case  If true, the match will be case-insensitive.
 *  \return  True if the string "s" equals or ends with the suffix "suffix."
 */
inline bool EndsWith(const std::string &s, const std::string &suffix, const bool ignore_case = false)
{
        return suffix.empty() or (s.length() >= suffix.length()
               and (ignore_case
                    ? (::strncasecmp(s.c_str() + (s.length() - suffix.length()), suffix.c_str(), suffix.length()) == 0)
                    : (std::strncmp(s.c_str() + (s.length() - suffix.length()), suffix.c_str(), suffix.length()) == 0)));
}


/** Returns true if "s" ends with "possible_last_char", else returns false. */
inline bool EndsWith(const std::string &s, const char possible_last_char)
{
        return not s.empty() and s[s.length() - 1] == possible_last_char;
}


/* \brief  Calculates the edit distance from "s1" to "s2".
 * \param  s1  The reference string (actually d(s1,s2) == d(s2,s1)).
 * \param  s2  The string to compare "s1" to.
 * \note   All weights are 1.0.  This function should only be used to compare very short strings.  It uses
 *         dynamic programming and takes O(s1.size() * s2.size()) time and space.
 * \return The edit distance.
 */
unsigned EditDistance(const std::string &s1, const std::string &s2);


/* \brief  Finds the longest series of consecutive letters occuring in both "s1" and "s2".
 * \param  s1  The first string.
 * \param  s2  The string to compare "s1" to.
 * \return The longest common substring.
 */
std::string LongestCommonSubstring(const std::string &s1, const std::string &s2);


/** \brief  Returns the MD5 cryptographic hash for "s".
 *  \param  s  The string for which we want the cryptographic hash.
 *  \return The cryptographic hash for "s".
 *  \note   Use Sha1() instead!
 */
std::string Md5(const std::string &s);


/** \brief  Returns a "folding" of the MD5 cryptographic hash for "s".
 *  \param  s  The string for which we want the cryptographic hash.
 *  \return The folded cryptographic hash for "s".
 *  \note   Use Sha1() instead!
 */
uint64_t Md5As64Bits(const std::string &s);


/** \brief  Returns the SHA-1 cryptographic hash for "s".
 *  \param  s  The string for which we want the cryptographic hash.
 *  \return The cryptographic hash for "s".
 */
std::string Sha1(const std::string &s);


/** Tries to fit an SHA-1 hash into a size_t by "folding" it using xor. */
size_t Sha1Hash(const std::string &s);


uint32_t SuperFastHash(const char * data, unsigned len);
inline size_t SuperFastHash(const std::string &s)
{
        return SuperFastHash(s.data(), static_cast<unsigned>(s.size()));
}


/** \brief    Calculates the Adler-32 checksum for "s".
 *  \param    s         The string whose checksum we desire.
 *  \param    s_length  The number of bytes in "s."
 *  \warning  Adler-32 has a weakness for short messages with few hundred bytes, because the checksums for these
 *            messages have a poor coverage of the 32 available bits.
 *  \warning  Like the standard CRC-32, the Adler-32 checksum can be forged easily and is therefore unsafe for
 *            protecting against intentional modification.
 *  \note     See for example http://en.wikipedia.org/wiki/Adler32 for documentation.
 */
uint32_t Adler32(const char * const s, const size_t s_length);


inline uint32_t Adler32(const std::string &s)
{
        return Adler32(s.c_str(), s.size());
}


/** Returns the string of all chars that pass isprint(). */
std::string GetPrintableChars();


/** Returns the string of all chars that fail isprint(). */
std::string GetNonprintableChars();


/** \return  The string of all chars that pass ispunct().
 *  \warning This depends on the locale and this function caches the result of the initial call!
 */
std::string GetPunctuationChars();


/** Capitalizes the first letter of "word." */
std::string &CapitalizeWord(std::string * const word);


inline std::string CapitalizeWord(const std::string &word)
{
        std::string temp(word);
        return CapitalizeWord(&temp);
}


/** Capitalises the first letter of each "word" in "text. */
std::string InitialCapsWords(const std::string &text);


/** Returns the initial substring that is shared by "s1" and "s2". */
std::string CommonPrefix(const std::string &s1, const std::string &s2);


/** Returns the final substring that is shared by "s1" and "s2". */
std::string CommonSuffix(const std::string &s1, const std::string &s2);


/** Returns the number of occurrences of "count_char" in "s".  Please note that "s" has to be NUL-terminated! */
size_t CharCount(const char *s, const char count_char);


/** Returns the number of occurrences of "count_char" in "s."  Please note that "s" is allowed to contain embedded NULs! */
inline size_t CharCount(const std::string &s, const char count_char)
{
        return std::count(s.begin(), s.end(), count_char);
}


/** Safely converts a C-style string to an std::string taking into account the possibility that the C-style string may
    be nullptr. */
inline std::string CStringToNonNullString(const char * const c_string)
{
        if (c_string == nullptr)
                return "";
        else
                return c_string;
}


/**
 * Create an std::string using printf style format string.  "format" cannot be std::string because gcc
 * __attribute__ checking does not understand std::string
*/
std::string Format(const char *format, ...) __attribute__((format(printf, 1, 2)));


/**
 * Create a cstyle string into a buffer you pass in. A one liner substitute for sprintf.  use like this for
 * example: Format(::alloca(200), 200, "File %s could not be opened", filename); Needed where speed is
 * important or where a mutable char* is necessary and ::strdup is not useful because it is not exception safe
 * and is a potential memory leak.
*/
char *FastFormat(void *buffer, const size_t max_length, const char *format, ...) __attribute__((format(printf, 3, 4)));


/** Returns true only if all characters of "test_string" return true for isalpha().  Returns false if "test_string" is
    empty! */
bool IsAlphabetic(const std::string &test_string);


/** Returns the first character position of "test_string" that matches any character in "match_set" or
    std::string::npos. */
std::string::size_type FindAnyOf(const std::string &test_string, const std::string &match_set);


/** Returns the last character position of "test_string" that matches any character in "match_set" or
    std::string::npos. */
std::string::size_type RFindAnyOf(const std::string &test_string, const std::string &match_set);


/** Returns the offset of the "nth" word in "target" using "word_separator_characters" to determine what separates words,
    or npos if such doesn't exist. */
std::string::size_type NthWordByteOffset(const std::string &target, const size_t nth, const std::string &word_separator_characters = WHITE_SPACE);


/** Returns an excerpt from "text", centered on "offset", of "length" characters. */
std::string Context(const std::string &text, const std::string::size_type offset, const std::string::size_type length = 60);


/** \brief   Tries to determine the best place to truncate a line of text, given that you want the result to be
 *           somewhat logical, sensible, a complete thought.
 *  \param   source_text   The text we want to extract from.
 *  \param   delimiters    The characters that delimit complete thoughts in the text. They will be tried in the order given, the first one found
 *                         will be used as the break point, with the text to the left of it being returned as the sensible subphrase. The characters .?!
                           if they are the delimiter which split the text, will be returned along with the subphrase.
 *  \param   minimum       Don't look in anything less than this offset in the source text.
 *  \param   maximum       Don't look any further than this offset in the text.
 *  \return  Returns the string determined to be the best subphrase.
 *  \note    This code assumes periods, commas, semicolons, !, ?, newlines and some other characters break on clauses that are complete thoughts. It
 *  starts at the "minimum" offset in the "source_text" and takes each delimiter in the order specified. If that delimiter occurs in the text, we are
 *  done and we return all the text up to that delimiter, less the delimiter. If the source_text is shorter than minimum, the entire text is
 *  returned.  The logic here is that a phrase shorter than "minimum" is the complete text, and thus likely to be a complete thought. If no breakpoint
 *  is found (highly unlikely if ' ' is one of the characters in "delimiters") the entire source_text up to "maximum" is returned.
*/
std::string ExtractSensibleSubphrase(const std::string &source_text, const std::string &delimiters = "!?.;,|\n\r\t ", const unsigned minimum = 20,
                                     const unsigned maximum = 200);


/** A predicate for comparing the relative ordering of two C-style strings lexicographically.  Returns true if "s1" preceeds "s2", otherwise false. */
inline bool strless(const char * const s1, const char * s2)
{
        return std::strcmp(s1, s2) < 0;
}


/** Combines the isspace() test with comparison against the non-break space character code. */
inline bool IsSpace(char ch)
{
        const char NO_BREAK_SPACE('\xA0');
        return isspace(ch) or ch == NO_BREAK_SPACE;
}


/** Creates a C-style escape sequance, e.g. turns a newline into "\n" (that's a literal backslash followed by an "n") etc. and other
    non-printable characters that have no standard C backslash escape into octal sequences starting with "\0".  Backslashes get turned into
    double backslashes. */
std::string CStyleEscape(const char ch);


/** Creates a C-style string, e.g. turns a newline into "\n" (that's a literal backslash followed by an "n") etc. and other non-printable
    characters that have no standard C backslash escape into octal sequences starting with "\0".  Backslashes get turned into double backslashes. */
std::string CStyleEscape(const std::string &unescaped_text);


/** Returns the character code for a sequence "\c" where "c" has to be one of "ntbrfva\".  For any other symbol "c" itself will be returned. */
char CStyleUnescape(const char c);


/** Counterpart to CStyleEscape(). */
std::string CStyleUnescape(const std::string &escaped_text);


inline bool IsLatin9Whitespace(const char ch)
{
        return std::strchr(WHITE_SPACE.c_str(), ch) != nullptr;
}


/** \brief  Generates a string of random characters.
 *  \param  length          How many random characters to generate.
 *  \param  character_pool  Which characters to choose from.  The empty string implies choosing uniformly from all 256 character codes.
 *  \return The random string.
 */
std::string GenerateRandomString(const unsigned length, const std::string &character_pool = "");


/** \brief  Determines if a given string is a suffix of another given string.
 *  \param  suffix_candidate  The suffix that we'd like to test for.
 *  \param  s                 The string that may or may not have the suffix "suffix_candidate."
 */
inline bool IsSuffixOf(const std::string &suffix_candidate, const std::string &s)
{
        if (s.length() < suffix_candidate.length())
                return false;

        return std::memcmp(s.data() + s.length() - suffix_candidate.length(), suffix_candidate.data(), suffix_candidate.length()) == 0;
}


/** \brief  Determines if a given string is a \em{proper} suffix of another given string.
 *  \param  suffix_candidate  The suffix that we'd like to test for.
 *  \param  s                 The string that may or may not have the suffix "suffix_candidate."
 */
inline bool IsProperSuffixOf(const std::string &suffix_candidate, const std::string &s)
{
        if (s.length() <= suffix_candidate.length())
                return false;

        return std::memcmp(s.data() + s.length() - suffix_candidate.length(), suffix_candidate.data(), suffix_candidate.length()) == 0;
}


/** \brief  Determines if a given string is a \em{proper} suffix of another given string irrespective of capitalisation.
 *  \param  suffix_candidate  The suffix that we'd like to test for.
 *  \param  s                 The string that may or may not have the suffix "suffix_candidate."
 */
inline bool IsProperSuffixOfIgnoreCase(const std::string &suffix_candidate, const std::string &s)
{
        if (s.length() <= suffix_candidate.length())
                return false;

        return ::strcasecmp(s.c_str() + s.length() - suffix_candidate.length(), suffix_candidate.c_str()) == 0;
}


/** \brief  Determines if a given string is a prefix of another given string.
 *  \param  prefix_candidate  The prefix that we'd like to test for.
 *  \param  s                 The string that may or may not have the prefix "prefix_candidate."
 */
inline bool IsPrefixOf(const std::string &prefix_candidate, const std::string &s)
{
        if (s.length() < prefix_candidate.length())
                return false;

        return std::memcmp(s.data(), prefix_candidate.data(), prefix_candidate.length()) == 0;
}


/** \brief  Determines if a given string is a prefix of another given string \em{irrespective of capitalisation}.
 *  \param  prefix_candidate  The prefix that we'd like to test for.
 *  \param  s                 The string that may or may not have the prefix "prefix_candidate."
 */
inline bool IsPrefixOfIgnoreCase(const std::string &prefix_candidate, const std::string &s)
{
        if (s.length() < prefix_candidate.length())
                return false;

        return ::strncasecmp(s.data(), prefix_candidate.data(), prefix_candidate.length()) == 0;
}


/** \brief  Determines if a given string is a \em{proper} prefix of another given string.
 *  \param  prefix_candidate  The prefix that we'd like to test for.
 *  \param  s                 The string that may or may not have the prefix "prefix_candidate."
 */
inline bool IsProperPrefixOf(const std::string &prefix_candidate, const std::string &s)
{
        if (s.length() <= prefix_candidate.length())
                return false;

        return std::memcmp(s.data(), prefix_candidate.data(), prefix_candidate.length()) == 0;
}


/** \brief  Determines if a given string is a \em{proper} prefix of another given string \em{irrespective of capitalisation}.
 *  \param  prefix_candidate  The prefix that we'd like to test for.
 *  \param  s                 The string that may or may not have the prefix "prefix_candidate."
 */
inline bool IsProperPrefixOfIgnoreCase(const std::string &prefix_candidate, const std::string &s)
{
        if (s.length() <= prefix_candidate.length())
                return false;

        return ::strncasecmp(s.data(), prefix_candidate.data(), prefix_candidate.length()) == 0;
}


/** \class StringEqual
 *  \brief Binary functor that returns true if two std::strings are equal.
 */
class StringEqual: public std::binary_function<std::string, std::string, bool> {
public:
        bool operator()(const std::string &s1, const std::string &s2) const { return s1 == s2; }
};


/** \class StringCaseEqual
 *  \brief Binary functor that returns true if two std::strings are equal independent of case.
 */
class StringCaseEqual: public std::binary_function<std::string, std::string, bool> {
public:
        bool operator()(const std::string &s1, const std::string &s2) const { return ::strcasecmp(s1.c_str(), s2.c_str()) == 0; }
};


/** \brief  Removes a trailing newline or carriage-return-newline sequence, if present. */
std::string Chomp(std::string * const line);


/** \brief  Determines whether a string "s" only contains characters from a given set "set" or not. */
bool ConsistsOf(const std::string &s, const std::set<char> &set);


template<typename Number> inline std::string BinaryToString(Number n)
{
        std::string n_as_string;
        for (unsigned bit(0); bit < sizeof(n) * BITSPERBYTE; ++bit, n >>= 1u)
                n_as_string += (n & 1u) ? '1' : '0';
        std::reverse(n_as_string.begin(), n_as_string.end());

        return n_as_string;
}


template<typename Number> inline Number StringToBinary(const std::string &bits)
{
        errno = 0;
        char *end_ptr;
        unsigned long long binary(::strtoull(bits.c_str(), &end_ptr, 2));
        if (unlikely(errno != 0 or *end_ptr != '\0'))
                throw std::runtime_error("in StringUtil::BinaryToString: \"" + bits + "\" is not a valid binary string!");

        return static_cast<Number>(binary);
}


/** \brief   Predicate to determine whether a string "s" contains at least a single lowercase letter or not.
 *  \warning This function is locale dependent in that its notion of what consitutes a lowercase letter depends on the current locale setting!
 */
bool ContainsAtLeastOneLowercaseLetter(const std::string &s);


/** Pads "s" with leading "pad_char"'s if s.length() < min_length. */
std::string PadLeading(const std::string &s, const std::string::size_type min_length, const char pad_char = ' ');


/** \return If "needle" was found in "haystack", the starting position in "haystack" else std::string::npos. */
size_t FindCaseInsensitive(const std::string &haystack, const std::string &needle);


/** Removes all occurrences of any of the characters in "remove_set" from "source"
    and returns the result. */
std::string Filter(const std::string &source, const std::string &remove_set);


} // Namespace StringUtil


#endif // ifndef STRING_UTIL_H
