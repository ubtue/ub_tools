/** \file   XMLParser.h
 *  \brief  Wrapper class for Xerces XML parser
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <deque>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <xercesc/framework/XMLPScanToken.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/Locator.hpp>
#include <xercesc/util/XMLString.hpp>
#include "util.h"


class XMLParser final {
    xercesc::SAXParser *parser_;
    xercesc::XMLPScanToken token_;
    const xercesc::Locator *locator_;
    std::string xml_filename_or_string_;
    bool prolog_parsing_done_ = false;
    bool body_has_more_contents_;
    unsigned open_elements_;
    std::unordered_map<std::string, std::string> tag_aliases_to_canonical_tags_map_;

public:
    class Error : public std::runtime_error {
    public:
        explicit Error(const std::string &message): std::runtime_error(message) { }
    };
    typedef std::map<std::string, std::string> Attributes;

    enum Type { XML_FILE, XML_STRING };

    struct Options {
        /** \brief Parser enforces all the constraints / rules specified by the NameSpace specification (default false).*/
        bool do_namespaces_;
        /** \brief Found Schema information will only be processed if set to true (default false).*/
        bool do_schema_;
        /** \brief Defines if CHARACTERS that only contain whitespaces will be skipped (default true). */
        bool ignore_whitespace_;
        /** \brief When an external DTD is referenced load it (default false). */
        bool load_external_dtds_;
    };

    static const Options DEFAULT_OPTIONS;

    struct XMLPart {
        enum Type { UNINITIALISED, OPENING_TAG, CLOSING_TAG, CHARACTERS };
        static std::string TypeToString(const Type type);
        Type type_ = UNINITIALISED;
        std::string data_;
        Attributes attributes_;
        off_t offset_;
        std::string toString();

        inline bool isOpeningTag() const { return type_ == OPENING_TAG; }
        inline bool isOpeningTag(const std::string &tag) const { return type_ == OPENING_TAG and data_ == tag; }
        inline bool isClosingTag() const { return type_ == CLOSING_TAG; }
        inline bool isClosingTag(const std::string &tag) const { return type_ == CLOSING_TAG and data_ == tag; }
        inline bool isCharacters() const { return type_ == CHARACTERS; }
    };

private:
    Type type_;
    Options options_;

    static void ConvertAndThrowException(const xercesc::RuntimeException &exc);
    static void ConvertAndThrowException(const xercesc::SAXParseException &exc);
    static void ConvertAndThrowException(const xercesc::XMLException &exc);

    class Handler : public xercesc::HandlerBase {
        friend class XMLParser;
        XMLParser *parser_;
        inline off_t getOffset() { return static_cast<off_t>(parser_->parser_->getSrcOffset()); }

    public:
        void characters(const XMLCh * const chars, const XMLSize_t length);
        void endElement(const XMLCh * const name);
        void ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length);
        void setDocumentLocator(const xercesc::Locator * const locator);
        void startElement(const XMLCh * const name, xercesc::AttributeList &attributes);
    };

    class ErrorHandler : public xercesc::ErrorHandler {
        std::string getExceptionMessage(const xercesc::SAXParseException &exc) {
            std::string message;
            message += "line " + std::to_string(exc.getLineNumber());
            message += ", col " + std::to_string(exc.getColumnNumber());
            message += ": " + XMLParser::ToStdString(exc.getMessage());
            return message;
        }

    public:
        void warning(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToStdString(exc.getMessage())); }
        void error(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToStdString(exc.getMessage())); }
        void fatalError(const xercesc::SAXParseException &exc) { XMLParser::ConvertAndThrowException(exc); }
        void resetErrors() { }
    };

    Handler *handler_;
    ErrorHandler *error_handler_;
    std::deque<XMLPart> buffer_;
    inline void appendToBuffer(XMLPart &xml_part) { buffer_.emplace_back(xml_part); }

    /** \brief depending on type_: returns either length of the file or length of the string.*/
    off_t getMaxOffset();

    friend class Handler;

    /** \brief  converts xerces' internal string type to std::string. */
    static std::string ToStdString(const XMLCh * const xmlch);

public:
    explicit XMLParser(const std::string &xml_filename_or_string, const Type type, const Options &options = DEFAULT_OPTIONS);
    ~XMLParser();

    /** \brief Restarts parsing of a new file or string. */
    void reset(const std::string &xml_filename_or_string, const Type type, const Options &options = DEFAULT_OPTIONS);

    inline void rewind() { reset(xml_filename_or_string_, type_, options_); }

    bool peek(XMLPart * const xml_part);

    /** \brief  seeks for the given offset in the underlying string or file.
     *  \throws XMLParser::Error if offset cannot be found or there is no XMLPart that starts exactly at the given offset.
     */
    void seek(const off_t offset, const int whence = SEEK_SET);

    off_t tell();

    inline std::string getXmlFilenameOrString() const { return xml_filename_or_string_; }
    inline unsigned getLineNo() const { return static_cast<unsigned>(locator_->getLineNumber()); }
    inline unsigned getColumnNo() const { return static_cast<unsigned>(locator_->getColumnNumber()); }

    /** \brief Add a mapping for tag names.
     *  \note After a call to this function, keys and values in "tag_aliases_to_canonical_tags_map" will be considered as equivalent.
     *        All returned tag names will be the canonical names.
     */
    inline void setTagAliases(const std::unordered_map<std::string, std::string> &tag_aliases_to_canonical_tags_map) {
        tag_aliases_to_canonical_tags_map_ = tag_aliases_to_canonical_tags_map;
    }

    /** \return true if there are more elements to parse, o/w false.
     *  \param  guard_opening_tags  Contains a set of opening tags.  If any of these tags is encountered, parsing will stop and this
     *                              function returns false.  The next call will then return the previously encountered guard opening tag.
     *  \note   parsing is done in progressive mode, meaning that the document is
     *          still being parsed during consecutive getNext() calls.
     *  \throws XMLParser::Error
     */
    bool getNext(XMLPart * const next, const bool combine_consecutive_characters = true,
                 const std::set<std::string> &guard_opening_tags = {});

    inline bool getNext(XMLPart * const next, const std::set<std::string> &guard_opening_tags,
                        const bool combine_consecutive_characters = true) {
        return getNext(next, combine_consecutive_characters, guard_opening_tags);
    }


    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tags  If "type" is OPENING_TAG or CLOSING_TAG, the name of the tags we're looking for.
     *                        We return when we have found the first matching one.
     *                        If empty, we return on any tag.
     *  \param part           If not NULL, the found XMLPart will be returned here.
     *  \param skipped_data   If not NULL, the skipped over XML data will be appended here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    bool skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags = {}, XMLPart * const part = nullptr,
                std::string * const skipped_data = nullptr);

    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tag   If "type" is OPENING_TAG or CLOSING_TAG, the name of the tag we're looking for.
     *                        If empty, we return on any tag.
     *  \param part           If not NULL, the found XMLPart will be returned here.
     *  \param skipped_data   If not NULL, the skipped over XML data will be appended here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    inline bool skipTo(const XMLPart::Type expected_type, const std::string &expected_tag = "", XMLPart * const part = nullptr,
                       std::string * const skipped_data = nullptr) {
        if (expected_tag.empty())
            return skipTo(expected_type, std::set<std::string>{}, part, skipped_data);
        else
            return skipTo(expected_type, std::set<std::string>{ expected_tag }, part, skipped_data);
    }

    /** \brief  Extracts text between an opening and closing tag pair.
     *  \param  tag   The opening and closing tag name.
     *  \param  text  The extracted text.
     *  \param  guard_tags  If not empty, we give up looking for "tag" if we see any of these tags but we do not skip over them.
     *  \return True if we found the opening and closing "tag", o/w false.
     *  \note   This function does not deal well with tags that are nested, e.g. <x><x>text</x></x>.  Here we'd return after having
     *          processed the first closing x.
     */
    bool extractTextBetweenTags(const std::string &tag, std::string * const text, const std::set<std::string> &guard_tags = {});
};
