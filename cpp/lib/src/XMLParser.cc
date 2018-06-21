/** \file   XMLParser.cc
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
#include "XMLParser.h"
#include <xercesc/framework/MemBufInputSource.hpp>


const XMLParser::Options XMLParser::DEFAULT_OPTIONS {
    /* do_namespaces_ = */false,
    /* do_schema_ = */false,
};


std::string XMLParser::ToString(const XMLCh * const xmlch) {
    return xercesc::XMLString::transcode(xmlch);
}


std::string XMLParser::XMLPart::TypeToString(const Type type) {
    switch (type) {
    case UNINITIALISED:
        return "UNINITIALISED";
    case OPENING_TAG:
        return "OPENING_TAG";
    case CLOSING_TAG:
        return "CLOSING_TAG";
    case CHARACTERS:
        return "CHARACTERS";
        throw std::runtime_error("in XMLParser::XMLPart::TypeToString: we should never get here!");
    };
}


void XMLParser::Handler::startElement(const XMLCh * const name, xercesc::AttributeList &attributes) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::OPENING_TAG;
    xml_part.data_ = XMLParser::ToString(name);
    for (XMLSize_t i = 0; i < attributes.getLength(); i++)
        xml_part.attributes_[XMLParser::ToString(attributes.getName(i))] = XMLParser::ToString(attributes.getValue(i));
    parser_->addToBuffer(xml_part);
}


void XMLParser::Handler::endElement(const XMLCh * const name) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CLOSING_TAG;
    xml_part.data_ = XMLParser::ToString(name);
    parser_->addToBuffer(xml_part);
}


void XMLParser::Handler::characters(const XMLCh * const chars, const XMLSize_t /*length*/) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CHARACTERS;
    xml_part.data_ = XMLParser::ToString(chars);
    parser_->addToBuffer(xml_part);
}


void XMLParser::Handler::ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length) {
    characters(chars, length);
}


XMLParser::XMLParser(const std::string &xml_file_or_string, const Type type, const Options &options) {
    xml_file_or_string_ = xml_file_or_string;
    type_ = type;
    options_ = options;
    rewind();
}


void XMLParser::rewind() {
    xercesc::XMLPlatformUtils::Initialize();
    parser_  = new xercesc::SAXParser();

    handler_ = new XMLParser::Handler();
    handler_->parser_ = this;
    parser_->setDocumentHandler(handler_);

    error_handler_ = new XMLParser::ErrorHandler();
    parser_->setErrorHandler(error_handler_);

    parser_->setDoNamespaces(options_.do_namespaces_);
    parser_->setDoSchema(options_.do_schema_);

}


bool XMLParser::getNext(XMLPart * const next) {
    if (not prolog_parsing_done_) {

        if (type_ == FILE) {
            body_has_more_contents_ = parser_->parseFirst(xml_file_or_string_.c_str(), token_);
            if (not body_has_more_contents_)
                LOG_ERROR("error parsing document header: " + xml_file_or_string_);
        } else if (type_ == STRING) {
            xercesc::MemBufInputSource input_buffer((const XMLByte*)xml_file_or_string_.c_str(), xml_file_or_string_.size(),
                                     "xml_string (in memory)");
            body_has_more_contents_ = parser_->parseFirst(input_buffer, token_);
            if (not body_has_more_contents_)
                LOG_ERROR("error parsing document header: " + xml_file_or_string_);
        } else
            LOG_ERROR("Undefined XMLParser::Type!");

        prolog_parsing_done_ = true;
    }

    if (buffer_.empty() && body_has_more_contents_)
        body_has_more_contents_ = parser_->parseNext(token_);

    if (not buffer_.empty()) {
        XMLPart current(*next);
        *next = buffer_.front();
        buffer_.pop();
    }

    return (not buffer_.empty() or body_has_more_contents_);
}


bool XMLParser::skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags,
                       XMLPart * const part)
{
    while (getNext(part)) {
        if (part->type_ == expected_type) {
            if ((expected_type == XMLPart::OPENING_TAG or expected_type == XMLPart::CLOSING_TAG)) {
                if (expected_tags.empty())
                    return true;
                else if (expected_tags.find(part->data_) != expected_tags.end())
                    return true;
            } else
                return true;
        }
    }
    return false;
}


bool XMLParser::skipTo(const XMLPart::Type expected_type, const std::string &expected_tag,
                       XMLPart * const part)
{
    std::set<std::string> expected_tags;
    if (expected_tag != "")
        expected_tags.emplace(expected_tag);
    return skipTo(expected_type, expected_tags, part);
}
