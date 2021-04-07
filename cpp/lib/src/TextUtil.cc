/** \file    TextUtil.cc
 *  \brief   Implementation of text related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
 *  Copyright 2015-2021 Universitätsbibliothek Tübingen.
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
#include <locale>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include "Compiler.h"
#include "FileUtil.h"
#include "HtmlParser.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "XMLParser.h"
#include "util.h"


namespace {


class TextExtractor: public HtmlParser {
    std::string &extracted_text_;
    std::string charset_;
public:
    TextExtractor(const std::string &html, const std::string &initial_charset, std::string * const extracted_text)
        : HtmlParser(html, initial_charset, HtmlParser::TEXT | HtmlParser::OPENING_TAG | HtmlParser::END_OF_STREAM),
          extracted_text_(*extracted_text) { }
    virtual void notify(const HtmlParser::Chunk &chunk);
};


void TextExtractor::notify(const HtmlParser::Chunk &chunk) {
    if (chunk.type_ == HtmlParser::TEXT) {
        if (not StringUtil::EndsWith(chunk.text_, " ") and not StringUtil::EndsWith(chunk.text_, "\n"))
            extracted_text_ += " ";
        extracted_text_ += chunk.text_;
    } else if (charset_.empty() and chunk.type_ == HtmlParser::OPENING_TAG
             and StringUtil::ASCIIToLower(chunk.text_) == "meta") {

        auto key_and_value(chunk.attribute_map_->find("charset"));
        if (key_and_value != chunk.attribute_map_->end()) {
            charset_ = key_and_value->second;
        } else if (((key_and_value = chunk.attribute_map_->find("http-equiv")) != chunk.attribute_map_->end())
                     and (StringUtil::ASCIIToLower(key_and_value->second) == "content-type")
                     and ((key_and_value = chunk.attribute_map_->find("content")) != chunk.attribute_map_->end()))
        {
            static RegexMatcher *matcher(nullptr);
            if (unlikely(matcher == nullptr)) {
                const std::string pattern("charset=([^ ;]+)");
                std::string err_msg;
                matcher = RegexMatcher::RegexMatcherFactory(pattern, &err_msg);
                if (unlikely(matcher == nullptr))
                    throw std::runtime_error("Failed to construct a RegexMatcher for \"" + pattern
                                             + "\" in TextExtractor::notify: " + err_msg);
            }

            if (matcher->matched(key_and_value->second))
                charset_ = (*matcher)[1];
        }
    }
}


} // unnamed namespace


namespace TextUtil {


const std::string EncodingConverter::CANONICAL_UTF8_NAME("utf8");


std::unique_ptr<EncodingConverter> EncodingConverter::Factory(const std::string &from_encoding, const std::string &to_encoding,
                                                              std::string * const error_message)
{
    if (CanonizeCharset(from_encoding) == CanonizeCharset(to_encoding))
        return std::unique_ptr<IdentityConverter>(new IdentityConverter());

    const iconv_t iconv_handle(::iconv_open(to_encoding.c_str(), from_encoding.c_str()));
    if (unlikely(iconv_handle == (iconv_t)-1)) {
        *error_message = "can't create an encoding converter for conversion from \"" + from_encoding + "\" to \"" + to_encoding
                         + "\"!";
        return std::unique_ptr<EncodingConverter>(nullptr);
    }

    error_message->clear();
    return std::unique_ptr<EncodingConverter>(new EncodingConverter(from_encoding, to_encoding, iconv_handle));
}


bool EncodingConverter::convert(const std::string &input, std::string * const output) {
    char *in_bytes(new char[input.length()]);
    const char *in_bytes_start(in_bytes);
    std::memcpy(reinterpret_cast<void *>(in_bytes), input.data(), input.length());
    static const size_t UTF8_SEQUENCE_MAXLEN(6);
    const size_t OUTBYTE_COUNT(UTF8_SEQUENCE_MAXLEN * input.length());
    char *out_bytes(new char[OUTBYTE_COUNT]);
    const char *out_bytes_start(out_bytes);

    size_t inbytes_left(input.length()), outbytes_left(OUTBYTE_COUNT);
    const ssize_t converted_count(
        static_cast<ssize_t>(::iconv(iconv_handle_, &in_bytes, &inbytes_left, &out_bytes, &outbytes_left)));
    if (unlikely((converted_count == -1) and (getToEncoding().find("//TRANSLIT") == std::string::npos) and
                 (getToEncoding().find("//IGNORE") == std::string::npos or errno == E2BIG)))
    {
        LOG_WARNING("iconv(3) failed! (Trying to convert \"" + from_encoding_ + "\" to \"" + to_encoding_ + "\"!");
        delete [] in_bytes_start;
        delete [] out_bytes_start;
        *output = input;
        return false;
    }

    output->assign(out_bytes_start, OUTBYTE_COUNT - outbytes_left);
    delete [] in_bytes_start;
    delete [] out_bytes_start;

    return true;
}


EncodingConverter::~EncodingConverter() {
    if (iconv_handle_ != (iconv_t)-1 and unlikely(::iconv_close(iconv_handle_) == -1))
        LOG_ERROR("iconv_close(3) failed!");
}


std::string ExtractTextFromHtml(const std::string &html, const std::string &initial_charset) {
    std::string extracted_text;
    TextExtractor extractor(html, initial_charset, &extracted_text);
    extractor.parse();
    return StringUtil::TrimWhite(extracted_text);
}


std::string ExtractTextFromUBTei(const std::string &tei) {
    std::string extracted_text;

    XMLParser parser(tei, XMLParser::XML_STRING);

    const std::string WORD_WRAP("¬");
    bool concat_with_whitespace(true);

    std::map<std::string, std::string> attrib_map;
    XMLParser::XMLPart part;
    while (parser.skipTo(XMLParser::XMLPart::OPENING_TAG, "span")) {
        if (parser.getNext(&part) and part.type_ == XMLParser::XMLPart::CHARACTERS) {
            if (concat_with_whitespace)
                extracted_text += ' ';

            if (StringUtil::EndsWith(part.data_, WORD_WRAP)) {
                StringUtil::RightTrim(WORD_WRAP, &part.data_);
                concat_with_whitespace = false;
            } else
                concat_with_whitespace = true;

            extracted_text += part.data_;
        }
    }

    return CollapseWhitespace(&extracted_text);
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


bool UTF8ToWCharString(const std::string &utf8_string, std::wstring * wchar_string) {
    wchar_string->clear();
    wchar_string->reserve(utf8_string.length());

    const char *cp(utf8_string.c_str());
    size_t remainder(utf8_string.size());
    std::mbstate_t state = std::mbstate_t();
    while (*cp != '\0') {
        wchar_t wch;
        const size_t retcode(std::mbrtowc(&wch, cp, remainder, &state));
        if (unlikely(retcode == static_cast<size_t>(-1) or retcode == static_cast<size_t>(-2)))
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
    iconv_t iconv_handle(::iconv_open("UTF-8", "WCHAR_T"));
    if (unlikely(iconv_handle == (iconv_t)-1))
        LOG_ERROR("iconv_open(3) failed!");

    const size_t INBYTE_COUNT(wchar_string.length() * sizeof(wchar_t));
    char *in_bytes(new char[INBYTE_COUNT]);
    const char *in_bytes_start(in_bytes);
    ::memcpy(reinterpret_cast<void *>(in_bytes), wchar_string.data(), INBYTE_COUNT);
    static const size_t UTF8_SEQUENCE_MAXLEN(6);
    const size_t OUTBYTE_COUNT(UTF8_SEQUENCE_MAXLEN * wchar_string.length());
    char *out_bytes(new char[OUTBYTE_COUNT]);
    const char *out_bytes_start(out_bytes);

    size_t inbytes_left(INBYTE_COUNT), outbytes_left(OUTBYTE_COUNT);
    const ssize_t converted_count(static_cast<ssize_t>(::iconv(iconv_handle, &in_bytes, &inbytes_left, &out_bytes, &outbytes_left)));

    delete [] in_bytes_start;
    if (unlikely(converted_count == -1)) {
        LOG_WARNING("iconv(3) failed!");
        delete [] out_bytes_start;
        return false;
    }

    utf8_string->assign(out_bytes_start, OUTBYTE_COUNT - outbytes_left);
    delete [] out_bytes_start;
    ::iconv_close(iconv_handle);

    return true;
}


std::string WCharToUTF8StringOrDie(const std::wstring &wchar_string) {
    std::string utf8_string;
    if (unlikely(not WCharToUTF8String(wchar_string, &utf8_string)))
        LOG_ERROR("failed to cpnvert a wide character string to an UTF8 string!");
    return utf8_string;
}


bool WCharToUTF8String(const wchar_t wchar, std::string * utf8_string) {
    const std::wstring wchar_string(1, wchar);
    return WCharToUTF8String(wchar_string, utf8_string);
}


static std::locale DEFAULT_LOCALE("");


bool UTF8ToLower(const std::string &utf8_string, std::string * const lowercase_utf8_string) {
    std::wstring wchar_string;
    if (not UTF8ToWCharString(utf8_string, &wchar_string))
        return false;

    // Lowercase the wide character string:
    std::wstring lowercase_wide_string;
    for (const auto wide_ch : wchar_string) {
        if (std::iswupper(wide_ch))
            lowercase_wide_string += std::tolower(wide_ch, DEFAULT_LOCALE);
        else
            lowercase_wide_string += wide_ch;
    }

    return WCharToUTF8String(lowercase_wide_string, lowercase_utf8_string);
}


std::string UTF8ToLower(std::string * const utf8_string) {
    std::string converted_string;
    if (unlikely(not UTF8ToLower(*utf8_string, &converted_string)))
        throw std::runtime_error("in TextUtil::UTF8ToLower: failed to convert a string \"" + CStyleEscape(*utf8_string)
                                 + "\"to lowercase!");

    utf8_string->swap(converted_string);
    return *utf8_string;
}


bool UTF8ToUpper(const std::string &utf8_string, std::string * const uppercase_utf8_string) {
    std::wstring wchar_string;
    if (not UTF8ToWCharString(utf8_string, &wchar_string))
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


std::string UTF8ToUpper(std::string * const utf8_string) {
    std::string converted_string;
    if (unlikely(not UTF8ToUpper(*utf8_string, &converted_string)))
        throw std::runtime_error("in TextUtil::UTF8ToUpper: failed to convert a string to lowercase!");

    utf8_string->swap(converted_string);
    return *utf8_string;
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


uint32_t UTF32ToLower(const uint32_t code_point) {
    return static_cast<uint32_t>(std::tolower(static_cast<wchar_t>(code_point), DEFAULT_LOCALE));
}


std::wstring &ToLower(std::wstring * const s) {
    for (auto &ch : *s)
        ch = std::tolower(ch);
    return *s;
}



uint32_t UTF32ToUpper(const uint32_t code_point) {
    return static_cast<uint32_t>(std::toupper(static_cast<wchar_t>(code_point), DEFAULT_LOCALE));
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


template<typename ContainerType> bool ChopIntoWords(const std::string &text, ContainerType * const words, const unsigned min_word_length) {
    words->clear();

    std::wstring wide_text;
    if (unlikely(not UTF8ToWCharString(text, &wide_text)))
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


bool ChopIntoWords(const std::string &text, std::unordered_set<std::string> * const words, const unsigned min_word_length) {
    return ChopIntoWords<std::unordered_set<std::string>> (text, words, min_word_length);
}


bool ChopIntoWords(const std::string &text, std::vector<std::string> * const words, const unsigned min_word_length) {
    return ChopIntoWords<std::vector<std::string>> (text, words, min_word_length);
}


// See https://en.wikipedia.org/wiki/UTF-8 in order to understand the implementation.
bool IsValidUTF8(const std::string &utf8_candidate) {
    for (std::string::const_iterator ch(utf8_candidate.begin()); ch != utf8_candidate.end(); ++ch) {
        const unsigned char uch(static_cast<unsigned char>(*ch));
        unsigned sequence_length;
        if ((uch & 0b10000000) == 0b00000000)
            sequence_length = 0;
        else if ((uch & 0b11100000) == 0b11000000)
            sequence_length = 1;
        else if ((uch & 0b11110000) == 0b11100000)
            sequence_length = 2;
        else if ((uch & 0b11111000) == 0b11110000)
            sequence_length = 3;
        else {
            LOG_DEBUG("bad sequence start character: 0x" + StringUtil::ToHexString(uch));
            return false;
        }

        for (unsigned i(0); i < sequence_length; ++i) {
            ++ch;
            if (unlikely(ch == utf8_candidate.end())) {
                LOG_DEBUG("premature string end in the middle of a UTF8 byte sequence!");
                return false;
            }
            if (unlikely((static_cast<unsigned char>(*ch) & 0b11000000) != 0b10000000)) {
                LOG_DEBUG("unexpected upper-bit pattern in a UFT8 sequence: 0x" + StringUtil::ToHexString(static_cast<uint8_t>(*ch)));
                return false;
            }
        }
    }

    return true;
}


std::vector<std::string>::const_iterator FindSubstring(const std::vector<std::string> &haystack, const std::vector<std::string> &needle) {
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


std::string Base64Encode(const std::string &s, const char symbol63, const char symbol64, const bool use_output_padding) {
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

    // Remove padding at the end.
    auto end(s.cend());
    while (end != s.cbegin() and *(end - 1) == '=')
        --end;

    for (auto encoded_ch(s.cbegin()); encoded_ch != end; ++encoded_ch) {
        const unsigned char ch(char_to_bits_map[static_cast<unsigned char>(*encoded_ch)]);
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


std::string EscapeString(const std::string &original_string, const bool also_escape_whitespace) {
    std::string escaped_string;
    escaped_string.reserve(original_string.size() * 2);

    for (char ch : original_string) {
        if (std::iscntrl(ch) or (not also_escape_whitespace or IsWhiteSpace(ch)))
            escaped_string += StringUtil::ToString(static_cast<unsigned char>(ch), /* radix */8, /* width */3, /* padding_char */'0');
        else
            escaped_string += ch;
    }

    return escaped_string;
}


std::string CSVEscape(const std::string &value, const bool add_quotes) {
    std::string escaped_value;
    escaped_value.reserve(value.length() + 2 /* for the quotes */);

    if (add_quotes)
        escaped_value += '"';

    for (const char ch : value) {
        if (not add_quotes and unlikely(ch == ','))
            escaped_value += "\",\"";
        else {
            if (unlikely(ch == '"'))
                escaped_value += '"';
            escaped_value += ch;
        }
    }

    if (add_quotes)
        escaped_value += '"';

    return escaped_value;
}


namespace {


enum CSVTokenType { VALUE, SEPARATOR, LINE_END, END_OF_INPUT, SYNTAX_ERROR };


class CSVTokenizer {
    File * const input_;
    const char separator_;
    const char quote_;
    std::string value_;
    unsigned line_no_;
    std::string err_msg_;
public:
    CSVTokenizer(File * const input, const char separator, const char quote)
        : input_(input), separator_(separator), quote_(quote), line_no_(1) { }
    CSVTokenType getToken();
    inline const std::string &getValue() const { return value_; }
    inline unsigned getLineNo() const { return line_no_; }
    inline const std::string &getErrMsg() const { return err_msg_; }
};


CSVTokenType CSVTokenizer::getToken() {
    int ch(input_->get());
    if (unlikely(ch == EOF))
        return END_OF_INPUT;
    if (ch == separator_)
        return SEPARATOR;
    if (ch == '\n') {
        ++line_no_;
        return LINE_END;
    }
    if (ch == '\r') {
        ch = input_->get();
        if (unlikely(ch != '\n')) {
            err_msg_ = "unexpected carriage-return!";
            return SYNTAX_ERROR;
        }
        ++line_no_;
        return LINE_END;
    }

    value_.clear();
    if (ch == quote_) {
        const unsigned start_line_no(line_no_);
        for (;;) {
            ch = input_->get();
            if (ch == quote_) {
                if (likely(input_->peek() != quote_))
                    break;
                input_->get();
                value_ += quote_;
            }
            if (unlikely(ch == EOF)) {
                err_msg_ = "quoted value starting on line #" + std::to_string(start_line_no) + " was never terminated!";
                return SYNTAX_ERROR;
            }
            value_ += static_cast<char>(ch);
        }
    } else { // Unquoted value.
        value_ += static_cast<char>(ch);
        for (;;) {
            ch = input_->get();
            if (ch == EOF or ch == '\r' or ch == '\n' or ch == separator_) {
                if (likely(ch != EOF))
                    input_->putback(ch);
                break;
            }
            if (unlikely(ch == quote_)) {
                err_msg_ = "unexpected quote in value!";
                return SYNTAX_ERROR;
            }
            value_ += static_cast<char>(ch);
        }
    }
    return VALUE;
}


} // unnamed namespace


void ParseCSVFileOrDie(const std::string &path, std::vector<std::vector<std::string>> * const lines, const char separator,
                       const char quote)
{
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(path));
    CSVTokenizer scanner(input.get(), separator, quote);
    std::vector<std::string> current_line;
    CSVTokenType last_token(LINE_END); // Can be anything except for SEPARATOR;
    for (;;) {
        const CSVTokenType token(scanner.getToken());
        if (unlikely(token == SYNTAX_ERROR))
            LOG_ERROR("on line #" + std::to_string(scanner.getLineNo() - 1) + " in \"" + input->getPath() + "\": "
                  + scanner.getErrMsg());
        else if (token == LINE_END) {
            if (unlikely(last_token == SEPARATOR))
                LOG_ERROR("line #" + std::to_string(scanner.getLineNo() - 1) + " in \"" + input->getPath()
                      + "\" ending in separator!");
            lines->emplace_back(current_line);
            current_line.clear();
            last_token = SEPARATOR;
            continue;
        } else if (unlikely(token == END_OF_INPUT)) {
            return;
        } else if (token == VALUE) {
            current_line.emplace_back(scanner.getValue());
        } else if (token == SEPARATOR) {
            if (last_token == SEPARATOR)
                current_line.emplace_back("");
        }
        last_token = token;
    }

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


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const std::string ToUTF32Decoder::CANONICAL_UTF32_NAME("UTF32LE");
#else
    const std::string ToUTF32Decoder::CANONICAL_UTF32_NAME("UTF32BE");
#endif
const uint32_t ToUTF32Decoder::NULL_CHARACTER(0);
const size_t MAX_UTF32_CODEPOINT_LENGTH(4);


AnythingToUTF32Decoder::AnythingToUTF32Decoder(const std::string &input_encoding, const bool permissive)
    : converter_handle_(nullptr), input_encoding_(input_encoding), utf32_char_(0), accum_buffer_(), current_state_(NO_CHARACTER_PENDING), permissive_(permissive)
{
    converter_handle_ = ::iconv_open(CANONICAL_UTF32_NAME.c_str(), input_encoding_.c_str());
    if (converter_handle_ == reinterpret_cast<iconv_t>(-1)) {
        LOG_ERROR("Couldn't init iconv for the following conversion: " +
                  input_encoding_ + " -> " + CANONICAL_UTF32_NAME + ". Errno: " + std::to_string(errno));
    }

    accum_buffer_.reserve(50);
}


AnythingToUTF32Decoder::~AnythingToUTF32Decoder() {
    if (converter_handle_ != reinterpret_cast<iconv_t>(-1)) {
        if (::iconv_close(converter_handle_) == -1) {
            LOG_ERROR("Couldn't deinit iconv for the following conversion: " +
                      input_encoding_ + " -> " + CANONICAL_UTF32_NAME + ". Errno: " + std::to_string(errno));
        }
    }
}


bool AnythingToUTF32Decoder::addByte(const char ch) {
    if (converter_handle_ == reinterpret_cast<iconv_t>(-1))
        throw std::runtime_error("Converter was not initialized correctly.");

    if (current_state_ == CHARACTER_PENDING) {
        if (not permissive_) {
            throw std::runtime_error("AnythingToUTF32Decoder::addByte: Pending character "
                                     + std::to_string(utf32_char_) + " hast not been consumed.");
        }
        else
            consumeAndReset();
    }

    // accumulate input and convert until we have a valid codepoint
    // kinda hacky, thanks to the brain-dead iconv API
    accum_buffer_.emplace_back(ch);
    size_t in_len(accum_buffer_.size()), out_len(MAX_UTF32_CODEPOINT_LENGTH * (in_len + 1));    // extra codepoint for the BOM
    std::unique_ptr<char[]> in_buf(new char[in_len]), out_buf(new char[out_len]);
    std::memcpy(in_buf.get(), accum_buffer_.data(), accum_buffer_.size());
    auto in_buf_start(in_buf.get()), out_buf_start(out_buf.get());

    size_t converted_count(::iconv(converter_handle_, &in_buf_start, &in_len, &out_buf_start, &out_len));
    if (converted_count == static_cast<size_t>(-1)) {
        switch (errno) {
        case E2BIG:
            // shouldn't happen
            LOG_ERROR("Couldn't perform for the following encoding conversion: " +
                      input_encoding_ + " -> " + CANONICAL_UTF32_NAME + " as the output buffer was too small");
            break;
        case EILSEQ:
            // invalid multi-byte sequence
            if (not permissive_) {
                throw std::runtime_error("AnythingToUTF32Decoder::addByte: Invalid multi-byte sequence. Current byte: "
                                         + std::to_string(static_cast<unsigned>(ch)) + ", consumed bytes: "
                                         + std::to_string(accum_buffer_.size()));
            } else {
                // return the replacement character
                utf32_char_ = REPLACEMENT_CHARACTER;
                current_state_ = CHARACTER_PENDING;
            }
            break;
        case EINVAL:
            // incomplete sequence, keep accumulating
            current_state_ = CHARACTER_INCOMPLETE;
            break;
        default:
            LOG_ERROR("Unknown iconv error.");
        }
    } else {
        // sequence decoded, convert the bytes
        utf32_char_ = *reinterpret_cast<uint32_t *>(out_buf.get());
        current_state_ = CHARACTER_PENDING;
    }

    return current_state_ != CHARACTER_PENDING;
}


ToUTF32Decoder::State AnythingToUTF32Decoder::getState() const {
    return current_state_;
}


uint32_t AnythingToUTF32Decoder::getUTF32Char() {
    auto out(consumeAndReset());
    if (out == NULL_CHARACTER and not permissive_)
        throw std::runtime_error("AnythingToUTF32Decoder::addByte: Attempting to consume a non-existent codepoint");

    return out;
}


const std::string &AnythingToUTF32Decoder::getInputEncoding() const {
    return input_encoding_;
}


uint32_t AnythingToUTF32Decoder::consumeAndReset() {
    auto temp(utf32_char_);
    utf32_char_ = NULL_CHARACTER;
    accum_buffer_.clear();
    current_state_ = NO_CHARACTER_PENDING;
    return temp;
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
        } else if (permissive_) {
            utf32_char_ = REPLACEMENT_CHARACTER;
            required_count_ = 0;
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


// See https://en.wikipedia.org/wiki/Whitespace_character for the original list.
const std::unordered_set<uint32_t> UNICODE_WHITESPACE {
    0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x0085, 0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005,
    0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000, 0x180E, 0x200B, 0x200C, 0x200D, 0x2060,
    0xFEFF
};



static std::string &CollapseWhitespaceHelper(std::string * const utf8_string,
                                             const bool last_char_was_whitespace_initial_state)
{
    std::string collapsed_string;

    UTF8ToUTF32Decoder utf8_to_utf32_decoder;
    bool last_char_was_whitespace(last_char_was_whitespace_initial_state);
    for (const auto ch : *utf8_string) {
        if (IsASCIIChar(ch)) {
            if (isspace(ch)) {
                if (not last_char_was_whitespace) {
                    last_char_was_whitespace = true;
                    collapsed_string += ' ';
                }
            } else {
                last_char_was_whitespace = false;
                collapsed_string += ch;
            }
        } else {
            if (not utf8_to_utf32_decoder.addByte(ch)) {
                const uint32_t utf32_char(utf8_to_utf32_decoder.getUTF32Char());
                if (IsWhitespace(utf32_char)) {
                    if (not last_char_was_whitespace) {
                        last_char_was_whitespace = true;
                        collapsed_string += ' ';
                    }
                } else {
                    std::string utf8;
                    if (unlikely(not WCharToUTF8String(utf32_char, &utf8))) {
                        LOG_WARNING("WCharToUTF8String failed! (Character was " + std::to_string(utf32_char) + ")");
                        return *utf8_string;
                    }
                    collapsed_string += utf8;
                    last_char_was_whitespace = false;
                }
            }
        }
    }

    if (unlikely(utf8_to_utf32_decoder.getState() == UTF8ToUTF32Decoder::CHARACTER_INCOMPLETE)) {
        LOG_WARNING("UTF8 input sequence contains an incomplete character!");
        return *utf8_string;
    }

    utf8_string->swap(collapsed_string);
    return *utf8_string;
}


std::string &CollapseWhitespace(std::string * const utf8_string) {
    return CollapseWhitespaceHelper(utf8_string, /* last_char_was_whitespace_initial_state = */ false);
}


std::string &CollapseAndTrimWhitespace(std::string * const utf8_string) {
    CollapseWhitespaceHelper(utf8_string, /* last_char_was_whitespace_initial_state = */ true);

    // String ends with a space? => Remove it!
    if (not utf8_string->empty() and utf8_string->back() == ' ')
        utf8_string->resize(utf8_string->size() - 1);

    return *utf8_string;
}


bool FromHex(const char ch, unsigned * const u) {
    if (ch >= '0' and ch <= '9') {
        *u = ch - '0';
        return true;
    }

    switch (ch) {
    case 'A':
    case 'a':
        *u = 10;
        return true;
    case 'B':
    case 'b':
        *u = 11;
        return true;
    case 'C':
    case 'c':
        *u = 12;
        return true;
    case 'D':
    case 'd':
        *u = 13;
        return true;
    case 'E':
    case 'e':
        *u = 14;
        return true;
    case 'F':
    case 'f':
        *u = 15;
        return true;
    }

    return false;
}


// Helper function for CStyleUnescape().
static std::string DecodeUnicodeEscapeSequence(std::string::const_iterator &ch, const std::string::const_iterator &end,
                                               const unsigned width)
{
    wchar_t wchar(0);
    for (unsigned i(0); i < width; ++i) {
        ++ch;
        if (unlikely(ch == end))
            throw std::runtime_error("in TextUtil::DecodeUnicodeEscapeSequence: short Unicode escape!");
        wchar <<= 4u;
        unsigned nybble;
        if (unlikely(not FromHex(*ch, &nybble)))
            throw std::runtime_error("in TextUtil::DecodeUnicodeEscapeSequence: invalid Unicode escape! (Not a valid nibble '"
                                     + std::string(1, *ch) + "'.)");
        wchar |= nybble;
    }
    std::string utf8_sequence;
    if (unlikely(not WCharToUTF8String(wchar, &utf8_sequence)))
        throw std::runtime_error("in TextUtil::DecodeUnicodeEscapeSequence: invalid Unicode escape \\u"
                                 + StringUtil::ToString(wchar, 16) + "!");

    return utf8_sequence;
}


// Helper function for CStyleUnescape().
static char DecodeHexadecimalEscapeSequence(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    ++ch;
    if (unlikely(ch == end))
        throw std::runtime_error("in TextUtil::DecodeHexadecimalEscapeSequence: missing first hex nibble at end of string!");
    std::string hex_string(1, *ch++);
    if (unlikely(ch == end))
        throw std::runtime_error("in TextUtil::DecodeHexadecimalEscapeSequence: missing second hex nibble at end of string!");
    hex_string += *ch;
    unsigned byte;
    if (unlikely(not StringUtil::ToNumber(hex_string, &byte, 16)))
        throw std::runtime_error("in TextUtil::DecodeHexadecimalEscapeSequence: bad hex escape \"\\x" + hex_string + "\"!");

    return static_cast<char>(byte);
}


// Helper function for CStyleUnescape().
static char DecodeOctalEscapeSequence(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    unsigned code(0), count(0);
    while (count < 3 and ch != end and (*ch >= '0' and *ch <= '7')) {
        code <<= 3u;
        code += *ch - '0';
        ++ch;
        ++count;
    }
    --ch;

    return static_cast<char>(code);
}


std::string &CStyleUnescape(std::string * const s) {
    std::string unescaped_string;
    bool backslash_seen(false);
    for (auto ch(s->cbegin()); ch != s->cend(); ++ch) {
        if (not backslash_seen) {
            if (*ch == '\\')
                backslash_seen = true;
            else
                unescaped_string += *ch;
        } else {
            switch (*ch) {
            case 'n':
                unescaped_string += '\n';
                break;
            case 't':
                unescaped_string += '\t';
                break;
            case 'b':
                unescaped_string += '\b';
                break;
            case 'r':
                unescaped_string += '\r';
                break;
            case 'f':
                unescaped_string += '\f';
                break;
            case 'v':
                unescaped_string += '\v';
                break;
            case 'a':
                unescaped_string += '\a';
                break;
            case '\\':
                unescaped_string += '\\';
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                unescaped_string += DecodeOctalEscapeSequence(ch, s->cend());
                break;
            case 'x':
                unescaped_string += DecodeHexadecimalEscapeSequence(ch, s->cend());
                break;
            case 'u':
                unescaped_string += DecodeUnicodeEscapeSequence(ch, s->cend(), /* width = */ 4);
                break;
            case 'U':
                unescaped_string += DecodeUnicodeEscapeSequence(ch, s->cend(), /* width = */ 8);
                break;
            default:
                throw std::runtime_error("unknown escape sequence: backslash followed by '" + std::string(1, *ch) + "'!");
            }
            backslash_seen = false;
        }
    }

    if (unlikely(backslash_seen))
        throw std::runtime_error("in TextUtil::CStyleUnescape: trailing slash in input string!");

    s->swap(unescaped_string);
    return *s;
}


static std::string To4ByteHexString(uint16_t u16) {
    std::string as_string;
    for (unsigned i(0); i < 4; ++i) {
        as_string += StringUtil::ToHex(u16 & 0xF);
        u16 >>= 4u;
    }
    std::reverse(as_string.begin(), as_string.end());

    return as_string;
}


static std::string ToUnicodeEscape(const uint32_t code_point) {
    if (code_point <= 0xFFFFu)
        return "\\u" + To4ByteHexString(static_cast<uint16_t>(code_point));
    return "\\U" + To4ByteHexString(code_point >> 16u) + To4ByteHexString(static_cast<uint16_t>(code_point));
}


std::string &CStyleEscape(std::string * const s) {
    std::string escaped_string;

    UTF8ToUTF32Decoder utf8_to_utf32_decoder;
    for (auto ch(s->cbegin()); ch != s->cend(); ++ch) {
        if (IsASCIIChar(*ch)) {
            switch (*ch) {
            case '\n':
                escaped_string += "\\n";
                break;
            case '\t':
                escaped_string += "\\t";
                break;
            case '\b':
                escaped_string += "\\b";
                break;
            case '\r':
                escaped_string += "\\r";
                break;
            case '\f':
                escaped_string += "\\f";
                break;
            case '\v':
                escaped_string += "\\v";
                break;
            case '\a':
                escaped_string += "\\a";
                break;
            case '\\':
                escaped_string += "\\\\";
                break;
            case '"':
                escaped_string += "\\\"";
                break;
            default: {
                if (std::isprint(*ch))
                    escaped_string += *ch;
                else
                    escaped_string += "\\"
                                      + StringUtil::ToString(static_cast<unsigned char>(*ch), /* radix */8, /* width */3,
                                                             /* padding_char */'0');
            }
            }
        } else { // We found the first byte of a UTF8 byte sequence.
            for (;;) {
                if (unlikely(ch == s->cend()))
                    throw std::runtime_error("in TextUtil::CStyleEscape: invalid UTF8 sequence found!");
                if (not utf8_to_utf32_decoder.addByte(*ch)) {
                    escaped_string += ToUnicodeEscape(utf8_to_utf32_decoder.getUTF32Char());
                    break;
                }
                ++ch;
            }
        }
    }

    s->swap(escaped_string);
    return *s;
}


std::string InitialCaps(const std::string &text) {
    if (text.empty())
        return "";

    std::wstring wchar_string;
    if (unlikely(not UTF8ToWCharString(text, &wchar_string)))
        LOG_ERROR("can't convert a supposed UTF-8 string to a wide string!");

    auto wide_ch(wchar_string.begin());
    if (std::iswlower(*wide_ch))
        *wide_ch = std::towupper(*wide_ch);
    for (++wide_ch; wide_ch != wchar_string.end(); ++wide_ch) {
        if (std::iswupper(*wide_ch))
            *wide_ch = std::towlower(*wide_ch);
    }

    std::string utf8_string;
    if (unlikely(not WCharToUTF8String(wchar_string, &utf8_string)))
        LOG_ERROR("can't convert a supposed wide string to a UTF-8 string!");

    return utf8_string;
}


std::string ToTitleCase(const std::string &text) {
    if (text.empty())
        return "";

    std::wstring wchar_string;
    if (unlikely(not UTF8ToWCharString(text, &wchar_string)))
        LOG_ERROR("can't convert a supposed UTF-8 string to a wide string!");

    bool force_next_char_to_uppercase(true);
    for (auto &wide_ch : wchar_string) {
        if (TextUtil::IsWhitespace(wide_ch)) {
            force_next_char_to_uppercase = true;
            continue;
        }

        if (force_next_char_to_uppercase)
            wide_ch = std::towupper(wide_ch);
        else
            wide_ch = std::towlower(wide_ch);

        force_next_char_to_uppercase = false;
    }

    std::string utf8_string;
    if (unlikely(not WCharToUTF8String(wchar_string, &utf8_string)))
        LOG_ERROR("can't convert a supposed wide string to a UTF-8 string!");

    return utf8_string;
}


std::string CanonizeCharset(std::string charset) {
    StringUtil::ASCIIToLower(&charset);
    StringUtil::RemoveChars("- ", &charset);
    return charset;
}


std::string &UTF8Truncate(std::string * const utf8_string, const size_t max_length) {
    size_t codepoint_count(0);
    auto cp(utf8_string->cbegin());
    while (codepoint_count < max_length and cp != utf8_string->cend()) {
        ++codepoint_count;
        cp = GetEndOfCurrentUTF8CodePoint(cp, utf8_string->cend());
    }
    utf8_string->resize(cp - utf8_string->cbegin());

    return *utf8_string;
}


std::string &UTF8ByteTruncate(std::string * const utf8_string, const size_t max_length) {
    size_t byte_count(0);
    auto cp(utf8_string->cbegin());
    while (cp != utf8_string->cend()) {
        auto cp2(GetEndOfCurrentUTF8CodePoint(cp, utf8_string->cend()));
        const size_t codepoint_length(cp2 - cp);
        if (byte_count + codepoint_length > max_length)
            break;
        byte_count += codepoint_length;
        cp = cp2;
    }
    utf8_string->resize(cp - utf8_string->cbegin());

    return *utf8_string;
}


// See https://www.compart.com/en/unicode/category/Zs for where we got this.
bool IsSpaceSeparatorCharacter(const wchar_t ch) {
    return ch == 0x0020 or ch == 0x00A0 or ch == 0x1680 or ch == 0x2000 or ch == 0x2001 or ch == 0x2002 or ch == 0x2003 or ch == 0x2004
           or ch == 0x2005 or ch == 0x2006 or ch == 0x2007 or ch == 0x2008 or ch == 0x2009 or ch == 0x200A or ch == 0x202F or ch == 0x205F
           or ch == 0x3000;
}


bool IsSpace(const wchar_t ch) {
    return IsSpaceSeparatorCharacter(ch) or ch == '\f' or ch == '\n' or ch == '\r' or ch == '\t' or ch == '\v';
}


double CalcTextSimilarity(const std::string &text1, const std::string &text2, const bool ignore_case) {
    std::wstring wtext1;
    if (unlikely(UTF8ToWCharString(text1, &wtext1)))
        LOG_ERROR("failed to convert \"text1\" to a wstring!");

    std::wstring wtext2;
    if (unlikely(UTF8ToWCharString(text2, &wtext2)))
        LOG_ERROR("failed to convert \"text2\" to a wstring!");

    if (ignore_case) {
        ToLower(&wtext1);
        ToLower(&wtext2);
    }

    return MiscUtil::LevenshteinDistance(wtext1, wtext2) / std::max(wtext1.length(), wtext2.length());
}


bool IsSomeKindOfDash(const uint32_t ch) {
    return ch == '-' /*ordinary minus */ or ch == EN_DASH or ch == EM_DASH or ch == TWO_EM_DASH or ch == THREE_EM_DASH
           or ch == SMALL_EM_DASH or ch == NON_BREAKING_HYPHEN;

}


std::string &NormaliseDashes(std::string * const s) {
    std::vector<uint32_t> utf32_string;
    if (unlikely(not UTF8ToUTF32(*s, &utf32_string)))
        LOG_ERROR("can't convert from UTF-8 to UTF-32!");

    for (auto &utf32_char :  utf32_string) {
        if (IsSomeKindOfDash(utf32_char))
            utf32_char = '-'; // ASCII minus sign.
    }

    s->clear();
    for (const auto utf32_char :  utf32_string)
        s->append(UTF32ToUTF8(utf32_char));

    return *s;
}


static const std::map<wchar_t, std::pair<wchar_t, wchar_t>> ligature_to_expansion_map{
    {L'æ', { 'a', 'e' }},
    {L'Æ', { 'A', 'E' }},
    {L'œ', { 'o', 'e' }},
    {L'Œ', { 'O', 'E' }},
};


std::wstring ExpandLigatures(const std::wstring &string) {
    std::wstring wstring_with_expanded_ligatures;
    for (const auto wchar : string) {
        const auto ligature_and_expansion(ligature_to_expansion_map.find(wchar));
        if (likely(ligature_and_expansion == ligature_to_expansion_map.cend()))
            wstring_with_expanded_ligatures += wchar;
        else {
            wstring_with_expanded_ligatures += ligature_and_expansion->second.first;
            wstring_with_expanded_ligatures += ligature_and_expansion->second.second;
        }
    }

    return wstring_with_expanded_ligatures;
}


std::string ExpandLigatures(const std::string &utf8_string) {
    std::wstring wstring;
    if (unlikely(not UTF8ToWCharString(utf8_string, &wstring)))
        LOG_ERROR("failed to convert a UTF8 string to a wide character string!");

    const auto wstring_with_expanded_ligatures(ExpandLigatures(wstring));
    std::string utf8_with_expanded_ligatures;
    if (unlikely(not WCharToUTF8String(wstring_with_expanded_ligatures, &utf8_with_expanded_ligatures)))
        LOG_ERROR("failed to convert a wide character string to a UTF8 string!");

    return utf8_with_expanded_ligatures;
}


// Please note that this list is incomplete!
static const std::map<wchar_t, wchar_t> char_with_diacritics_to_char_without_diarcritics_map{
    { L'ẚ', L'a' },
    { L'À', L'A' },
    { L'à', L'a' },
    { L'Á', L'A' },
    { L'á', L'a' },
    { L'Â', L'A' },
    { L'â', L'a' },
    { L'Ầ', L'A' },
    { L'ầ', L'a' },
    { L'Ấ', L'A' },
    { L'ấ', L'a' },
    { L'Ẫ', L'A' },
    { L'ẫ', L'a' },
    { L'Ấ', L'A' },
    { L'ấ', L'a' },
    { L'Ẫ', L'A' },
    { L'ẫ', L'a' },
    { L'Ẩ', L'A' },
    { L'ẩ', L'a' },
    { L'Ã', L'A' },
    { L'ã', L'a' },
    { L'ā', L'a' },
    { L'Ä', L'A' },
    { L'ä', L'a' },
    { L'Å', L'A' },
    { L'À', L'A' },
    { L'Á', L'A' },
    { L'Â', L'A' },
    { L'Ã', L'A' },
    { L'Ç', L'C' },
    { L'È', L'E' },
    { L'É', L'E' },
    { L'Ê', L'E' },
    { L'Ë', L'E' },
    { L'Ì', L'I' },
    { L'Í', L'I' },
    { L'Î', L'I' },
    { L'Ï', L'I' },
    { L'Ñ', L'N' },
    { L'Ò', L'O' },
    { L'Ó', L'O' },
    { L'Ô', L'O' },
    { L'Õ', L'O' },
    { L'Ö', L'O' },
    { L'Ø', L'O' },
    { L'Ù', L'U' },
    { L'Ú', L'U' },
    { L'Û', L'U' },
    { L'Ü', L'U' },
    { L'Ý', L'Y' },
    { L'à', L'a' },
    { L'á', L'a' },
    { L'â', L'a' },
    { L'å', L'a' },
    { L'ç', L'c' },
    { L'è', L'e' },
    { L'é', L'e' },
    { L'ê', L'e' },
    { L'ë', L'e' },
    { L'ì', L'i' },
    { L'í', L'i' },
    { L'î', L'i' },
    { L'ï', L'i' },
    { L'ñ', L'n' },
    { L'ò', L'o' },
    { L'ó', L'o' },
    { L'ô', L'o' },
    { L'õ', L'o' },
    { L'ö', L'o' },
    { L'ø', L'o' },
    { L'ù', L'u' },
    { L'ú', L'u' },
    { L'ú', L'u' },
    { L'û', L'u' },
    { L'ü', L'u' },
    { L'ý', L'y' },
    { L'ÿ', L'y' },
};


std::wstring RemoveDiacritics(const std::wstring &string) {
    std::wstring wstring_without_diacritics;
    for (const auto wchar : string) {
        const auto char_with_and_without_diacritics(char_with_diacritics_to_char_without_diarcritics_map.find(wchar));
        if (likely(char_with_and_without_diacritics == char_with_diacritics_to_char_without_diarcritics_map.cend()))
            wstring_without_diacritics += wchar;
        else
            wstring_without_diacritics += char_with_and_without_diacritics->second;
    }

    return wstring_without_diacritics;
}


std::string RemoveDiacritics(const std::string &utf8_string) {
    std::wstring wstring;
    if (unlikely(not UTF8ToWCharString(utf8_string, &wstring)))
        LOG_ERROR("failed to convert a UTF8 string to a wide character string!");

    const auto wstring_without_diacritics(RemoveDiacritics(wstring));
    std::string utf8_without_diacritics;
    if (unlikely(not WCharToUTF8String(wstring_without_diacritics, &utf8_without_diacritics)))
        LOG_ERROR("failed to convert a wide character string to a UTF8 string!");

    return utf8_without_diacritics;
}


static const std::vector<wchar_t> quotation_marks_to_normalise {
L'«',  L'‹',  L'»',  L'›',  L'„',  L'‚',  L'“',  L'‟',  L'‘',  L'‛',  L'”',  L'’',  L'"',  L'❛',  L'❜',  L'❟',  L'❝',  L'❞',  L'❮',  L'❯',  L'⹂',  L'〝',  L'〞',  L'〟',  L'＂'
};


std::wstring NormaliseQuotationMarks(const std::wstring &string) {
   std::wstring string_with_normalised_quotes;
   for (const auto wchar : string) {
       if (likely(std::find(quotation_marks_to_normalise.cbegin(), quotation_marks_to_normalise.cend(), wchar) ==
                  quotation_marks_to_normalise.cend()))
           string_with_normalised_quotes += wchar;
       else
           string_with_normalised_quotes += '"';
   }

   return string_with_normalised_quotes;
}


std::string NormaliseQuotationMarks(const std::string &utf8_string) {
    std::wstring wstring;
    if (unlikely(not UTF8ToWCharString(utf8_string, &wstring)))
        LOG_ERROR("failed to convert a UTF8 string to a wide character string!");

    const auto wstring_normalised_quotations_marks(NormaliseQuotationMarks(wstring));
    std::string utf8_normalised_quotations_marks;
    if (unlikely(not WCharToUTF8String(wstring_normalised_quotations_marks, &utf8_normalised_quotations_marks)))
        LOG_ERROR("failed to convert a wide character string to a UTF8 string!");

    return utf8_normalised_quotations_marks;
}


bool ConvertToUTF8(const std::string &encoding, const std::string &text, std::string * const utf8_text) {
    utf8_text->clear();

    std::string error_message;
    const auto to_utf8_converter(EncodingConverter::Factory(encoding, "UTF-8", &error_message));
    if (to_utf8_converter.get() == nullptr)
        return false;

    return to_utf8_converter->convert(text, utf8_text);
}


bool ConsistsEntirelyOfLetters(const std::string &utf8_string) {
    std::wstring wstring;
    if (unlikely(not UTF8ToWCharString(utf8_string, &wstring)))
        LOG_ERROR("invalid UTF-8 input!");

    for (const auto wch : wstring) {
        if (not std::iswalpha(wch))
            return false;
    }

    return true;
}


size_t CodePointCount(const std::string &utf8_string) {
    size_t code_point_count(0);
    for (const char ch : utf8_string) {
        if (IsStartOfUTF8CodePoint(ch))
            ++code_point_count;
    }

    return code_point_count;
}


static std::string ExtractUTF8Substring(const std::string::const_iterator start, const std::string::const_iterator end,
                                        const size_t max_length)
{
    std::string substring;
    size_t substring_length(0);
    for (auto ch(start); ch != end; ++ch) {
        if (IsStartOfUTF8CodePoint(*ch)) {
            if (substring_length == max_length)
                break;
            ++substring_length;
        }
        substring += *ch;
    }
    return substring;
}


std::string UTF8Substr(const std::string &utf8_string, const size_t pos, const size_t len) {
    const size_t total_length(CodePointCount(utf8_string));
    if (pos == 0) {
        if (unlikely(len == std::string::npos))
            return utf8_string;
        return ExtractUTF8Substring(utf8_string.cbegin(), utf8_string.cend(), len);
    } else if (pos == total_length)
        return "";
    else if (unlikely(pos > total_length))
        throw std::out_of_range("substring start is out-of-range in TextUtil::UTF8Substr!");
    else {
        std::string::const_iterator start(utf8_string.cbegin());
        size_t skip_count(0);
        while (start != utf8_string.cend() and skip_count < pos) {
            if (IsStartOfUTF8CodePoint(*start)) {
                if (skip_count == pos)
                    break;
                ++skip_count;
            }
            ++start;
        }
        return ExtractUTF8Substring(start, utf8_string.cend(), len);
    }
}


} // namespace TextUtil
