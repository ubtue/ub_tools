/** \file    HtmlParser.cc
 *  \brief   Implementation of an HTML parser class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2007 Dr. Johannes Ruscheinski.
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

#include "HtmlParser.h"
#include <stdexcept>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include "Compiler.h"
#include "StringUtil.h"
#include "util.h"


// uncomment the following line for the builtin debugging main():
//#define DEBUG_HTML_PARSER


#ifdef DIM
#      undef DIM
#endif
#define DIM(array)      (sizeof(array) / sizeof(array[0]))


namespace {


struct EntityMapEntry {
    const char *entity_;
    unsigned char code_;
} entity_map[] = {
    { "quot",   '"'    },
    { "apos",   '\''   },
    { "amp",    '&'    },
    { "lt",     '<'    },
    { "gt",     '>'    },
    { "nbsp",   static_cast<unsigned char>('\xA0') },
    { "iexcl",  161u    },
    { "cent",   162u    },
    { "pound",  163u    },
    { "curren", 164u    },
    { "yen",    165u    },
    { "brvbar", 166u    },
    { "sect",   167u    },
    { "uml",    168u    },
    { "copy",   169u    },
    { "ordf",   170u    },
    { "laquo",  171u    },
    { "not",    172u    },
    { "shy",    173u    },
    { "reg",    174u    },
    { "macr",   175u    },
    { "deg",    176u    },
    { "plusmn", 177u    },
    { "sup2",   178u    },
    { "sup3",   179u    },
    { "acute",  180u    },
    { "micro",  181u    },
    { "para",   182u    },
    { "middot", 183u    },
    { "cedil",  184u    },
    { "sup1",   185u    },
    { "ordm",   186u    },
    { "raquo",  187u    },
    { "fraq14", 188u    },
    { "fraq12", 189u    },
    { "fraq34", 190u    },
    { "iquest", 191u    },
    { "Agrave", 192u    },
    { "Aacute", 193u    },
    { "Acirc",  194u    },
    { "Atilde", 195u    },
    { "Auml",   196u    },
    { "Aring",  197u    },
    { "AElig",  198u    },
    { "Ccedil", 199u    },
    { "Egrave", 200u    },
    { "Eacute", 201u    },
    { "Ecirc",  202u    },
    { "Euml",   203u    },
    { "Igrave", 204u    },
    { "Iacute", 205u    },
    { "Icirc",  206u    },
    { "Iuml",   207u    },
    { "ETH",    208u    },
    { "Ntilde", 209u    },
    { "Ograve", 210u    },
    { "Oacute", 211u    },
    { "Ocirc",  212u    },
    { "Otilde", 213u    },
    { "Ouml",   214u    },
    { "times",  215u    },
    { "Oslash", 216u    },
    { "Ugrave", 217u    },
    { "Uacute", 218u    },
    { "Ucirc",  219u    },
    { "Uuml",   220u    },
    { "Yacute", 221u    },
    { "THORN",  222u    },
    { "szlig",  223u    },
    { "agrave", 224u    },
    { "aacute", 225u    },
    { "acirc",  226u    },
    { "atilde", 227u    },
    { "auml",   228u    },
    { "aring",  229u    },
    { "aelig",  230u    },
    { "ccedil", 231u    },
    { "egrave", 232u    },
    { "eacute", 233u    },
    { "ecirc",  234u    },
    { "euml",   235u    },
    { "igrave", 236u    },
    { "iacute", 237u    },
    { "icirc",  238u    },
    { "iuml",   239u    },
    { "eth",    240u    },
    { "ntilde", 241u    },
    { "ograve", 242u    },
    { "oacute", 243u    },
    { "ocirc",  244u    },
    { "otilde", 245u    },
    { "ouml",   246u    },
    { "divide", 247u    },
    { "oslash", 248u    },
    { "ugrave", 249u    },
    { "uacute", 250u    },
    { "ucirc",  251u    },
    { "uuml",   252u    },
    { "yacute", 253u    },
    { "thorn",  254u    },
    { "yuml",   255u    },
};


bool DecodeEntity(const char *entity_string, char * const ch) {
    // numeric entity?
    if (entity_string[0] == '#') { // Yes!
        errno = 0;
        unsigned long code;

        // Hexdecimal version?
        if (entity_string[1] == 'x') // Yes!
            code = std::strtoul(entity_string + 2, nullptr, 16);
        else // We are dealing with the decimal version.
            code = std::strtoul(entity_string + 1, nullptr, 10);

        if (errno != 0 or code > 255)
            return false;

        *ch = static_cast<char>(code);
        return true;
    }

    for (unsigned i = 0; i < DIM(entity_map); ++i) {
        if (std::strcmp(entity_string, entity_map[i].entity_) == 0) {
            *ch = entity_map[i].code_;
            return true;
        }
    }

    *ch = '\0';
    return false;
}


void __attribute__((__format__(printf,1,2),noreturn)) Throw(const char * const fmt, ...) {
    char msg_buffer[1024+1];
    std::strcpy(msg_buffer, "HtmlParser: ");
    size_t len = std::strlen(msg_buffer);

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer+len, sizeof(msg_buffer)-len, fmt, args);
    va_end(args);

    throw std::runtime_error(msg_buffer);
}


inline bool IsAsciiLetter(const int ch) {
    return ('A' <= ch and ch <= 'Z') or ('a' <= ch and ch <= 'z');
}


inline bool IsAsciiLetterOrDigit(const int ch) {
    return ('A' <= ch and ch <= 'Z') or ('a' <= ch and ch <= 'z') or ('0' <= ch and ch <= '9');
}


} // unnamed namespace


// HtmlParser::AttributeMap::toString -- Construct a string representation of an attribute map.
//
std::string HtmlParser::AttributeMap::toString() const
{
    std::string result;
    for (std::map<std::string, std::string>::const_iterator pair(map_.begin()); pair != map_.end(); ++pair)
        result += " " + pair->first + "=\"" + pair->second + "\"";

    return result;
}


// HtmlParser::Chunk::toString -- Construct a string representation of a chunk.
//
std::string HtmlParser::Chunk::toString() const
{
    switch (type_) {
    case OPENING_TAG:
    case MALFORMED_TAG:
        return "<" + text_ + (attribute_map_ == nullptr ? "" : attribute_map_->toString()) + ">";
    case CLOSING_TAG:
    case UNEXPECTED_CLOSING_TAG:
        return "</" + text_ + ">";
    case WORD:
    case PUNCTUATION:
    case WHITESPACE:
    case TEXT:
        return text_;
    case COMMENT:
        return "<!--" + text_ + "->\n";
    case END_OF_STREAM:
    case UNEXPECTED_END_OF_STREAM:
        if (error_message_.empty())
            return "";
        else
            return "(" + error_message_ + ")";
    }

    // If we get this far, it's an error:
    Throw("cannot convert an unknown chunk type to a string!");
}


// HtmlParser::Chunk::toPlainText -- Return the "plain text" embodied by this HTML fragment.
//
std::string HtmlParser::Chunk::toPlainText() const
{
    switch (type_) {
    case WORD:
    case PUNCTUATION:
    case WHITESPACE:
    case TEXT:
        return text_;
    default:
        return "";
    }
}


HtmlParser::HtmlParser(const std::string &input_string, unsigned chunk_mask, const bool header_only)
    : input_string_(StringUtil::strnewdup(input_string.c_str())), lineno_(1), chunk_mask_(chunk_mask), header_only_(header_only), is_xhtml_(false)
{
    if ((chunk_mask_ & TEXT) and (chunk_mask_ & (WORD | PUNCTUATION | WHITESPACE)))
        throw std::runtime_error("TEXT cannot be set simultaneously with any of WORD, PUNCTUATION or WHITESPACE!");

    cp_ = cp_start_ = input_string_;
    end_of_stream_ = *cp_ == '\0';

    if (likely(not end_of_stream_))
        replaceEntitiesInString();
}


bool HtmlParser::AttributeMap::insert(const std::string &name, const std::string &value)
{
    const const_iterator iter = find(name);
    if (iter != end())
        return false;

    map_.insert(std::pair<std::string, std::string>(name, value));
    return true;
}


void HtmlParser::replaceEntitiesInString()
{
    char *read_ptr = input_string_;
    char *write_ptr = read_ptr;

    while (likely(*read_ptr != '\0')) {
        if (likely(*read_ptr != '&')) {
            *write_ptr = *read_ptr;
            ++read_ptr;
            ++write_ptr;
            continue;
        }

        //
        // If we get here we're pointing at an ampersand with "read_ptr":
        //

        char entity[6 + 1];
        unsigned entity_length = 0;
        char *entity_read_ptr = read_ptr;
        while (entity_length < sizeof(entity) - 1) {
            ++entity_read_ptr;
            if (unlikely(*entity_read_ptr == '\0'))
                break;

            if (*entity_read_ptr == ';')
                break;
            else
                entity[entity_length++] = *entity_read_ptr;
        }

        // Hit EOS while trying to store an entity?
        if (unlikely(*entity_read_ptr == '\0')) {
            // We found a truncated entity or some other garbage at the end of our document
            // and just discard it.
            *write_ptr = '\0';
            return;
        }

        entity[entity_length] = '\0';
        if (entity_length == sizeof(entity) - 1) {
            ++entity_read_ptr;
            if (*entity_read_ptr != ';') {
                *write_ptr = '&';
                ++read_ptr;
                ++write_ptr;
                continue; // we couldn't find an entity
            }
        }

        char code('\0');
        if (likely(DecodeEntity(entity, &code))) {
            *write_ptr = code;
            read_ptr += entity_length + 2; // 2 for '&' and ';'
            if (code == '<' or code == '>')
                angle_bracket_entity_positions_.insert(write_ptr);
        }
        else {
            *write_ptr = '&';
            ++read_ptr;
        }
        ++write_ptr;
    }

    *write_ptr = '\0';
}


int HtmlParser::getChar(bool * const is_entity)
{
    if (unlikely(is_entity != nullptr))
        *is_entity = false;

    char c = *cp_;
    if (unlikely(c == '\0')) {
        end_of_stream_ = true;
        return EOF;
    }
    if (unlikely(c == '\n'))
        ++lineno_;
    else if (c == '<' or c == '>') {
        if (unlikely(is_entity != nullptr))
            *is_entity = angle_bracket_entity_positions_.find(const_cast<char *>(cp_))
                != angle_bracket_entity_positions_.end();
    }
    ++cp_;

    return c;
}


void HtmlParser::ungetChar()
{
    if (unlikely(cp_ == cp_start_))
        logger->error("in HtmlParser::ungetChar: trying to push back at beginning of input!");

    --cp_;
    if (unlikely(*cp_ == '\n'))
        --lineno_;
}


void HtmlParser::skipJavaScriptStringConstant(const int start_quote)
{
    const unsigned start_lineno(lineno_);
    bool escaped(false); // Have we seen a backslash?
    for (;;) {
        int ch = getChar();
        if (unlikely(endOfStream())) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected EOF within JavaScript string literal (started on line "
                                       + std::to_string(start_lineno) + ")");
            preNotify(&unexpected_eof_chunk);
            return;
        }
        if (ch == start_quote and not escaped)
            return;
        escaped = ch == '\\';
    }
}


// HtmlParser::skipJavaScriptDoubleSlashComment -- skips over a JavaScript "//" comment and trailing lineends assuming
//                                                 we've already seen "//".
//
void HtmlParser::skipJavaScriptDoubleSlashComment()
{
    int ch;
    do {
        ch = getChar();
        if (unlikely(endOfStream()))
            return;
    } while (ch != '\n' and ch != '\r');
    while (ch == '\n' or ch == '\r')
        ch = getChar();

    // Last character was not a lineend character:
    if (not endOfStream())
        ungetChar();
}


// HtmlParser::skipJavaScriptCStyleComment -- skips over a JavaScript C-style comment assuming we've already "/*".
//
void HtmlParser::skipJavaScriptCStyleComment()
{
    int ch;
    const unsigned start_lineno(lineno_);
    for (;;) {
        do {
            ch = getChar();
            if (unlikely(endOfStream())) {
                Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                           "unxepected EOF in JavaScript C-style comment (started "
                                           "on line " + std::to_string(start_lineno) + ")");
                preNotify(&unexpected_eof_chunk);
                return;
            }
        } while (ch != '*');
        while (ch == '*') {
            ch = getChar();
            if (unlikely(endOfStream())) {
                Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                           "unxepected EOF in JavaScript C-style comment (started on "
                                           "line " + std::to_string(start_lineno) + ")");
                preNotify(&unexpected_eof_chunk);
                return;
            }
            if (ch == '/')
                return;
        }
    }
}


void HtmlParser::skipWhiteSpace()
{
    for (;;) {
        int ch = getChar();
        if (not isspace(ch)) {
            if (not endOfStream())
                ungetChar();
            return;
        }
    }
}


// HtmlParser::skipDoctype -- assumes that at this point we have read "<!DOCTYPE" and
//                            skips over all input up to and including ">".
//
void HtmlParser::skipDoctype()
{
    int ch;
    do
        ch = getChar();
    while (ch != '>' and ch != EOF);
    if (ch == EOF) {
        Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                   "unexpected end of HTML while skipping over a DOCTYPE");
        preNotify(&unexpected_eof_chunk);
    }
}


// HtmlParser::skipComment -- assumes that at this point we have read "<!--" and
//                            skips over all input up to and including "-->".
//
void HtmlParser::skipComment()
{
    const unsigned start_lineno(lineno_);
    std::string comment_text;
    unsigned hyphen_count = 0;
    for (;;) {
        int ch = getChar();
        if (unlikely(ch == EOF)) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected EOF within HTML comment (started on line "
                                       + std::to_string(start_lineno) + ")");
            preNotify(&unexpected_eof_chunk);
            return;
        }

        comment_text += static_cast<char>(ch);

        if (ch == '>') {
            if (hyphen_count >= 2)
                break;
            hyphen_count = 0;
        }
        else if (ch != '-')
            hyphen_count = 0;
        else
            ++hyphen_count;
    }

    if (chunk_mask_ & COMMENT) {
        // report the contents of the comments without the "--" at the end:
        Chunk comment_chunk(COMMENT, comment_text.substr(0, comment_text.length()-2), start_lineno);
        preNotify(&comment_chunk);
    }
}


// HtmlParser::skipToEndOfTag -- attempts to skip until the closing '>' of a tag or a '<'.
//
void HtmlParser::skipToEndOfTag(const std::string &tag_name, const unsigned tag_start_lineno)
{
    int ch;
    bool is_entity;
    do {
        ch = getChar(&is_entity);
        if (ch == EOF) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected end-of-stream while skipping tag \"" + tag_name
                                       + "\" opened on line " + std::to_string(tag_start_lineno));
            preNotify(&unexpected_eof_chunk);
            return;
        }
    } while (is_entity or (ch != '>' and ch != '<'));
    if (ch == '<')
        ungetChar();
}


// HtmlParser::skipToEndOfScriptOrStyle -- attempts to skip to the ending pair of a block, i.e. given "table" will
//                                 skip to the position just past "</table>". Ignores everything up to the ending
//                                 tag of "tag_name".
//
bool HtmlParser::skipToEndOfScriptOrStyle(const std::string &tag_name, const unsigned tag_start_lineno)
{
    int ch;
    for (;;) {
        ch = getChar();
        if (ch == EOF) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected end-of-stream while skipping tag \"" + tag_name
                                       + "\" opened on line " + std::to_string(lineno_));
            preNotify(&unexpected_eof_chunk);
            return false;
        }

        if (ch != '<') // No end in sight, keep going.
            continue;

        ch = getChar();
        if (ch == EOF) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected end-of-stream while skipping tag \"" + tag_name
                                       + "\" opened on line " + std::to_string(lineno_));
            preNotify(&unexpected_eof_chunk);
            return false;
        }

        if (ch != '/') // We got a '<' but not an end style tag, keep going.
            continue;

        // Ah, we have an ending tag. See if it's the same tag and if so, we are done.
        if (::strcasecmp(extractTagName().c_str(), tag_name.c_str()) == 0) {
            skipToEndOfTag(tag_name, tag_start_lineno);
            return true;
        }
    }
}


// HtmlParser::skipToEndOfMalformedTag -- attempts to skip until the closing '>' of a tag.
//                                        Note: when setting "report_malformed_tag" to true,
//                                        the caller should probably immedeately return after
//                                        invoking this function.
//
void HtmlParser::skipToEndOfMalformedTag(const std::string &tag_name, const unsigned tag_start_lineno)
{
    skipToEndOfTag(tag_name, tag_start_lineno);

    if (chunk_mask_ & MALFORMED_TAG) {
        Chunk malformed_tag(MALFORMED_TAG, tag_name, tag_start_lineno);
        preNotify(&malformed_tag);
    }
}


void HtmlParser::parseWord()
{
    bool hyphen_seen(false);

    std::string word;
    for (;;) {
        const int ch(getChar());
        if (endOfStream())
            break;

        if (isalnum(ch)) {
            hyphen_seen = false;
            word += static_cast<char>(ch);
        }
        else if (ch == '-') {
            if (hyphen_seen) { // Two dashes in a row.
                ungetChar();
                break;
            }

            hyphen_seen = true;
            word += '-';
        }
        else { // Some character that we don't know how to handle.
            ungetChar();
            break;
        }
    }

    // Remove a possible trailing hyphen:
    if (unlikely(word[word.length() - 1] == '-') ) {
        word.resize(word.length() - 1);
        ungetChar();
    }

    if (chunk_mask_ & WORD) {
        Chunk chunk(WORD, word, lineno_);
        preNotify(&chunk);
    }
}


void HtmlParser::parseText()
{
    std::string text;
    for (;;) {
        bool is_entity;
        const int ch(getChar(&is_entity));
        if (endOfStream())
            break;

        if (unlikely(ch == '<') and not is_entity) {
            ungetChar();
            break;
        }
        else
            text += static_cast<char>(ch);
    }

    Chunk chunk(TEXT, text, lineno_);
    preNotify(&chunk);
}


std::string HtmlParser::extractTagName()
{
    std::string tag_name;
    bool is_entity;
    int ch = getChar(&is_entity);
    if (not IsAsciiLetter(ch) or is_entity) {
        ungetChar();
        return ""; // let's hope the caller deals with the error reporting!
    }

    while (not endOfStream() and IsAsciiLetterOrDigit(ch)) {
        tag_name += static_cast<char>(ch);
        ch = getChar();
    }
    if (likely(not endOfStream()))
        ungetChar();

    return StringUtil::ToLower(&tag_name);
}


// HtmlParser::extractAttribute -- returns true if an attribute was found, else false.
//
bool HtmlParser::extractAttribute(const std::string &tag_name, std::string * const attribute_name,
                                  std::string * const attribute_value)
{
    attribute_name->clear();
    attribute_value->clear();

    int ch = getChar();
    if (not IsAsciiLetter(ch)) {
        ungetChar();
        return false;
    }

    *attribute_name += static_cast<char>(ch);
    ch = getChar();
    while (not endOfStream() and (IsAsciiLetterOrDigit(ch) or ch == '-')) {
        *attribute_name += static_cast<char>(ch);
        ch = getChar();
    }
    if (unlikely(endOfStream())) {
        Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                   "unexpected end-of-stream while parsing an attribute name in a \"" + tag_name
                                   + "\" tag on line " + std::to_string(lineno_));
        preNotify(&unexpected_eof_chunk);
        return false;
    }
    ungetChar();

    if (attribute_name->length() == 0)
        return false;

    // cannonize the name:
    StringUtil::ToLower(attribute_name);

    skipWhiteSpace();

    ch = getChar();
    if (ch != '=') {
        if (unlikely(endOfStream())) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected end-of-stream while looking for an equal sign following "
                                       "the attribute name \"" + *attribute_name + "\" in tag \"" + tag_name
                                       + "\" on line " + std::to_string(lineno_));
            preNotify(&unexpected_eof_chunk);
            return false;
        }
        ungetChar();
        return true;
    }

    skipWhiteSpace();

    ch = getChar();
    if (ch == '\'' or ch == '"') {
        const int DELIMITER(ch);
        bool is_entity;
        ch = getChar(&is_entity);
        while (ch != EOF and (is_entity or ch != DELIMITER)) {
            *attribute_value += static_cast<char>(ch);
            ch = getChar(&is_entity);
        }
        if (ch == EOF) {
            Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                       "unexpected end-of-stream while reading attribute value for "
                                       "attribute \"" + *attribute_name + "\" in tag \"" + tag_name
                                       + "\" on line " + std::to_string(lineno_));
            preNotify(&unexpected_eof_chunk);
            return false;
        }
    }
    else { // unquoted attribute_value
        do {
            *attribute_value += static_cast<char>(ch);
            ch = getChar();
        } while (ch != EOF and not isspace(ch) and ch != '>');
        ungetChar();
    }
    if (unlikely(endOfStream())) {
        Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                   "unexpected end-of-stream while parsing an attribute value for "
                                   "attribute \"" + *attribute_name + "\" in tag \"" + tag_name
                                   + "\" on line " + std::to_string(lineno_));
        preNotify(&unexpected_eof_chunk);
        return false;
    }

    // if the lookahead char is a whitespace character we may have more attributes:
    return true;
}


// HtmlParser::parseTag -- parse HTML tags.  Returns false if we want to abort parsing, else true.
//
bool HtmlParser::parseTag()
{
    const unsigned start_lineno(lineno_);

    skipWhiteSpace();
    if (unlikely(endOfStream())) {
        Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                   "tag opened on line " + std::to_string(lineno_) + " was never closed");
        preNotify(&unexpected_eof_chunk);
        return false;
    }

    int ch = getChar();
    bool is_end_tag = ch == '/';
    if (not is_end_tag)
        ungetChar();

    const std::string tag_name(extractTagName());
    if (unlikely(tag_name.length() == 0)) {
        skipToEndOfMalformedTag(tag_name, start_lineno);
        return true;
    }
    if (unlikely(endOfStream())) {
        Chunk unexpected_eof_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                   "tag \"" + tag_name + "\" opened on line " + std::to_string(lineno_)
                                   + " was never closed");
        preNotify(&unexpected_eof_chunk);
        return false;
    }

    // peek ahead:
    ch = getChar();
    if (ch != '>' and not isspace(ch)) {
        ungetChar();
        skipToEndOfMalformedTag(tag_name, start_lineno);
        return true;
    }
    ungetChar();

    AttributeMap attribute_map;

    if (tag_name == "script" or tag_name == "style") {
        if (unlikely(is_end_tag)) {
            if (chunk_mask_ & UNEXPECTED_CLOSING_TAG) {
                Chunk chunk(UNEXPECTED_CLOSING_TAG, tag_name, lineno_);
                preNotify(&chunk);
            }
            skipToEndOfTag(tag_name, start_lineno);
        }
        else {
            if (chunk_mask_ & OPENING_TAG) {
                Chunk chunk(OPENING_TAG, tag_name, start_lineno, &attribute_map);
                preNotify(&chunk);
            }
            if (not skipToEndOfScriptOrStyle(tag_name, lineno_))
                return false;
            if (chunk_mask_ & CLOSING_TAG) {
                Chunk chunk(CLOSING_TAG, tag_name, lineno_);
                preNotify(&chunk);
            }
        }

        return true;
    }

    if (not is_end_tag) {
        std::string attribute_name, attribute_value;
        bool more;
        do {
            skipWhiteSpace();
            more = extractAttribute(tag_name, &attribute_name, &attribute_value);
            if (attribute_name.length() > 0)
                attribute_map.insert(attribute_name, attribute_value);
        } while (more);
    }

    skipWhiteSpace();

    ch = getChar();
    bool is_opening_and_closing_tag = ch == '/';
    if (is_opening_and_closing_tag) {
        if (is_end_tag) {
            skipToEndOfMalformedTag(tag_name, start_lineno);
            return true;
        }
        ch = getChar();
    }
    if (ch != '>') {
        ungetChar();
        skipToEndOfMalformedTag(tag_name, start_lineno);
        return true;
    }

    if (is_end_tag) {
        if (header_only_ and tag_name == "header")
            return false;
        if (chunk_mask_ & CLOSING_TAG) {
            Chunk chunk(CLOSING_TAG, tag_name, start_lineno);
            preNotify(&chunk);
        }
    }
    else { // We have an opening tag.
        if (header_only_ and tag_name == "body")
            return false;
        if (chunk_mask_ & OPENING_TAG) {
            Chunk chunk(OPENING_TAG, tag_name, start_lineno, &attribute_map);
            preNotify(&chunk);
            if (is_opening_and_closing_tag and (chunk_mask_ & CLOSING_TAG)) {
                Chunk closing_tag_chunk(CLOSING_TAG, tag_name, start_lineno);
                preNotify(&closing_tag_chunk);
            }
        }
    }

    return true;
}


void HtmlParser::parse()
{
    const unsigned NBSP(0xA0); // Non-breaking space.

    try {
        for (;;) {
            bool is_entity;
            int ch(getChar(&is_entity));

            if (unlikely(ch == EOF)) {
                if (chunk_mask_ & END_OF_STREAM) {
                    Chunk end_of_stream_chunk(END_OF_STREAM, "", lineno_);
                    preNotify(&end_of_stream_chunk);
                }
                return;
            }
            if (ch == '<' and not is_entity) {
                if ((ch = getChar()) != '!') {
                    ungetChar();
                    if (unlikely(not parseTag()))
                        return;
                }
                else { // must be a comment or DOCTYPE
                    ch = getChar();
                    if (ch != '-') { // We assume that we have a DOCTYPE
                        unsigned start_lineno(lineno_);
                        std::string doctype;
                        do {
                            doctype += static_cast<char>(ch);
                            ch = getChar();
                        } while (not endOfStream() and ch != '>' and doctype.length() < 7);
                        if (unlikely(endOfStream())) {
                            Chunk unexpected_end_of_stream_chunk(UNEXPECTED_END_OF_STREAM, lineno_,
                                                                 "unexpected end-of-stream while skipping \"!" + doctype
                                                                 + "...\"");
                            preNotify(&unexpected_end_of_stream_chunk);
                            return;
                        }

                        std::string uppercase_doctype(doctype);
                        if (StringUtil::ToUpper(&uppercase_doctype) != "DOCTYPE") {
                            ungetChar(); // unget the '>'
                            skipToEndOfMalformedTag("!" + doctype, start_lineno);
                            continue;
                        }
                        skipDoctype();
                    }
                    else { // we assume we have a comment
                        ch = getChar();
                        if (unlikely(ch != '-'))
                            Throw("possibly malformed comment on line %u!", lineno_);

                        skipComment();
                    }
                }
            }
            else if (chunk_mask_ & TEXT) {
                ungetChar();
                parseText();
            }
            else if ((isspace(ch) or static_cast<unsigned char>(ch) == NBSP) and (chunk_mask_ & WHITESPACE)) {
                char s[1 + 1];
                s[0] = static_cast<char>(ch);
                s[1] = '\0';
                Chunk whitespace_chunk(WHITESPACE, s, ch == '\n' ? lineno_ - 1 : lineno_);
                preNotify(&whitespace_chunk);
            }
            else if (isalnum(ch)) {
                ungetChar();
                parseWord();
            }
            else if (ispunct(ch) and (chunk_mask_ & PUNCTUATION)) {
                std::string punctuation;
                punctuation += static_cast<char>(ch);
                Chunk chunk(PUNCTUATION, punctuation, lineno_);
                preNotify(&chunk);
            }
        }
    }
    catch (const std::exception &x) {
        if (chunk_mask_ & UNEXPECTED_END_OF_STREAM) {
            Chunk unexpected_eos_chunk(UNEXPECTED_END_OF_STREAM, x.what(), lineno_);
            preNotify(&unexpected_eos_chunk);
        }
    }
}


void MetaTagExtractor::notify(const Chunk &chunk)
{
    if (chunk.text_ == "meta") {
        // Read the current tag
        AttributeMap::const_iterator name_attrib = chunk.attribute_map_->find("name");
        if (name_attrib == chunk.attribute_map_->end())
            return; // malformed meta tag (has no "name" attribute)
        std::string current_tag(name_attrib->second);
        StringUtil::ToLower(&current_tag);

        // Make sure this is one of the META tags we are looking for
        bool name_matches_list(false);
        for (std::list<std::string>::const_iterator meta_tag_name(meta_tag_names_.begin()); meta_tag_name != meta_tag_names_.end();
             ++meta_tag_name)
            {
                if (::strcasecmp(current_tag.c_str(), meta_tag_name->c_str()) == 0) {
                    name_matches_list = true;
                    break;
                }
            }
        if (not name_matches_list)
            return;

        // Add the content of the tag to the extracted_data_ list
        AttributeMap::const_iterator content_attrib = chunk.attribute_map_->find("content");
        if (content_attrib == chunk.attribute_map_->end())
            return; // malformed meta tag (has no "content" attribute)
        extracted_data_.push_back(std::make_pair(current_tag, content_attrib->second));
    }
}


void HttpEquivExtractor::notify(const Chunk &chunk)
{
    if (chunk.text_ == "meta") {
        // Read the current tag
        AttributeMap::const_iterator name_attrib = chunk.attribute_map_->find("http-equiv");
        if (name_attrib == chunk.attribute_map_->end())
            return; // malformed meta tag (has no "name" attribute)
        std::string current_tag(name_attrib->second);
        StringUtil::ToLower(&current_tag);

        // Make sure this is one of the META tags we are looking for
        bool name_matches_list(false);
        for (std::list<std::string>::const_iterator meta_tag_name(meta_tag_names_.begin()); meta_tag_name != meta_tag_names_.end();
             ++meta_tag_name)
            {
                if (::strcasecmp(current_tag.c_str(), meta_tag_name->c_str()) == 0) {
                    name_matches_list = true;
                    break;
                }
            }
        if (not name_matches_list)
            return;

        // Add the content of the tag to the extracted_data_ list
        AttributeMap::const_iterator content_attrib = chunk.attribute_map_->find("content");
        if (content_attrib == chunk.attribute_map_->end())
            return; // malformed meta tag (has no "content" attribute)
        extracted_data_.push_back(std::make_pair(current_tag, content_attrib->second));
    }
}


std::string HtmlParser::ChunkTypeToString(const unsigned chunk_type) {
    switch (chunk_type) {
    case OPENING_TAG:
        return "OPENING_TAG";
    case CLOSING_TAG:
        return "CLOSING_TAG";
    case MALFORMED_TAG:
        return "MALFORMED_TAG";
    case UNEXPECTED_CLOSING_TAG:
        return "UNEXPECTED_CLOSING_TAG";
    case WORD:
        return "WORD";
    case TEXT:
        return "TEXT";
    case PUNCTUATION:
        return "PUNCTUATION";
    case COMMENT:
        return "COMMENT";
    case WHITESPACE:
        return "WHITESPACE";
    case END_OF_STREAM:
        return "END_OF_STREAM";
    case EVERYTHING:
        return "EVERYTHING";
    default:
        throw std::runtime_error("in HtmlParser::ChunkTypeToString: unknown chunk type: " + std::to_string(chunk_type) + "!");
    }
}


// UrlExtractorParser::notify -- act on the notification that a chunk of HTML has been parsed.
//
void UrlExtractorParser::notify(const Chunk &chunk) {
    // Handle opening/closing "a" tags:
    if (chunk.text_ == "a") {
        if (chunk.type_ == HtmlParser::OPENING_TAG) {
            AttributeMap::const_iterator href_attrib = chunk.attribute_map_->find("href");
            if (href_attrib != chunk.attribute_map_->end()) {
                last_url_and_anchor_text_.url_ = href_attrib->second;
                StringUtil::TrimWhite(&last_url_and_anchor_text_.url_);
                opening_a_tag_seen_ = true;
            }
        }
        else if (chunk.type_ == HtmlParser::CLOSING_TAG and opening_a_tag_seen_) {
            opening_a_tag_seen_ = false;
            if (not last_url_and_anchor_text_.url_.empty()) {
                if (clean_up_anchor_text_) {
                    StringUtil::TrimWhite(&last_url_and_anchor_text_.anchor_text_);
                    StringUtil::CollapseWhitespace(&last_url_and_anchor_text_.anchor_text_);
                }
                urls_.push_back(last_url_and_anchor_text_);
                last_url_and_anchor_text_.clear();
            }
        }
    }
    // Handle the "area" tag:
    else if (chunk.text_ == "area") {
        if (chunk.type_ == HtmlParser::OPENING_TAG) {
            const AttributeMap::const_iterator href_attrib(chunk.attribute_map_->find("href"));
            if (href_attrib != chunk.attribute_map_->end()) {
                std::string href_str(href_attrib->second);
                last_url_and_anchor_text_.url_ = href_str;
                const AttributeMap::const_iterator alt(chunk.attribute_map_->find("alt"));
                if (alt == chunk.attribute_map_->end())
                    last_url_and_anchor_text_.anchor_text_.clear();
                else {
                    std::string alt_text(alt->second);
                    StringUtil::TrimWhite(&alt_text);
                    StringUtil::CollapseWhitespace(&alt_text);
                    last_url_and_anchor_text_.anchor_text_ = alt_text;
                }

                // Save the occurence:
                urls_.push_back(last_url_and_anchor_text_);
                last_url_and_anchor_text_.clear();
            }
        }
    }
    // Handle "frame" tag:
    if (chunk.text_ == "frame" and accept_frame_tags_ and chunk.type_ == HtmlParser::OPENING_TAG) {
        AttributeMap::const_iterator src_attrib = chunk.attribute_map_->find("src");
        if (src_attrib != chunk.attribute_map_->end()) {
            last_url_and_anchor_text_.anchor_text_.clear();
            last_url_and_anchor_text_.url_ = src_attrib->second;
            urls_.push_back(last_url_and_anchor_text_);
            last_url_and_anchor_text_.clear();
        }
    }
    // Image tags:
    else if (chunk.type_ == HtmlParser::OPENING_TAG and chunk.text_ == "img" and ignore_image_tags_) {
        last_url_and_anchor_text_.clear();
        opening_a_tag_seen_ = false;
    }
    // Base tag:
    else if (chunk.type_ == HtmlParser::OPENING_TAG and chunk.text_ == "base") {
        AttributeMap::const_iterator href_attrib = chunk.attribute_map_->find("href");
        if (href_attrib != chunk.attribute_map_->end())
            base_url_ = href_attrib->second;
    }
    // Anchor text:
    else if ((chunk.type_ == HtmlParser::WORD or chunk.type_ == HtmlParser::PUNCTUATION or chunk.type_ ==  HtmlParser::WHITESPACE)
             and opening_a_tag_seen_)
        last_url_and_anchor_text_.anchor_text_ += chunk.text_;
}


#ifdef DEBUG_HTML_PARSER
#include <iostream>


class SentenceCounter: public HtmlParser {
    static const unsigned MAX_SENTENCE_LENGTH = 50;
    unsigned no_of_sentences_;
    unsigned count_[MAX_SENTENCE_LENGTH];
    unsigned current_sentence_word_count_;
public:
    explicit SentenceCounter(std::istream &input);
    virtual void notify(const Chunk &chunk);
    void report(std::ostream &output) const;
};


SentenceCounter::SentenceCounter(std::istream &input)
    : HtmlParser(input, HtmlParser::PUNCTUATION | HtmlParser::WORD), no_of_sentences_(0),
      current_sentence_word_count_(0)
{
    for (unsigned i = 0; i < MAX_SENTENCE_LENGTH; ++i)
        count_[i] = 0;
}


void SentenceCounter::notify(const Chunk &chunk)
{
    if (chunk.type_ == HtmlParser::PUNCTUATION and (chunk.text_[0] == '.' or chunk.text_[0] == '?' or chunk.text_[0] == '!')) {
        ++no_of_sentences_;
        if (current_sentence_word_count_ >= MAX_SENTENCE_LENGTH)
            ++count_[MAX_SENTENCE_LENGTH - 1];
        else
            ++count_[current_sentence_word_count_];


        current_sentence_word_count_ = 0;
    }
    else if (chunk.type_ == HtmlParser::WORD)
        ++current_sentence_word_count_;
}


void SentenceCounter::report(std::ostream &output) const
{
    for (unsigned i = 0; i < MAX_SENTENCE_LENGTH - 1; ++i) {
        if (count_[i] > 0)
            output << i << '\t' << count_[i] << '\n';
    }

    if (count_[MAX_SENTENCE_LENGTH - 1] > 0)
        output << MAX_SENTENCE_LENGTH << "+\t" << count_[MAX_SENTENCE_LENGTH - 1] << "\n\n";
}


class TestParser: public HtmlParser {
public:
    explicit TestParser(std::istream &input)
        : HtmlParser(input) { }
    virtual void notify(const Chunk &chunk);
};


void TestParser::notify(const Chunk &chunk)
{
    if (chunk.type_ == HtmlParser::OPENING_TAG)
        std::cerr << "Found opening tag: " << chunk.text_ << '\n';
    else if (chunk.type_ == HtmlParser::CLOSING_TAG)
        std::cerr << "Found closing tag: " << chunk.text_ << '\n';
    else if (chunk.type_ == HtmlParser::PUNCTUATION)
        std::cerr << "Found punctuation: " << chunk.text_[0] << '\n';
    else if (chunk.type_ == HtmlParser::WORD)
        std::cerr << "Found \"word\": " << chunk.text_ << '\n';
}


int main()
{
    try {
        MiscUtil::Locale locale("en_US.ISO-8859-1", LC_CTYPE);
#if 0
        SentenceCounter sentence_counter(std::cin);
        sentence_counter.parse();
        sentence_counter.report(std::cout);
#else
        TestParser test_parser(std::cin);
        test_parser.parse();
#endif
    }
    catch (const std::exception &x) {
        std::cerr << "Caught exception: " << x.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


#endif // ifdef DEBUG_HTML_PARSER
