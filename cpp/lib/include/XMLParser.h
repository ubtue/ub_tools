/** \file   XMLParser.h
 *  \brief  Wrapper class for Xerces XML parser
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <exception>
#include <map>
#include <memory>
#include <deque>
#include <set>
#include <string>
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
        void resetErrors() {}
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
    ~XMLParser() { delete parser_; delete handler_; delete error_handler_; xercesc::XMLPlatformUtils::Terminate(); }
    void rewind();

    bool peek(XMLPart * const xml_part);

    /** \brief  seeks for the given offset in the underlying string or file.
     *  \throws XMLParser::Error if offset cannot be found or there is no XMLPart that starts exactly at the given offset.
     */
    void seek(const off_t offset, const int whence = SEEK_SET);

    off_t tell();

    inline unsigned getLineNo() { return static_cast<unsigned>(locator_->getLineNumber()); }
    inline unsigned getColumnNo() { return static_cast<unsigned>(locator_->getColumnNumber()); }


    /** \return true if there are more elements to parse, o/w false.
     *  \note   parsing is done in progressive mode, meaning that the document is
     *          still being parsed during consecutive getNext() calls.
     *  \throws XMLParser::Error
     */
    bool getNext(XMLPart * const next, bool combine_consecutive_characters = true);


    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tags  If "type" is OPENING_TAG or CLOSING_TAG, the name of the tags we're looking for.  We return when we
     *                        have found the first matching one.
     *  \param part           If not NULL, the found XMLPart will be returned here.
     *  \param skipped_data   If not NULL, the skipped over XML data will be appended here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    bool skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags,
                XMLPart * const part = nullptr, std::string * const skipped_data = nullptr);

    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tag   If "type" is OPENING_TAG or CLOSING_TAG, the name of the tag we're looking for.
     *  \param part           If not NULL, the found XMLPart will be returned here.
     *  \param skipped_data   If not NULL, the skipped over XML data will be appended here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    inline bool skipTo(const XMLPart::Type expected_type, const std::string &expected_tag,
                XMLPart * const part = nullptr, std::string * const skipped_data = nullptr)
        { return skipTo(expected_type, std::set<std::string>{ expected_tag }, part, skipped_data); }
};
