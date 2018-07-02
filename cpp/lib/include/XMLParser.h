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


class XMLParser {
    xercesc::SAXParser *parser_;
    xercesc::XMLPScanToken token_;
    const xercesc::Locator *locator_;
    std::string xml_filename_or_string_;
    bool prolog_parsing_done_ = false;
    bool body_has_more_contents_;
    unsigned open_elements_;
public:
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
        std::string toString();
    };
private:
    Type type_;
    Options options_;
    class Handler : public xercesc::HandlerBase {
        friend class XMLParser;
        XMLParser *parser_;

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
            message += ": " + XMLParser::ToString(exc.getMessage());
            return message;
        }
    public:
        void warning(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToString(exc.getMessage())); }
        void error(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToString(exc.getMessage())); }
        void fatalError(const xercesc::SAXParseException &exc) { throw std::runtime_error(getExceptionMessage(exc)); }
        void resetErrors() {}
    };

    Handler *handler_;
    ErrorHandler *error_handler_;
    std::deque<XMLPart> buffer_;
    inline void appendToBuffer(XMLPart &xml_part) { buffer_.emplace_back(xml_part); }
    friend class Handler;

    /** \brief  converts xerces' internal string type to std::string. */
    static std::string ToString(const XMLCh * const xmlch);
public:
    explicit XMLParser(const std::string &xml_filename_or_string, const Type type, const Options &options = DEFAULT_OPTIONS);
    ~XMLParser() = default;
    void rewind();
    unsigned getLineNo() { return static_cast<unsigned>(locator_->getLineNumber()); }
    unsigned getColumnNo() { return static_cast<unsigned>(locator_->getColumnNumber()); }


    /** \return true if there are more elements to parse, o/w false.
     *  \note   parsing is done in progressive mode, meaning that the document is
     *          still being parsed during consecutive getNext() calls.
     *  \throws std::runtime_error, see ErrorHandler.
     */
    bool getNext(XMLPart * const next, bool combine_consecutive_characters = true);


    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tags  If "type" is OPENING_TAG or CLOSING_TAG, the name of the tags we're looking for.  We return when we
     *                        have found the first matching one.
     *  \param part           The found XMLPart will be returned here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    bool skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags,
                XMLPart * const part = nullptr);

    /** \brief Skip forward until we encounter a certain element.
     *  \param expected_type  The type of element we're looking for.
     *  \param expected_tag   If "type" is OPENING_TAG or CLOSING_TAG, the name of the tag we're looking for.
     *  \param part           The found XMLPart will be returned here.
     *  \return False if we encountered END_OF_DOCUMENT before finding what we're looking for, else true.
     */
    inline bool skipTo(const XMLPart::Type expected_type, const std::string &expected_tag,
                XMLPart * const part = nullptr)
        { return skipTo(expected_type, { expected_tag }, part); }

};
