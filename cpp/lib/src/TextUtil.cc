/** \file    TextUtil.h
 *  \brief   Declarations of text related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
 *  Copyright 2015,2017 Universitätsbibliothek Tübingen.
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
#include <memory>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <iconv.h>
#include "Compiler.h"
#include "Locale.h"
#include "HtmlParser.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


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


std::string ExtractTextFromHtml(const std::string &html) {
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
    static iconv_t iconv_handle((iconv_t)-1);
    if (unlikely(iconv_handle == (iconv_t)-1)) {
        iconv_handle = ::iconv_open("UTF-8","WCHAR_T");
        if (unlikely(iconv_handle == (iconv_t)-1))
            Error("in TextUtil::WCharToUTF8String: iconv_open(3) failed!");
    }

    const size_t INBYTE_COUNT(wchar_string.length() * sizeof(wchar_t));
    char *in_bytes(new char[INBYTE_COUNT]);
    const char *in_bytes_start(in_bytes);
    ::memcpy(reinterpret_cast<void *>(in_bytes), wchar_string.data(), INBYTE_COUNT);
    static const size_t UTF8_SEQUENCE_MAXLEN(6);
    const size_t OUTBYTE_COUNT(UTF8_SEQUENCE_MAXLEN * wchar_string.length());
    char *out_bytes(new char[OUTBYTE_COUNT]);
    const char *out_bytes_start(out_bytes);

    size_t inbytes_left(INBYTE_COUNT), outbytes_left(OUTBYTE_COUNT);
    const ssize_t converted_count(static_cast<ssize_t>(::iconv(iconv_handle, &in_bytes, &inbytes_left, &out_bytes,
                                                               &outbytes_left)));
    if (unlikely(converted_count == -1))
        Error("in TextUtil::WCharToUTF8String: iconv(3) failed!");

    utf8_string->assign(out_bytes_start, OUTBYTE_COUNT - outbytes_left);
    delete [] in_bytes_start;
    delete [] out_bytes_start;

    return true;
}


bool WCharToUTF8String(const wchar_t wchar, std::string * utf8_string) {
    const std::wstring wchar_string(1, wchar);
    return WCharToUTF8String(wchar_string, utf8_string);
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


bool UTF8ToUpper(const std::string &utf8_string, std::string * const uppercase_utf8_string) {
    std::wstring wchar_string;
    if (not UTF8toWCharString(utf8_string, &wchar_string))
        return false;

    // Uppercase the wide character string:
    std::wstring uppercase_wide_string;
    for (const auto wide_ch : wchar_string) {
        if (std::iswlower(static_cast<wint_t>(wide_ch)))
            uppercase_wide_string += std::towupper(static_cast<wint_t>(wide_ch));
        else
            uppercase_wide_string += wide_ch;
    }

    return WCharToUTF8String(uppercase_wide_string, uppercase_utf8_string);
}


/** The following conversions are implemented here:

    Unicode range 0x00000000 - 0x0000007F:
       Returned byte: 0xxxxxxx

    Unicode range 0x00000080 - 0x000007FF:
       Returned bytes: 110xxxxx 10xxxxxx

    Unicode range 0x00000800 - 0x0000FFFF:
       Returned bytes: 1110xxxx 10xxxxxx 10xxxxxx

    Unicode range 0x00010000 - 0x001FFFFF:
       Returned bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

    Unicode range 0x00200000 - 0x03FFFFFF:
       Returned bytes: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

    Unicode range 0x04000000 - 0x7FFFFFFF:
       Returned bytes: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
std::string UTF32ToUTF8(const uint32_t code_point) {
    std::string utf8;

    if (code_point <= 0x7Fu)
        utf8 += static_cast<char>(code_point);
    else if (code_point <= 0x7FFu) {
        utf8 += static_cast<char>(0b11000000u | (code_point >> 6u));
        utf8 += static_cast<char>(0b10000000u | (code_point & 0b00111111u));
    } else if (code_point <= 0xFFFF) {
        utf8 += static_cast<char>(0b11100000u | (code_point >> 12u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 6u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | (code_point & 0b00111111u));
    } else if (code_point <= 0x1FFFFF) {
        utf8 += static_cast<char>(0b11110000u | (code_point >> 18u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 12u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 6u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | (code_point & 0b00111111u));
    } else if (code_point <= 0x3FFFFFF) {
        utf8 += static_cast<char>(0b11111000u | (code_point >> 24u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 18u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 12u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 6u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | (code_point & 0b00111111u));
    } else if (code_point <= 0x7FFFFFFF) {
        utf8 += static_cast<char>(0b11111100u | (code_point >> 30u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 24u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 18u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 12u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | ((code_point >> 6u) & 0b00111111u));
        utf8 += static_cast<char>(0b10000000u | (code_point & 0b00111111u));
    } else
        throw std::runtime_error("in TextUtil::UTF32ToUTF8: invalid Unicode code point 0x"
                                 + StringUtil::ToHexString(code_point) + "!");

    return utf8;
}


bool UTF8ToUTF32(const std::string &utf8_string, std::vector<uint32_t> * utf32_chars) {
    utf32_chars->clear();

    UTF8ToUTF32Decoder decoder;
    try {
        bool last_addByte_retval(false);
        for (const char ch : utf8_string) {
            if (not (last_addByte_retval = decoder.addByte(ch)))
                utf32_chars->emplace_back(decoder.getUTF32Char());
        }

        return not last_addByte_retval;
    } catch (...) {
        return false;
    }
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


static char base64_symbols[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\0\0";


std::string Base64Encode(const std::string &s, const char symbol63, const char symbol64,
                         const bool use_output_padding)
{
    base64_symbols[62] = symbol63;
    base64_symbols[63] = symbol64;

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
            next4[4 - 1 - char_no] = base64_symbols[buf & 0x3Fu];
            buf >>= 6u;
        }

        for (unsigned char_no(0); char_no < 4 - ignore_count; ++char_no)
            encoded_chars += next4[char_no];
    }

    if (not use_output_padding)
        return encoded_chars;

    switch (encoded_chars.size() % 3) {
    case 1:
        return encoded_chars + "==";
    case 2:
        return encoded_chars + "=";
    default:
        return encoded_chars;
    }
}


std::string Base64Decode(const std::string &s, const char symbol63, const char symbol64) {
    static unsigned char char_to_bits_map[128];
    static bool initialised(false);
    if (not initialised) {
        unsigned char mask(0);
        for (const char *cp(base64_symbols); *cp != '\0'; ++cp)
            char_to_bits_map[static_cast<unsigned char>(*cp)] = mask++;
        initialised = true;
    }
    char_to_bits_map[static_cast<unsigned char>(symbol63)] = 62;
    char_to_bits_map[static_cast<unsigned char>(symbol64)] = 63;

    std::string decoded_chars;
    unsigned state(1);
    unsigned char decoded_ch;
    for (const char encoded_ch : s) {
        const unsigned char ch(char_to_bits_map[static_cast<unsigned char>(encoded_ch)]);
        switch (state) {
        case 1:
            decoded_ch = ch << 2u;
            state = 2;
            break;
        case 2:
            decoded_ch |= ch >> 4u;
            decoded_chars += static_cast<char>(decoded_ch);
            decoded_ch = (ch & 0b001111u) << 4u;
            state = 3;
            break;
        case 3:
            decoded_ch |= ch >> 2u;
            decoded_chars += static_cast<char>(decoded_ch);
            decoded_ch = ch << 6u;
            state = 4;
            break;
        case 4:
            decoded_ch |= ch;
            decoded_chars += static_cast<char>(decoded_ch);
            state = 1;
            break;
        }
    }

    return decoded_chars;
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


std::string CSVEscape(const std::string &value) {
    std::string escaped_value;
    escaped_value.reserve(value.length());

    for (const char ch : value) {
        if (unlikely(ch == '"'))
            escaped_value += '"';
        escaped_value += ch;
    }

    return escaped_value;
}


// See https://en.wikipedia.org/wiki/UTF-8 in order to understand this implementation.
bool TrimLastCharFromUTF8Sequence(std::string * const s) {
    if (unlikely(s->empty()))
        return false;

    int i(s->length() - 1);
    while (i >=0 and ((*s)[i] & 0b11000000) == 0b10000000)
        --i;
    if (unlikely(i == -1))
        return false;

    switch (s->length() - i) {
    case 1:
        if (((*s)[i] & 0b10000000) == 0b00000000) {
            s->resize(s->length() - 1);
            return true;
        }
        return false;
    case 2:
        if (((*s)[i] & 0b11100000) == 0b11000000) {
            s->resize(s->length() - 2);
            return true;
        }
        return false;
    case 3:
        if (((*s)[i] & 0b11110000) == 0b11100000) {
            s->resize(s->length() - 3);
            return true;
        }
        return false;
    case 4:
        if (((*s)[i] & 0b11111000) == 0b11110000) {
            s->resize(s->length() - 4);
            return true;
        }
        return false;
    default:
        return false;
    }
}


bool UTF32CharIsAsciiLetter(const uint32_t ch) {
    return ('A' <= ch and ch <= 'Z') or ('a' <= ch and ch <= 'z');
}


bool UTF32CharIsAsciiDigit(const uint32_t ch) {
    return '0' <= ch and ch <= '9';
}


bool UTF8ToUTF32Decoder::addByte(const char ch) {
    if (required_count_ == -1) {
        if ((static_cast<unsigned char>(ch) & 0b10000000) == 0b00000000) {
            utf32_char_ = static_cast<unsigned char>(ch);
            required_count_ = 0;
        } else if ((static_cast<unsigned char>(ch) & 0b11100000) == 0b11000000) {
            utf32_char_ = static_cast<unsigned char>(ch) & 0b11111;
            required_count_ = 1;
        } else if ((static_cast<unsigned char>(ch) & 0b11110000) == 0b11100000) {
            utf32_char_ = static_cast<unsigned char>(ch) & 0b1111;
            required_count_ = 2;
        } else if ((static_cast<unsigned char>(ch) & 0b11111000) == 0b11110000) {
            utf32_char_ = static_cast<unsigned char>(ch) & 0b111;
            required_count_ = 3;
        } else
            #ifndef __clang__
            #    pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
            #endif
            throw std::runtime_error("in TextUtil::UTF8ToUTF32Decoder::addByte: bad UTF-8 byte "
                                     "sequence! (partial utf32_char: 0x" + StringUtil::ToHexString(utf32_char_)
                                     + ", current char 0x" + StringUtil::ToHexString(static_cast<unsigned char>(ch))
                                     + ")");
            #ifndef __clang__
            #    pragma GCC diagnostic warning "-Wmaybe-uninitialized"
            #endif
    } else if (required_count_ > 0) {
        --required_count_;
        utf32_char_ <<= 6u;
        utf32_char_ |= (static_cast<unsigned char>(ch) & 0b00111111);
    }

    return required_count_ != 0;
}


// In order to understand this implementation, it may help to read https://en.wikipedia.org/wiki/Quoted-printable
std::string EncodeQuotedPrintable(const std::string &s) {
    if (unlikely(s.empty()))
        return s;

    std::string encoded_string;

    for (const char ch : s) {
        const unsigned char uch(static_cast<unsigned char>(ch));
        if ((uch >= 33 and uch <= 126) or uch == 9 or uch == 32) {
            if (unlikely(ch == '='))
                encoded_string += "=3D";
            else
                encoded_string += ch;
        } else {
            encoded_string += '=';
            encoded_string += StringUtil::ToHex(uch >> 4u);
            encoded_string += StringUtil::ToHex(uch & 0xFu);
        }
    }

    // Tab and space at the end of the string must be encoded:
    if (unlikely(encoded_string[encoded_string.size() - 1] == ' ')) {
        encoded_string.resize(encoded_string.size() - 1);
        encoded_string += "=20";
    } else if (unlikely(encoded_string[encoded_string.size() - 1] == '\t')) {
        encoded_string.resize(encoded_string.size() - 1);
        encoded_string += "=09";
    }

    return encoded_string;
}


static char HEX_CHARS[] = "0123456789ABCDEF";


// In order to understand this implementation, it may help to read https://en.wikipedia.org/wiki/Quoted-printable
std::string DecodeQuotedPrintable(const std::string &s) {
    std::string decoded_string;

    for (auto ch(s.cbegin()); ch != s.cend(); ++ch) {
        if (unlikely(*ch == '=')) {
            // First nibble:
            ++ch;
            if (unlikely(ch == s.cend()))
                throw std::runtime_error("TextUtil::DecodeQuotedPrintable: bad character sequence! (1)");
            char *cp(std::strchr(HEX_CHARS, *ch));
            if (unlikely(cp == nullptr))
                throw std::runtime_error("TextUtil::DecodeQuotedPrintable: bad character sequence! (2)");
            unsigned char uch(cp - HEX_CHARS);
            uch <<= 4u;

            // Second nibble:
            ++ch;
            if (unlikely(ch == s.cend()))
                throw std::runtime_error("TextUtil::DecodeQuotedPrintable: bad character sequence! (3)");
            cp = std::strchr(HEX_CHARS, *ch);
            if (unlikely(cp == nullptr))
                throw std::runtime_error("TextUtil::DecodeQuotedPrintable: bad character sequence! (4)");
            uch |= cp - HEX_CHARS;

            decoded_string += static_cast<char>(uch);
        } else
            decoded_string += *ch;
    }

    return decoded_string;
}


} // namespace TextUtil
