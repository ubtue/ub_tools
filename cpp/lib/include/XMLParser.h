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

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <xercesc/framework/XMLPScanToken.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include "util.h"


class XMLParser {
    xercesc::SAXParser * parser_;
    xercesc::XMLPScanToken token_;
    std::string xml_file_;
    bool prolog_parsing_done_ = false;
    bool body_has_more_contents_;
public:
    typedef std::map<std::string, std::string> Attributes;

    struct Options {
        bool do_namespaces_;
        bool do_schema_;
    };

    static const Options DEFAULT_OPTIONS;

    struct XMLPart {
        enum Type { UNINITIALISED, OPENING_TAG, CLOSING_TAG, CHARACTERS };
        static std::string TypeToString(const Type type);
        Type type_ = UNINITIALISED;
        std::string data_;
        Attributes attributes_;
    };
private:
    Options options_;
    class Handler : public xercesc::HandlerBase {
        friend class XMLParser;
        XMLParser * parser_;

    public:
        void characters(const XMLCh * const chars, const XMLSize_t length);
        void endElement(const XMLCh * const name);
        void ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length);
        void startElement(const XMLCh * const name, xercesc::AttributeList &attributes);
    };

    class ErrorHandler : public xercesc::ErrorHandler {
    public:
        void warning(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToString(exc.getMessage())); }
        void error(const xercesc::SAXParseException &exc) { LOG_WARNING(XMLParser::ToString(exc.getMessage())); }
        void fatalError(const xercesc::SAXParseException &exc) { LOG_ERROR(XMLParser::ToString(exc.getMessage())); }
        void resetErrors() {}
    };

    Handler* handler_;
    ErrorHandler* error_handler_;
    std::queue<XMLPart> buffer_;
    void addToBuffer(XMLPart &xml_part) { buffer_.push(xml_part); }
    friend class Handler;
public:
    XMLParser(const std::string &xml_file, const Options &options = DEFAULT_OPTIONS);
    ~XMLParser() = default;
    void rewind();

    /** \brief  converts xerces' internal string type to std::string. */
    static std::string ToString(const XMLCh * const xmlch);

    /** \return true if there are more elements to parse, o/w false.
     *  \note   parsing is done in progressive mode, meaning that the document is
     *          still being parsed during consecutive getNext() calls.
     *  \throws xerces might throw exceptions, e.g. xercesc::RuntimeException.
     */
    bool getNext(XMLPart * const next);


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
    bool skipTo(const XMLPart::Type expected_type, const std::string &expected_tag = "",
                XMLPart * const part = nullptr);
};
