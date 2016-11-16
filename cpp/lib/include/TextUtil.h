/** \file    TextUtil.h
 *  \brief   Declarations of text related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
 *  Copyright 2015 Universitätsbibliothek Tübingen.
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

#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H


#include <string>
#include <unordered_set>
#include <vector>
#include <cwchar>


namespace TextUtil {


/** \brief Strips HTML tags and converts entities. */
std::string ExtractText(const std::string &html);


/** \brief Recognises roman numerals up to a few thousand. */
bool IsRomanNumeral(const std::string &s);


/** \brief Recognises base-10 unsigned integers. */
bool IsUnsignedInteger(const std::string &s);


/** \brief Convert UTF8 to wide characters. */
bool UTF8toWCharString(const std::string &utf8_string, std::wstring * wchar_string);


/** \brief Convert wide characters to UTF8. */
bool WCharToUTF8String(const std::wstring &wchar_string, std::string * utf8_string);


/** \brief Convert a wide character to UTF8. */
bool WCharToUTF8String(const wchar_t wchar, std::string * utf8_string);
    

/** \brief Converts a UTF8 string to lowercase.
 *  \return True if no character set conversion error occurred, o/w false.
  */
bool UTF8ToLower(const std::string &utf8_string, std::string * const lowercase_utf8_string);


/** \brief Break up text into individual lowercase "words".
 *
 *  \param text             Assumed to be in UTF8.
 *  \param words            The individual words, also in UTF8.
 *  \param min_word_length  Reject chunks that are shorter than this.
 *  \return True if there were no character conversion problems, else false.
 */
bool ChopIntoWords(const std::string &text, std::unordered_set<std::string> * const words,
                   const unsigned min_word_length = 1);


/** \brief Break up text into individual lowercase "words".
 *
 *  \param text             Assumed to be in UTF8.
 *  \param words            The individual words, also in UTF8.
 *  \param min_word_length  Reject chunks that are shorter than this.
 *  \return True if there were no character conversion problems, else false.
 */
bool ChopIntoWords(const std::string &text, std::vector<std::string> * const words,
                   const unsigned min_word_length = 1);


/** \return The position at which "needle" starts in "haystack" or "haystack.cend()" if "needle"
    is not in "haystack". */
std::vector<std::string>::const_iterator FindSubstring(const std::vector<std::string> &haystack,
                                                       const std::vector<std::string> &needle);


/** \brief  Base64 encodes a string.
 *  \param  s         The string that will be encoded.
 *  \param  symbol63  The character that will be used for symbol 63.
 *  \param  symbol64  The character that will be used for symbol 64.
 *  \return The encoded string.
 */
std::string Base64Encode(const std::string &s, const char symbol63 = '+', const char symbol64 = '/');


/** \brief Replaces non-printable characters with octal C-style escapes.
 *  \param also_escape_whitespace  if true, whitespace characters tab, vertical tab, newline, space and
 *         hard space will also be escaped.
 */
std::string EscapeString(const std::string &original_string, const bool also_escape_whitespace = false);

    
} // namespace TextUtil


#endif // ifndef TEXT_UTIL_H
