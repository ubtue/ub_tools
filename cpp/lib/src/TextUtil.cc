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

#include "TextUtil.h"
#include <algorithm>
#include <exception>
#include <cstdio>
#include <cwctype>
#include "Compiler.h"
#include "Locale.h"
#include "HtmlParser.h"
#include "RegexMatcher.h"
#include "StringUtil.h"


namespace {


class TextExtractor: public HtmlParser {
    std::string &extracted_text_;
public:
    TextExtractor(const std::string &html, std::string * const extracted_text)
        : HtmlParser(html, HtmlParser::TEXT), extracted_text_(*extracted_text) { }
    virtual void notify(const HtmlParser::Chunk &chunk);
};


void TextExtractor::notify(const HtmlParser::Chunk &chunk) {
    if (chunk.type_ == HtmlParser::TEXT)
        extracted_text_ += chunk.text_;
}


} // unnamned namespace


namespace TextUtil {


std::string ExtractText(const std::string &html) {
    std::string extracted_text;
    TextExtractor extractor(html, &extracted_text);
    extractor.parse();

    return extracted_text;
}


bool IsRomanNumeral(const std::string &s) {
    if (s.empty())
        return false;

    std::string err_msg;
    static RegexMatcher *matcher(nullptr);
    if (unlikely(matcher == nullptr)) {
        const std::string pattern("^M{0,4}(CM|CD|D?C{0,3})(XC|XL|L?X{0,3})(IX|IV|V?I{0,3})$");
        matcher = RegexMatcher::RegexMatcherFactory(pattern, &err_msg);
        if (unlikely(matcher == nullptr))
            throw std::runtime_error("Failed to construct a RegexMatcher for \"" + pattern
                                     + "\" in TextUtil::IsRomanNumeral: " + err_msg);
    }

    const bool retcode(matcher->matched(s, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("Failed to match \"" + s + "\" against pattern \"" + matcher->getPattern()
                                 + "\" in TextUtil::IsRomanNumeral: " + err_msg);

    return retcode;
}


bool IsUnsignedInteger(const std::string &s) {
    std::string err_msg;
    static RegexMatcher *matcher(nullptr);
    if (unlikely(matcher == nullptr)) {
        const std::string pattern("^[0-9]+$");
        matcher = RegexMatcher::RegexMatcherFactory(pattern, &err_msg);
        if (unlikely(matcher == nullptr))
            throw std::runtime_error("Failed to construct a RegexMatcher for \"" + pattern
                                     + "\" in TextUtil::IsUnsignedInteger: " + err_msg);
    }

    const bool retcode(matcher->matched(s, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("Failed to match \"" + s + "\" against pattern \"" + matcher->getPattern()
                                 + "\" in TextUtil::IsUnsignedInteger: " + err_msg);

    return retcode;
}


bool UTF8toWCharString(const std::string &utf8_string, std::wstring * wchar_string) {
    wchar_string->clear();

    const char *cp(utf8_string.c_str());
    size_t remainder(utf8_string.size());
    std::mbstate_t state = std::mbstate_t();
    while (*cp != '\0') {
        wchar_t wch;
        const size_t retcode(std::mbrtowc(&wch, cp, remainder, &state));
        if (retcode == static_cast<size_t>(-1) or retcode == static_cast<size_t>(-2))
            return false;
        if (retcode == 0)
            return true;
        *wchar_string += wch;
        cp += retcode;
        remainder -= retcode;
    }

    return true;
}


bool WCharToUTF8String(const std::wstring &wchar_string, std::string * utf8_string) {
    utf8_string->clear();

    char buf[6];
    std::mbstate_t state = std::mbstate_t();
    for (const auto wch : wchar_string) {
        const size_t retcode(std::wcrtomb(buf, wch, &state));
        if (retcode == static_cast<size_t>(-1))
            return false;
        utf8_string->append(buf, retcode);
    }

    return true;
}


bool WCharToUTF8String(const wchar_t wchar, std::string * utf8_string) {
    utf8_string->clear();

    char buf[6];
    std::mbstate_t state = std::mbstate_t();
    const size_t retcode(std::wcrtomb(buf, wchar, &state));
    if (retcode == static_cast<size_t>(-1))
        return false;
    utf8_string->append(buf, retcode);
    return true;
}


bool UTF8ToLower(const std::string &utf8_string, std::string * const lowercase_utf8_string) {
    std::wstring wchar_string;
    if (not UTF8toWCharString(utf8_string, &wchar_string))
        return false;

    // Lowercase the wide character string:
    std::wstring lowercase_wide_string;
    for (const auto wide_ch : wchar_string) {
        if (std::iswupper(static_cast<wint_t>(wide_ch)))
            lowercase_wide_string += std::towlower(static_cast<wint_t>(wide_ch));
        else
            lowercase_wide_string += wide_ch;
    }

    return WCharToUTF8String(lowercase_wide_string, lowercase_utf8_string);
}


namespace {


/** \return True if "number_candidate" is non-empty and consists only of characters belonging
 *          to the wide-character class "digit"
 */
bool IsNumber(const std::wstring &number_candidate) {
    if (number_candidate.empty())
        return false;

    for (const wchar_t ch : number_candidate) {
        if (not std::iswdigit(ch))
            return false;
    }

    return true;
}


template<typename ContainerType> bool ChopIntoWords(const std::string &text, ContainerType * const words,
                                                    const unsigned min_word_length)
{
    words->clear();

    std::wstring wide_text;
    if (unlikely(not UTF8toWCharString(text, &wide_text)))
        return false;

    std::wstring word;
    std::string utf8_word;
    bool leading(true);
    for (const wchar_t ch : wide_text) {
        if (leading and (ch == L'-' or ch == L'\''))
            ; // Do nothing!
        else if (iswalnum(ch) or ch == L'-' or ch == L'\'') {
            word += ch;
            leading = false;
        } else if (ch == L'.' and IsNumber(word)) {
            word += ch;
            if (word.length() >= min_word_length) {
                if (unlikely(not WCharToUTF8String(word, &utf8_word)))
                    return false;
                words->insert(words->end(), utf8_word);
            }
            word.clear();
            leading = true;
        } else {
            // Remove trailing and leading hyphens and quotes:
            while (word.length() > 0 and (word[word.length() - 1] == L'-' or word[word.length() - 1] == L'\''))
                word.resize(word.length() - 1);
            if (word.length() >= min_word_length) {
                if (unlikely(not WCharToUTF8String(word, &utf8_word)))
                    return false;
                words->insert(words->end(), utf8_word);
            }
            word.clear();
            leading = true;
        }
    }

    // Remove trailing and leading hyphens and quotes:
    while (word.length() > 0 and word[word.length() - 1] == '-')
        word.resize(word.length() - 1);
    if (word.length() >= min_word_length) {
        if (unlikely(not WCharToUTF8String(word, &utf8_word)))
            return false;
        words->insert(words->end(), utf8_word);
    }

    return true;
}


} // unnamed namespace


bool ChopIntoWords(const std::string &text, std::unordered_set<std::string> * const words,
                   const unsigned min_word_length)
{
    return ChopIntoWords<std::unordered_set<std::string>> (text, words, min_word_length);
}


bool ChopIntoWords(const std::string &text, std::vector<std::string> * const words,
                   const unsigned min_word_length)
{
    return ChopIntoWords<std::vector<std::string>> (text, words, min_word_length);
}


std::vector<std::string>::const_iterator FindSubstring(const std::vector<std::string> &haystack,
                                                       const std::vector<std::string> &needle)
{
    if (needle.empty())
        return haystack.cbegin();

    std::vector<std::string>::const_iterator search_start(haystack.cbegin());
    while (search_start != haystack.cend()) {
        const std::vector<std::string>::const_iterator haystack_start(
            std::find(search_start, haystack.cend(), needle[0]));
        if ((haystack.cend() - haystack_start) < static_cast<ssize_t>(needle.size()))
            return haystack.cend();

        std::vector<std::string>::const_iterator needle_word(needle.cbegin());
        std::vector<std::string>::const_iterator haystack_word(haystack_start);
        for (;;) {
            ++needle_word;
            if (needle_word == needle.cend())
                return haystack_start;
            ++haystack_word;
            if (haystack_word == haystack.cend())
                return haystack.cend();
            else if (*haystack_word != *needle_word) {
                search_start = haystack_start + 1;
                break;
            }
        }
    }

    return haystack.cend();
}


std::string Base64Encode(const std::string &s, const char symbol63, const char symbol64) {
    static char symbols[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\0\0";
    symbols[62] = symbol63;
    symbols[63] = symbol64;

    std::string encoded_chars;
    std::string::const_iterator ch(s.begin());
    while (ch != s.end()) {
        // Collect groups of 3 characters:
        unsigned buf(static_cast<unsigned char>(*ch));
        buf <<= 8u;
        ++ch;
        unsigned ignore_count(0);
        if (ch != s.end()) {
            buf |= static_cast<unsigned char>(*ch);
            ++ch;
        } else
            ++ignore_count;
        buf <<= 8u;
        if (ch != s.end()) {
            buf |= static_cast<unsigned char>(*ch);
            ++ch;
        }
        else
            ++ignore_count;

        // Now grab 6 bits at a time and encode them starting with the 4th character:
        char next4[4];
        for (unsigned char_no(0); char_no < 4; ++char_no) {
            next4[4 - 1 - char_no] = symbols[buf & 0x3Fu];
            buf >>= 6u;
        }

        for (unsigned char_no(0); char_no < 4 - ignore_count; ++char_no)
            encoded_chars += next4[char_no];
    }

    return encoded_chars;
}


inline bool IsWhiteSpace(const char ch) {
    return ch == ' ' or ch == '\t' or ch == '\n' or ch == '\v' or ch == '\xA0';
}


inline std::string OctalEscape(const char ch) {
    char buf[1 + 3 + 1];
    std::sprintf(buf, "\\%03o", ch);
    return buf;
}


std::string EscapeString(const std::string &original_string, const bool also_escape_whitespace) {
    std::string escaped_string;
    escaped_string.reserve(original_string.size() * 2);

    for (char ch : original_string) {
        if (std::iscntrl(ch) or (not also_escape_whitespace or IsWhiteSpace(ch)))
            escaped_string += OctalEscape(ch);
        else
            escaped_string += ch;
    }

    return escaped_string;
}


} // namespace TextUtil
