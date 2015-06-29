#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H


#include <string>
#include <unordered_set>
#include <cwchar>


namespace TextUtil {


/** \brief Strips HTML tags and converts entities. */
std::string ExtractText(const std::string &html);


/** \brief Recognises roman numerals up to a few thousand. */
bool IsRomanNumeral(const std::string &s);


/** \brief Recognises base-10 unsigned integers. */
bool IsUnsignedInteger(const std::string &s);


/** \brief Convert UTF8 to wide characters. */
bool UTF8toWCharString(const std::string &utf8_string, std::basic_string<wchar_t> * wchar_string);


/** \brief Convert wide characters to UTF8. */
bool WCharToUTF8String(const std::basic_string<wchar_t> &wchar_string, std::string * utf8_string);
    

/** \brief Converts a UTF8 string to lowercase. */
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


} // namespace TextUtil


#endif // ifndef TEXT_UTIL_H
