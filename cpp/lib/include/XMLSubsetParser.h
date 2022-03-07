/** \file   XMLSubsetParser.h (former SimpleXmlParser.h)
 *  \brief  A non-validating XML parser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include "Compiler.h"
#include "TextUtil.h"
#include "XmlUtil.h"
#include "util.h"


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#endif // ifndef ARRAY_SIZE


/**
 * Note on encoding: XML files can optinonally specify their encoding in the prologue/header. Also,
 *                   datasources can optionally be supplied with their corresponding encoding.
 *                   We attempt to resolve the encoding in the following manner:
 *                      1. Parse the optional header/prologue and use the encoding specified therein.
 *                      2. If the header is missing, then we use the supplied encoding.
 *                      3. If neither of the above are available, we fallback to UTF-8 as the default
 *                         and fail elegantly.
 */
template <typename DataSource>
class XMLSubsetParser {
public:
    enum Type { UNINITIALISED, START_OF_DOCUMENT, END_OF_DOCUMENT, ERROR, OPENING_TAG, CLOSING_TAG, CHARACTERS };

private:
    // Order-dependent: See below (cf. FIRST_FOUR_BYTES_...)
    enum Encoding : uint8_t { UTF32_BE, UTF32_LE, UTF16_BE, UTF16_LE, UTF8, OTHER };

    DataSource * const input_;
    std::deque<int> pushed_back_chars_;
    unsigned line_no_;
    Type last_type_;
    std::string last_error_message_;
    bool last_element_was_empty_;
    std::string last_tag_name_;
    std::string *data_collector_;
    std::string internal_encoding_;
    std::string external_encoding_;
    std::unique_ptr<TextUtil::ToUTF32Decoder> to_utf32_decoder_;
    off_t datasource_content_start_pos_; // offset in the datasource that denotes the start of the content (excluding BOMs)

    static const std::deque<int> CDATA_START_DEQUE;

    // arrays corresponding to the different encodings
    static const uint8_t FIRST_FOUR_BYTES_WITH_BOM[5][4];
    static const uint8_t FIRST_FOUR_BYTES_NO_BOM[5][4];
    static const std::string CANONICAL_ENCODING_NAMES[5];

public:
    XMLSubsetParser(DataSource * const input, const std::string &external_encoding = "");

    bool getNext(Type * const type, std::map<std::string, std::string> * const attrib_map, std::string * const data);

    const std::string &getLastErrorMessage() const { return last_error_message_; }

    /** \brief Returns 0 if line number cannot be determined, e.g. after seek was used. */
    unsigned getLineNo() const { return line_no_; }

    DataSource *getDataSource() const { return input_; }

    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tags  If "type" is OPENING_TAG or CLOSING_TAG, the name of the tags we're looking for.  We return when we
     *                        have found the first matching one.
     *  \param found_tag      If "type" is OPENING_TAG or CLOSING_TAG, the name of the actually found tag.
     *  \param attrib_map     If not NULL and if we find what we're looking for and if it is an opening tag, the attribute map
     *                        for the found opening tag.
     *  \param data           If not NULL, the skipped over XML will be returned here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    bool skipTo(const Type expected_type, const std::vector<std::string> &expected_tags, std::string * const found_tag,
                std::map<std::string, std::string> * const attrib_map = nullptr, std::string * const data = nullptr);

    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tag   If "type" is OPENING_TAG or CLOSING_TAG, the name of the tag we're looking for.
     *  \param attrib_map     If not NULL and if we find what we're looking for and if it is an opening tag, the attribute map
     *                        for the found opening tag.
     *  \param data           If not NULL, the skipped over XML will be returned here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    bool skipTo(const Type expected_type, const std::string &expected_tag = "",
                std::map<std::string, std::string> * const attrib_map = nullptr, std::string * const data = nullptr);

    void skipWhiteSpace();
    void rewind();
    bool seek(const off_t offset, const int whence = SEEK_SET);
    off_t tell() const;

    static std::string TypeToString(const Type type);

private:
    void detectEncoding();
    int getUnicodeCodePoint();
    bool skipOptionalComment();
    int get(const bool skip_comment = true, bool * const cdata_start = nullptr);
    int peek();
    void unget(const int ch);
    bool extractAttribute(std::string * const name, std::string * const value, std::string * const error_message);
    void parseOptionalPrologue();
    bool skipOptionalProcessingInstruction();
    bool extractName(std::string * const name);
    bool extractQuotedString(const int closing_quote, std::string * const s);
    bool parseCDATA(std::string * const data);
    bool parseOpeningTag(std::string * const tag_name, std::map<std::string, std::string> * const attrib_map,
                         std::string * const error_message);
    bool parseClosingTag(std::string * const tag_name);
};


template <typename DataSource>
const std::deque<int> XMLSubsetParser<DataSource>::CDATA_START_DEQUE{ '<', '!', '[', 'C', 'D', 'A', 'T', 'A', '[' };

// '##' characters denote any byte value except that two consecutive ##s cannot be both 00
// in the case of UTF-8, the 4th byte is ignored
template <typename DataSource>
const uint8_t XMLSubsetParser<DataSource>::FIRST_FOUR_BYTES_WITH_BOM[5][4]{ { 0, 0, 0xFE, 0xFF },
                                                                            { 0xFF, 0xFE, 0, 0 },
                                                                            { 0xFE, 0xFF, /*##*/ 0x0, /*##*/ 0x0 },
                                                                            { 0xFF, 0xFE, /*##*/ 0x0, /*##*/ 0x0 },
                                                                            { 0xEF, 0xBB, 0xBF, /*##*/ 0x0 } };

// Encodings of the first four characters in the XML file (<?xm)
template <typename DataSource>
const uint8_t XMLSubsetParser<DataSource>::FIRST_FOUR_BYTES_NO_BOM[5][4]{
    { 0, 0, 0, 0x3C }, { 0x3C, 0, 0, 0 }, { 0, 0x3C, 0, 0x3F }, { 0x3C, 0, 0x3F, 0 }, { 0x3C, 0x3F, 0x78, 0x6D }
};

template <typename DataSource>
const std::string XMLSubsetParser<DataSource>::CANONICAL_ENCODING_NAMES[5]{ "UTF32BE", "UTF32LE", "UTF16BE", "UTF16LE", "UTF8" };


template <typename DataSource>
XMLSubsetParser<DataSource>::XMLSubsetParser(DataSource * const input, const std::string &external_encoding)
    : input_(input), line_no_(1), last_type_(UNINITIALISED), last_element_was_empty_(false), data_collector_(nullptr),
      datasource_content_start_pos_(0) {
    if (not external_encoding.empty())
        external_encoding_ = external_encoding;

    detectEncoding();
}


template <typename DataSource>
void XMLSubsetParser<DataSource>::detectEncoding() {
    // compare the first 4 bytes with the expected bytes to determine the initial encoding
    char first_four_bytes[4]{ 0 };
    const char null_two_bytes[2]{ 0 };
    for (int i(0); i < 4; ++i) {
        const char byte(input_->get());
        if (byte == EOF)
            LOG_ERROR("Invalid XML file \"" + input_->getPath() + "\". Reached EOF unexpectedly.");
        else
            first_four_bytes[i] = byte;
    }

    uint8_t inital_encoding(UTF32_BE);
    bool has_BOM(true), unknown_encoding(false);
    for (/* intentionally empty */; inital_encoding < Encoding::OTHER; ++inital_encoding) {
        bool found(false);
        has_BOM = true;

        switch (inital_encoding) {
        case UTF16_BE:
        case UTF16_LE:
            if (std::memcmp(first_four_bytes, FIRST_FOUR_BYTES_WITH_BOM[inital_encoding], 2) == 0) {
                if (std::memcmp(first_four_bytes + 2, null_two_bytes, 2) != 0) {
                    found = true;
                }
            }
            break;
        case UTF8:
            if (std::memcmp(first_four_bytes, FIRST_FOUR_BYTES_WITH_BOM[inital_encoding], 3) == 0) {
                found = true;
            }
            break;
        default:
            if (std::memcmp(first_four_bytes, FIRST_FOUR_BYTES_WITH_BOM[inital_encoding], 4) == 0) {
                found = true;
            }
        }

        if (found)
            break;

        has_BOM = false;
        if (std::memcmp(first_four_bytes, FIRST_FOUR_BYTES_NO_BOM[inital_encoding], 4) == 0)
            break;
    }

    // fallback to UTF-8
    if (inital_encoding == Encoding::OTHER) {
        inital_encoding = Encoding::UTF8;
        unknown_encoding = true;
    }

    // attempt to parse the prologue to determine the specified encoding, if any
    switch (inital_encoding) {
    case UTF8:
        to_utf32_decoder_.reset(new TextUtil::UTF8ToUTF32Decoder());
        break;
    default:
        to_utf32_decoder_.reset(new TextUtil::AnythingToUTF32Decoder(TextUtil::CanonizeCharset(CANONICAL_ENCODING_NAMES[inital_encoding])));
    }

    // reset the file pointer while skipping the BOM, if any
    if (has_BOM) {
        datasource_content_start_pos_ = input_->tell();
        if (inital_encoding == Encoding::UTF8) {
            // only put back the last character
            input_->putback(first_four_bytes[3]);
            --datasource_content_start_pos_;
        }
    } else {
        // content starts with the first byte
        datasource_content_start_pos_ = 0;
        input_->rewind();
    }

    parseOptionalPrologue();
    if (not internal_encoding_.empty()) {
        if (not unknown_encoding) {
            if (::strcasecmp(internal_encoding_.c_str(), to_utf32_decoder_->getInputEncoding().c_str()) != 0) {
                LOG_WARNING("Mismatching XML file encoding for \"" + input_->getPath()
                            + "\". Detected: " + to_utf32_decoder_->getInputEncoding() + ", provided (internal): " + internal_encoding_);
            } else if (not external_encoding_.empty() and ::strcasecmp(external_encoding_.c_str(), internal_encoding_.c_str()) != 0) {
                LOG_WARNING("Mismatching XML file encoding for \"" + input_->getPath() + "\". Detected (internal): " + internal_encoding_
                            + ", provided (external): " + external_encoding_);
            }
        }

        to_utf32_decoder_.reset(new TextUtil::AnythingToUTF32Decoder(internal_encoding_));
    } else if (not external_encoding_.empty())
        to_utf32_decoder_.reset(new TextUtil::AnythingToUTF32Decoder(external_encoding_));
    else {
        LOG_WARNING("Couldn't detect XML file encoding for \"" + input_->getPath() + "\". Falling back to UTF-8.");
        to_utf32_decoder_.reset(new TextUtil::UTF8ToUTF32Decoder());
    }
}


template <typename DataSource>
int XMLSubsetParser<DataSource>::getUnicodeCodePoint() {
    int ch(input_->get());
    if (unlikely(ch == EOF))
        return ch;
    for (;;) {
        if (not to_utf32_decoder_->addByte(static_cast<char>(ch)))
            return static_cast<int>(to_utf32_decoder_->getUTF32Char());
        ch = input_->get();
        if (unlikely(ch == EOF))
            throw std::runtime_error(
                "in XMLSubsetParser::getUnicodeCodePoint: unexpected EOF while decoding "
                "a byte sequence!");
    }
}


template <typename DataSource>
int XMLSubsetParser<DataSource>::get(const bool skip_comment, bool * const cdata_start) {
    if (skip_comment) {
        static constexpr char COMMENT_START[]{ "<!--" };
        if (pushed_back_chars_.empty())
            pushed_back_chars_.push_back(getUnicodeCodePoint());
        while (pushed_back_chars_.size() < sizeof(COMMENT_START) - 1 and pushed_back_chars_.back() != EOF)
            pushed_back_chars_.push_back(getUnicodeCodePoint());

        auto pushed_back_char(pushed_back_chars_.cbegin());
        auto cp(COMMENT_START);
        for (;;) {
            if (*cp != *pushed_back_char)
                break;
            ++cp, ++pushed_back_char;
        }
        if (*cp == '\0') {
            for (unsigned i(0); i < sizeof(COMMENT_START) - 1; ++i)
                pushed_back_chars_.pop_front();

            // Skip to end of comment:
            int consecutive_dash_count(0);
            for (;;) {
                int ch = get(/* skip_comment = */ false);
                if (ch == '-')
                    ++consecutive_dash_count;
                else if (unlikely(ch == EOF)) {
                    last_error_message_ = "unexpected EOF while looking for the end of a comment!";
                    return EOF;
                } else {
                    if (ch == '>' and consecutive_dash_count >= 2)
                        break;
                    consecutive_dash_count = 0;
                }
            }
        }
    }

    if (cdata_start != nullptr) {
        // Look for a cached EOF:
        if (std::find(pushed_back_chars_.cbegin(), pushed_back_chars_.cend(), EOF) != pushed_back_chars_.cend())
            *cdata_start = false;
        else {
            while (pushed_back_chars_.size() < __builtin_strlen("<![CDATA[")) {
                const int ch(getUnicodeCodePoint());
                pushed_back_chars_.push_back(ch);
                if (unlikely(ch == EOF)) {
                    *cdata_start = false;
                    break;
                }
            }

            if (pushed_back_chars_ == CDATA_START_DEQUE) {
                pushed_back_chars_.clear();
                *cdata_start = true;
                return EOF;
            }
            *cdata_start = false;
        }
    } else if (pushed_back_chars_.empty())
        pushed_back_chars_.push_back(getUnicodeCodePoint());

    const int ch(pushed_back_chars_.front());
    if (likely(ch != EOF)) {
        if (data_collector_ != nullptr)
            *data_collector_ += TextUtil::UTF32ToUTF8(ch);
        pushed_back_chars_.pop_front();
    }

    return ch;
}


template <typename DataSource>
int XMLSubsetParser<DataSource>::peek() {
    if (pushed_back_chars_.empty())
        pushed_back_chars_.push_back(getUnicodeCodePoint());
    return pushed_back_chars_.front();
}


template <typename DataSource>
void XMLSubsetParser<DataSource>::unget(const int ch) {
    if (unlikely(pushed_back_chars_.size() == __builtin_strlen("<![CDATA[")))
        throw std::runtime_error("in XMLSubsetParser::unget: can't push back more than " + std::to_string(__builtin_strlen("<![CDATA["))
                                 + " characters in a row!");
    pushed_back_chars_.push_front(ch);
    if (data_collector_ != nullptr) {
        if (unlikely(not TextUtil::TrimLastCharFromUTF8Sequence(data_collector_)))
            throw std::runtime_error("in XMLSubsetParser<DataSource>::unget: \"" + *data_collector_ + "\" is an invalid UTF-8 sequence!");
    }
}


template <typename DataSource>
void XMLSubsetParser<DataSource>::skipWhiteSpace() {
    for (;;) {
        const int ch(get());
        if (unlikely(ch == EOF))
            return;
        if (ch != ' ' and ch != '\t' and ch != '\n' and ch != '\r') {
            unget(ch);
            return;
        } else if (ch == '\n' and line_no_ != 0)
            ++line_no_;
    }
}


// \return If true, we have a valid "name" and "value".  If false we haven't found a name/value-pair if
//         *error_message is empty or we have a real problem if *error_message is not empty.
template <typename DataSource>
bool XMLSubsetParser<DataSource>::extractAttribute(std::string * const name, std::string * const value, std::string * const error_message) {
    error_message->clear();

    skipWhiteSpace();
    if (not extractName(name))
        return false;

    skipWhiteSpace();
    const int ch(get(/* skip_comment = */ false));
    if (unlikely(ch != '=')) {
        *error_message = "Could not find an equal sign as part of an attribute.";
        return false;
    }

    skipWhiteSpace();
    const int quote(get(/* skip_comment = */ false));
    if (unlikely(quote != '"' and quote != '\'')) {
        *error_message = "Found neither a single- nor a double-quote starting an attribute value.";
        return false;
    }
    if (unlikely(not extractQuotedString(quote, value))) {
        *error_message = "Failed to extract the attribute value.";
        return false;
    }

    return true;
}


template <typename DataSource>
void XMLSubsetParser<DataSource>::parseOptionalPrologue() {
    skipWhiteSpace();
    int ch(get(/* skip_comment = */ false));
    if (unlikely(ch != '<') or peek() != '?') {
        unget(ch);
        return;
    }
    get(/* skip_comment = */ false); // Skip over '?'.

    std::string name;
    if (not extractName(&name) or name != "xml")
        throw std::runtime_error("in XMLSubsetParser::parseOptionalPrologue: failed to parse a prologue!");

    std::string attrib_name, attrib_value, error_message;
    while (extractAttribute(&attrib_name, &attrib_value, &error_message) and attrib_name != "encoding")
        skipWhiteSpace();
    if (not error_message.empty())
        throw std::runtime_error("in XMLSubsetParser::parseOptionalPrologue: " + error_message);

    if (attrib_name == "encoding")
        internal_encoding_ = TextUtil::CanonizeCharset(attrib_value);

    while (ch != EOF and ch != '>') {
        if (unlikely(ch == '\n' and line_no_ != 0))
            ++line_no_;
        ch = get(/* skip_comment = */ false);
    }
    skipWhiteSpace();
}


inline bool IsValidElementFirstCharacter(const int ch) {
    return TextUtil::UTF32CharIsAsciiLetter(ch) or ch == '_';
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::extractName(std::string * const name) {
    name->clear();

    int ch(get(/* skip_comment = */ false));
    if (unlikely(ch == EOF or not IsValidElementFirstCharacter(ch))) {
        unget(ch);
        return false;
    }

    *name += static_cast<char>(ch);
    for (;;) {
        ch = get(/* skip_comment = */ false);
        if (unlikely(ch == EOF))
            return false;
        if (not(TextUtil::UTF32CharIsAsciiLetter(ch) or TextUtil::UTF32CharIsAsciiDigit(ch) or ch == '_' or ch == ':' or ch == '.'
                or ch == '-')) {
            unget(ch);
            return true;
        }
        *name += static_cast<char>(ch);
    }
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::skipOptionalProcessingInstruction() {
    int ch(get(/* skip_comment = */ false));
    if (ch != '<' or peek() != '?') {
        unget(ch);
        return true;
    }
    get(/* skip_comment = */ false); // Skip over the '?'.

    while ((ch = get(/* skip_comment = */ false)) != '?') {
        if (unlikely(ch == EOF)) {
            last_error_message_ = "unexpected end-of-input while parsing a processing instruction!";
            return false;
        }
    }
    if (unlikely((ch = get(/* skip_comment = */ false)) != '>')) {
        last_error_message_ = "expected '>' at end of a processing instruction!";
        return false;
    }

    return true;
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::extractQuotedString(const int closing_quote, std::string * const s) {
    s->clear();

    for (;;) {
        const int ch(get(/* skip_comment = */ false));
        if (unlikely(ch == EOF))
            return false;
        if (unlikely(ch == closing_quote))
            return true;
        *s += static_cast<char>(ch);
    }
}


// Collects characters while looking for the end of a CDATA section.
template <typename DataSource>
bool XMLSubsetParser<DataSource>::parseCDATA(std::string * const data) {
    int consecutive_closing_bracket_count(0);
    for (;;) {
        const int ch(get(/* skip_comment = */ false));
        if (unlikely(ch == EOF)) {
            last_error_message_ = "Unexpected EOF while looking for the end of CDATA!";
            return false;
        } else if (ch == ']')
            ++consecutive_closing_bracket_count;
        else if (ch == '>') {
            if (consecutive_closing_bracket_count >= 2) {
                data->resize(data->length() - 2); // Trim off the last 2 closing brackets.
                return true;
            }
            consecutive_closing_bracket_count = 0;
        } else {
            if (unlikely(ch == '\n' and line_no_ != 0))
                ++line_no_;
            consecutive_closing_bracket_count = 0;
        }
        *data += TextUtil::UTF32ToUTF8(ch);
    }
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::getNext(Type * const type, std::map<std::string, std::string> * const attrib_map,
                                          std::string * const data) {
    if (unlikely(last_type_ == ERROR))
        throw std::runtime_error("in XMLSubsetParser::getNext: previous call already indicated an error!");

    attrib_map->clear();
    data->clear();

    if (last_element_was_empty_) {
        last_type_ = *type = CLOSING_TAG;
        data->swap(last_tag_name_);
        last_element_was_empty_ = false;
        last_type_ = CLOSING_TAG;
        return true;
    }

    int ch;
    if (last_type_ == OPENING_TAG) {
        last_type_ = *type = CHARACTERS;

collect_next_character:
        bool cdata_start(false);
        while ((ch = get(/* skip_comment = */ true, &cdata_start)) != '<') {
            if (cdata_start) {
                std::string cdata;
                if (not parseCDATA(&cdata))
                    return false;
                data->append(XmlUtil::XmlEscape(cdata));
            } else {
                if (unlikely(ch == EOF)) {
                    last_error_message_ = "Unexpected EOF while looking for the start of a closing tag!";
                    return false;
                }
                if (unlikely(ch == '\n' and line_no_ != 0))
                    ++line_no_;
                *data += TextUtil::UTF32ToUTF8(ch);
            }
        }
        const int lookahead(peek());
        if (likely(lookahead != EOF) and unlikely(lookahead != '/' and not IsValidElementFirstCharacter(lookahead))) {
            *data += '<';
            goto collect_next_character;
        }
        unget(ch); // Putting back the '<'.

        if (not XmlUtil::DecodeEntities(data)) {
            last_type_ = *type = ERROR;
            last_error_message_ = "Invalid entity in character data ending on line " + std::to_string(line_no_) + "!";
            return false;
        }
    } else { // end-of-document or opening or closing tag or more Characters
        if (not skipOptionalProcessingInstruction()) {
            *type = ERROR;
            return false;
        }

        // HACK! skipping Characters/everything in-between a closing tag and an opening tag
        while (true) {
            skipWhiteSpace();
            ch = get();
            if (ch == '<') {
                if (peek() == '!') {
                    // ensure that the comment gets skipped correctly
                    unget(ch);
                    ch = get();
                    continue;
                } else
                    break;
            }
            if (unlikely(ch == EOF)) {
                last_type_ = *type = END_OF_DOCUMENT;
                return true;
            }
        }

        // If we're at the beginning, we may have an XML prolog:
        if (unlikely(last_type_ == UNINITIALISED) and peek() == '?') {
            unget(ch);
            parseOptionalPrologue();
            last_type_ = *type = START_OF_DOCUMENT;
            return true;
        }

        ch = get();
        if (ch == '/') { // A closing tag.
            if (unlikely(not parseClosingTag(data))) {
                last_type_ = *type = ERROR;
                last_error_message_ = "Error while parsing a closing tag on line " + std::to_string(line_no_) + "!";
                return false;
            }

            last_type_ = *type = CLOSING_TAG;
        } else { // An opening tag.
            unget(ch);

            std::string error_message;
            if (unlikely(not parseOpeningTag(data, attrib_map, &error_message))) {
                last_type_ = *type = ERROR;
                last_error_message_ =
                    "Error while parsing an opening tag on line " + std::to_string(line_no_) + "! (" + error_message + ")";
                return false;
            }

            ch = get();
            if (ch == '/') {
                last_element_was_empty_ = true;
                last_tag_name_ = *data;
                ch = get();
            }

            if (unlikely(ch != '>')) {
                last_type_ = *type = ERROR;
                last_error_message_ = "Error while parsing an opening tag on line " + std::to_string(line_no_) + "! ("
                                      "Closing angle bracket not found.)";
                return false;
            }

            last_type_ = *type = OPENING_TAG;
        }
    }

    return true;
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::skipTo(const Type expected_type, const std::vector<std::string> &expected_tags,
                                         std::string * const found_tag, std::map<std::string, std::string> * const attrib_map,
                                         std::string * const data) {
    if (unlikely((expected_type == OPENING_TAG or expected_type == CLOSING_TAG) and expected_tags.empty()))
        throw std::runtime_error(
            "in XMLSubsetParser::skipTo: \"expected_type\" is OPENING_TAG or CLOSING_TAG but no "
            "tag names have been specified!");

    if (data != nullptr)
        data_collector_ = data;
    for (;;) {
        Type type;
        static std::map<std::string, std::string> local_attrib_map;
        std::string data2;
        if (unlikely(not getNext(&type, attrib_map == nullptr ? &local_attrib_map : attrib_map, &data2)))
            throw std::runtime_error("in XMLSubsetParser::skipTo: " + last_error_message_);

        if (expected_type == type) {
            if (expected_type == OPENING_TAG or expected_type == CLOSING_TAG) {
                const auto found(std::find(expected_tags.cbegin(), expected_tags.cend(), data2));
                if (found != expected_tags.cend()) {
                    *found_tag = *found;
                    data_collector_ = nullptr;
                    return true;
                }
            } else {
                data_collector_ = nullptr;
                return true;
            }
        } else if (type == END_OF_DOCUMENT) {
            data_collector_ = nullptr;
            return false;
        }
    }
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::skipTo(const Type expected_type, const std::string &expected_tag,
                                         std::map<std::string, std::string> * const attrib_map, std::string * const data) {
    std::string found_tag;
    return skipTo(expected_type, { expected_tag }, &found_tag, attrib_map, data);
}


template <typename DataSource>
void XMLSubsetParser<DataSource>::rewind() {
    input_->rewind();
    if (datasource_content_start_pos_ != 0)
        input_->seek(datasource_content_start_pos_);

    line_no_ = 1;
    last_type_ = UNINITIALISED;
    last_element_was_empty_ = false;
    data_collector_ = nullptr;
    pushed_back_chars_.clear();

    parseOptionalPrologue();
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::seek(const off_t offset, const int whence) {
    line_no_ = 0;
    last_type_ = UNINITIALISED;
    last_element_was_empty_ = false;
    pushed_back_chars_.clear();

    return input_->seek(offset, whence);
}


template <typename DataSource>
off_t XMLSubsetParser<DataSource>::tell() const {
    return input_->tell() - pushed_back_chars_.size();
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::parseOpeningTag(std::string * const tag_name, std::map<std::string, std::string> * const attrib_map,
                                                  std::string * const error_message) {
    attrib_map->clear();
    error_message->clear();

    if (unlikely(not extractName(tag_name))) {
        *error_message = "Failed to extract the tag name.";
        return false;
    }
    skipWhiteSpace();

    std::string attrib_name, attrib_value;
    while (extractAttribute(&attrib_name, &attrib_value, error_message)) {
        if (unlikely(attrib_map->find(attrib_name) != attrib_map->cend())) { // Duplicate attribute name?
            *error_message = "Found a duplicate attribute name.";
            return false;
        }

        (*attrib_map)[attrib_name] = attrib_value;

        skipWhiteSpace();
    }
    if (unlikely(not error_message->empty()))
        return false;

    return true;
}


template <typename DataSource>
bool XMLSubsetParser<DataSource>::parseClosingTag(std::string * const tag_name) {
    tag_name->clear();

    if (not extractName(tag_name))
        return false;

    skipWhiteSpace();
    return get() == '>';
}


template <typename DataSource>
std::string XMLSubsetParser<DataSource>::TypeToString(const Type type) {
    switch (type) {
    case UNINITIALISED:
        return "UNINITIALISED";
    case START_OF_DOCUMENT:
        return "START_OF_DOCUMENT";
    case END_OF_DOCUMENT:
        return "END_OF_DOCUMENT";
    case ERROR:
        return "ERROR";
    case OPENING_TAG:
        return "OPENING_TAG";
    case CLOSING_TAG:
        return "CLOSING_TAG";
    case CHARACTERS:
        return "CHARACTERS";
    }

    __builtin_unreachable();
}
