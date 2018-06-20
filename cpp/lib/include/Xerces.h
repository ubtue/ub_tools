/** \file   Xerces.h
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

#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <xercesc/framework/XMLPScanToken.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include "util.h"

class Xerces {
    xercesc::SAXParser * parser_;
    xercesc::XMLPScanToken token_;
    std::string xml_file_;
    bool prolog_parsing_done_ = false;
    bool body_has_more_contents_;
public:
    struct XmlPart {
        enum Type { START_ELEMENT, END_ELEMENT, PROCESSING_INSTRUCTION, CHARACTERS, IGNORABLE_WHITESPACE };
        virtual Type getType() const = 0;
        virtual ~XmlPart() = default;
        static std::string TypeToString(const Type type);
    };

    struct StartElement : XmlPart {
        std::string name_;
        std::vector<std::pair<std::string, std::string>> attributes_;
        inline virtual Type getType() const override { return START_ELEMENT; }
    };

    struct EndElement : XmlPart {
        std::string name_;
        inline virtual Type getType() const override { return END_ELEMENT; }
    };

    struct ProcessingInstruction : XmlPart {
        std::string target_;
        std::string data_;
        inline virtual Type getType() const override { return PROCESSING_INSTRUCTION; }
    };

    struct Characters : XmlPart {
        std::string chars_;
        inline virtual Type getType() const override { return CHARACTERS; }
    };

    struct IgnorableWhitespace : Characters {
        inline virtual Type getType() const override { return IGNORABLE_WHITESPACE; }
    };

private:
    template<typename XmlPartType> static const std::shared_ptr<const XmlPartType> CastToXmlPartOrDie(const std::shared_ptr<const XmlPart> part, const XmlPart::Type part_type) {
        if (unlikely(part->getType() != part_type))
            LOG_ERROR("Could not convert XmlPart to " + XmlPart::TypeToString(part_type));
        return std::static_pointer_cast<const XmlPartType>(part);
    }
public:
    static const std::shared_ptr<const StartElement> CastToStartElementOrDie(const std::shared_ptr<const XmlPart> part) { return CastToXmlPartOrDie<StartElement>(part, XmlPart::START_ELEMENT); }
    static const std::shared_ptr<const EndElement> CastToEndElementOrDie(const std::shared_ptr<const XmlPart> part) { return CastToXmlPartOrDie<EndElement>(part, XmlPart::END_ELEMENT); }
    static const std::shared_ptr<const ProcessingInstruction> CastToProcessingInstructionOrDie(const std::shared_ptr<const XmlPart> part) { return CastToXmlPartOrDie<ProcessingInstruction>(part, XmlPart::PROCESSING_INSTRUCTION); }
    static const std::shared_ptr<const Characters> CastToCharactersOrDie(const std::shared_ptr<const XmlPart> part) { return CastToXmlPartOrDie<Characters>(part, XmlPart::CHARACTERS); }
    static const std::shared_ptr<const IgnorableWhitespace> CastToIgnorableWhitespaceOrDie(const std::shared_ptr<const XmlPart> part) { return CastToXmlPartOrDie<IgnorableWhitespace>(part, XmlPart::IGNORABLE_WHITESPACE); }
private:
    class Handler : public xercesc::HandlerBase {
        friend class Xerces;
        Xerces * xerces_;

    public:
        void characters(const XMLCh * const chars, const XMLSize_t length);
        void endElement(const XMLCh * const name);
        void ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length);
        void processingInstruction(const XMLCh * const target, const XMLCh *const data);
        void startElement(const XMLCh * const name, xercesc::AttributeList &attributes);
    };

    class ErrorHandler : public xercesc::ErrorHandler {
    public:
        void warning(const xercesc::SAXParseException &exc) { LOG_WARNING(Xerces::ToString(exc.getMessage())); }
        void error(const xercesc::SAXParseException &exc) { LOG_WARNING(Xerces::ToString(exc.getMessage())); }
        void fatalError(const xercesc::SAXParseException &exc) { LOG_ERROR(Xerces::ToString(exc.getMessage())); }
        void resetErrors() {}
    };

    Handler* handler_;
    ErrorHandler* error_handler_;
    std::queue<std::shared_ptr<XmlPart>> buffer_;
    void addToBuffer(std::shared_ptr<XmlPart> xml_part) { buffer_.push(xml_part); }
    friend class Handler;
public:
    Xerces(const std::string &xml_file);
    ~Xerces() = default;

    /** \brief  converts xerces' internal string type to std::string. */
    static std::string ToString(const XMLCh * const xmlch);

    /** \return true if there are more elements to parse, o/w false.
     *  \note   parsing is done in progressive mode, meaning that the document is
     *          still being parsed during consecutive getNext() calls.
     *  \throws xerces might throw exceptions, e.g. xercesc::RuntimeException.
     */
    bool getNext(std::shared_ptr<XmlPart> &next);
};
