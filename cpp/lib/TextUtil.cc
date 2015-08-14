#include "TextUtil.h"
#include <algorithm>
#include <exception>
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
    static RegexMatcher *matcher(NULL);
    if (unlikely(matcher == NULL)) {
        const std::string pattern("^M{0,4}(CM|CD|D?C{0,3})(XC|XL|L?X{0,3})(IX|IV|V?I{0,3})$");
        matcher = RegexMatcher::RegexMatcherFactory(pattern, &err_msg);
        if (unlikely(matcher == NULL))
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
    static RegexMatcher *matcher(NULL);
    if (unlikely(matcher == NULL)) {
        const std::string pattern("^[0-9]+$");
        matcher = RegexMatcher::RegexMatcherFactory(pattern, &err_msg);
        if (unlikely(matcher == NULL))
            throw std::runtime_error("Failed to construct a RegexMatcher for \"" + pattern
                                     + "\" in TextUtil::IsUnsignedInteger: " + err_msg);
    }

    const bool retcode(matcher->matched(s, &err_msg));
    if (unlikely(not err_msg.empty()))
        throw std::runtime_error("Failed to match \"" + s + "\" against pattern \"" + matcher->getPattern()
                                 + "\" in TextUtil::IsUnsignedInteger: " + err_msg);

    return retcode;
}


bool UTF8toWCharString(const std::string &utf8_string, std::basic_string<wchar_t> * wchar_string) {
    wchar_string->clear();

    const char *cp(utf8_string.c_str());
    size_t remainder(utf8_string.size());
    std::mbstate_t state = std::mbstate_t();
    for (;;) {
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
}


bool WCharToUTF8String(const std::basic_string<wchar_t> &wchar_string, std::string * utf8_string) {
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


bool UTF8ToLower(const std::string &utf8_string, std::string * const lowercase_utf8_string) {
    std::basic_string<wchar_t> wchar_string;
    if (not UTF8toWCharString(utf8_string, &wchar_string))
        return false;

    // Lowercase the wide character string:
    std::basic_string<wchar_t> lowercase_wide_string;
    for (const auto wide_ch : wchar_string)
        lowercase_wide_string += std::towlower(static_cast<wint_t>(wide_ch));

    return WCharToUTF8String(lowercase_wide_string, lowercase_utf8_string);
}


namespace {


template<typename ContainerType> bool ChopIntoWords(const std::string &text, ContainerType * const words,
						    const unsigned min_word_length)
{
    words->clear();

    std::basic_string<wchar_t> wide_text;
    if (unlikely(not UTF8toWCharString(text, &wide_text)))
        return false;

    std::basic_string<wchar_t> word;
    std::string utf8_word;
    bool leading(true);
    for (const wchar_t ch : wide_text) {
        if (leading and (ch == L'-' or ch == L'\''))
            ; // Do nothing!
        else if (iswalnum(ch) or ch == L'-' or ch == L'\'') {
            word += ch;
            leading = false;
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

    if (haystack.size() < needle.size())
	return haystack.cend();

    const std::vector<std::string>::const_iterator haystack_start(
	std::find(haystack.cbegin(), haystack.cend(), needle[0]));
    if ((haystack.cend() - haystack_start) < static_cast<ssize_t>(needle.size()))
	return haystack.cend();

    std::vector<std::string>::const_iterator needle_word(needle.cbegin());
    std::vector<std::string>::const_iterator haystack_word(haystack_start);
    for (;;) {
	++needle_word;
	if (needle_word == needle.cend())
	    return haystack_start;
	++haystack_word;
	if (haystack_word == haystack.cend() or *haystack_word != *needle_word)
	    return haystack.cend();
    }
}
    

} // namespace TextUtil
