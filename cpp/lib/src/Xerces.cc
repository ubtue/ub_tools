/** \file   Xerces.cc
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
#include "Xerces.h"


std::string Xerces::ToString(const XMLCh * const xmlch) {
    return xercesc::XMLString::transcode(xmlch);
}


std::string Xerces::XmlPart::TypeToString(const Type type) {
    switch (type) {
    case START_ELEMENT:
        return "START_ELEMENT";
    case END_ELEMENT:
        return "END_ELEMENT";
    case PROCESSING_INSTRUCTION:
        return "PROCESSING_INSTRUCTION";
    case CHARACTERS:
        return "CHARACTERS";
    case IGNORABLE_WHITESPACE:
        return "IGNORABLE_WHITESPACE";
        throw std::runtime_error("in Xerces::XmlPart::TypeToString: we should never get here!");
    };
}


void Xerces::Handler::startElement(const XMLCh * const name, xercesc::AttributeList &attributes) {
    StartElement start_element;
    start_element.name_ = Xerces::ToString(name);
    for (XMLSize_t i = 0; i < attributes.getLength(); i++)
        start_element.attributes_.push_back(std::make_pair(Xerces::ToString(attributes.getName(i)), Xerces::ToString(attributes.getValue(i))));
    xerces_->addToBuffer(std::make_shared<StartElement>(start_element));
}


void Xerces::Handler::endElement(const XMLCh * const name) {
    EndElement end_element;
    end_element.name_ = Xerces::ToString(name);
    xerces_->addToBuffer(std::make_shared<EndElement>(end_element));
}


void Xerces::Handler::processingInstruction(const XMLCh * const target, const XMLCh * const data) {
    ProcessingInstruction processing_instruction;
    processing_instruction.target_ = Xerces::ToString(target);
    processing_instruction.data_ = Xerces::ToString(data);
    xerces_->addToBuffer(std::make_shared<ProcessingInstruction>(processing_instruction));
}


void Xerces::Handler::characters(const XMLCh * const chars, const XMLSize_t /*length*/) {
    Characters characters;
    characters.chars_ = Xerces::ToString(chars);
    xerces_->addToBuffer(std::make_shared<Characters>(characters));
}


void Xerces::Handler::ignorableWhitespace(const XMLCh * const chars, const XMLSize_t /*length*/) {
    IgnorableWhitespace ignorable_whitespace;
    ignorable_whitespace.chars_ = Xerces::ToString(chars);
    xerces_->addToBuffer(std::make_shared<IgnorableWhitespace>(ignorable_whitespace));
}


Xerces::Xerces(const std::string &xml_file) {
    xercesc::XMLPlatformUtils::Initialize();
    parser_ = new xercesc::SAXParser();

    handler_ = new Xerces::Handler();
    handler_->xerces_ = this;
    parser_->setDocumentHandler(handler_);

    error_handler_ = new Xerces::ErrorHandler();
    parser_->setErrorHandler(error_handler_);

    xml_file_ = xml_file;
}


bool Xerces::getNext(std::shared_ptr<XmlPart> &next) {
    if (not prolog_parsing_done_) {
        body_has_more_contents_ = parser_->parseFirst(xml_file_.c_str(), token_);
        if (not body_has_more_contents_)
            LOG_ERROR("error parsing document header: " + xml_file_);

        prolog_parsing_done_ = true;
    }

    if (buffer_.empty() && body_has_more_contents_)
        body_has_more_contents_ = parser_->parseNext(token_);

    if (not buffer_.empty()) {
        next = buffer_.front();
        buffer_.pop();
    }

    return (not buffer_.empty() or body_has_more_contents_);
}
