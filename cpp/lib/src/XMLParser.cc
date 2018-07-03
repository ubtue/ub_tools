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
#include "FileUtil.h"
#include "StringUtil.h"
#include "XmlUtil.h"


const XMLParser::Options XMLParser::DEFAULT_OPTIONS {
    /* convert_to_iso8859_15_ = */false,
    /* do_namespaces_ = */false,
    /* do_schema_ = */false,
    /* ignore_whitespace_ = */true,
};


void XMLParser::ConvertAndThrowException(const xercesc::RuntimeException &exc) {
    throw XMLParser::RuntimeError("Xerces RuntimeException: " + ToString(exc.getMessage()));
}


void XMLParser::ConvertAndThrowException(const xercesc::SAXParseException &exc) {
    throw XMLParser::RuntimeError("Xerces SAXParseException on line " + std::to_string(exc.getLineNumber()) + ": " + ToString(exc.getMessage()));
}


void XMLParser::ConvertAndThrowException(const xercesc::XMLException &exc) {
    throw XMLParser::RuntimeError("Xerces XMLException on line " + std::to_string(exc.getSrcLine()) + ": " + ToString(exc.getMessage()));
}


std::string XMLParser::ToString(const XMLCh * const xmlch) {
    return xercesc::XMLString::transcode(xmlch);
}


std::string XMLParser::XMLPart::toString() {
    switch (type_) {
    case UNINITIALISED:
        return "<<<UNINITIALISED>>>";
    case OPENING_TAG: {
        std::string xml_string("<" + data_);
        for (const auto &attribute : attributes_)
            xml_string += " " + attribute.first + "=\"" + XmlUtil::XmlEscape(attribute.second) + "\"";
        xml_string += ">";
        return xml_string;
    }
    case CLOSING_TAG:
        return "</" + data_ + ">";
    case CHARACTERS:
        return XmlUtil::XmlEscape(data_);
    }
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
    };
}


void XMLParser::Handler::startElement(const XMLCh * const name, xercesc::AttributeList &attributes) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::OPENING_TAG;

    if (parser_->options_.convert_to_iso8859_15_)
        xml_part.data_ = StringUtil::UTF8ToISO8859_15(XMLParser::ToString(name));
    else
        xml_part.data_ = XMLParser::ToString(name);

    for (XMLSize_t i = 0; i < attributes.getLength(); i++) {
        std::string attrib_name(XMLParser::ToString(attributes.getName(i))), attrib_value(XMLParser::ToString(attributes.getValue(i)));
        if (parser_->options_.convert_to_iso8859_15_) {
            attrib_name = StringUtil::UTF8ToISO8859_15(attrib_name);
            attrib_value = StringUtil::UTF8ToISO8859_15(attrib_value);
        }
        xml_part.attributes_[attrib_name] = attrib_value;
    }
    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
    ++parser_->open_elements_;
}


void XMLParser::Handler::endElement(const XMLCh * const name) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CLOSING_TAG;

    if (parser_->options_.convert_to_iso8859_15_)
        xml_part.data_ = StringUtil::UTF8ToISO8859_15(XMLParser::ToString(name));
    else
        xml_part.data_ = XMLParser::ToString(name);

    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
    --parser_->open_elements_;
}


void XMLParser::Handler::characters(const XMLCh * const chars, const XMLSize_t /*length*/) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CHARACTERS;

    if (parser_->options_.convert_to_iso8859_15_)
        xml_part.data_ = StringUtil::UTF8ToISO8859_15(XMLParser::ToString(chars));
    else
        xml_part.data_ = XMLParser::ToString(chars);

    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
}


void XMLParser::Handler::ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length) {
    characters(chars, length);
}


void XMLParser::Handler::setDocumentLocator(const xercesc::Locator * const locator) {
    parser_->locator_ = locator;
}


XMLParser::XMLParser(const std::string &xml_filename_or_string, const Type type, const Options &options) {
    xml_filename_or_string_ = xml_filename_or_string;
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
    parser_->setCalculateSrcOfs(true);

    open_elements_ = 0;
    locator_ = nullptr;
    prolog_parsing_done_ = false;
}


XMLParser::XMLPart XMLParser::peek() {
    XMLParser::XMLPart xml_part;
    if (getNext(&xml_part)) {
        buffer_.emplace_front(xml_part);
        return xml_part;
    }

    throw XMLParser::RuntimeError("peek not possible! (end of file)");
}


void XMLParser::seek(const off_t offset, const int whence) {
    if (whence == SEEK_SET) {
        rewind();
        XMLPart xml_part;
        while(getNext(&xml_part)) {
            if (xml_part.offset_ == offset) {
                buffer_.emplace_front(xml_part);
                return;
            } else if (xml_part.offset_ > offset)
                throw XMLParser::RuntimeError("no element found at offset: " + std::to_string(offset));
        }
    } else if (whence == SEEK_CUR)
        return seek(parser_->getSrcOffset() + offset, SEEK_SET);
    else {
        off_t size(FileUtil::GetFileSize(xml_filename_or_string_));
        return seek(size + offset, SEEK_SET);
    }

    throw XMLParser::RuntimeError("offset not found: " + std::to_string(offset));
}


bool XMLParser::getNext(XMLPart * const next, bool combine_consecutive_characters) {
    try {
        if (not prolog_parsing_done_) {
            if (type_ == XML_FILE) {
                body_has_more_contents_ = parser_->parseFirst(xml_filename_or_string_.c_str(), token_);
                if (not body_has_more_contents_)
                    throw XMLParser::RuntimeError("error parsing document header: " + xml_filename_or_string_);
            } else if (type_ == XML_STRING) {
                xercesc::MemBufInputSource input_buffer((const XMLByte*)xml_filename_or_string_.c_str(), xml_filename_or_string_.size(),
                                                        "xml_string (in memory)");
                body_has_more_contents_ = parser_->parseFirst(input_buffer, token_);
                if (not body_has_more_contents_)
                    throw XMLParser::RuntimeError("error parsing document header: " + xml_filename_or_string_);
            } else
                throw XMLParser::RuntimeError("Undefined XMLParser::Type!");

            prolog_parsing_done_ = true;
        }

        if (buffer_.empty() && body_has_more_contents_)
            body_has_more_contents_ = parser_->parseNext(token_);

        if (not buffer_.empty()) {
            buffer_.front();
            if (next != nullptr)
                *next = buffer_.front();
            buffer_.pop_front();
            if (next != nullptr and next->type_ == XMLPart::CHARACTERS and combine_consecutive_characters) {
                XMLPart peek;
                while(getNext(&peek, /* combine_consecutive_characters = */false) and peek.type_ == XMLPart::CHARACTERS)
                    next->data_ += peek.data_;
                if (peek.type_ != XMLPart::CHARACTERS) {
                    buffer_.emplace_front(peek);
                }
            }

            if (options_.ignore_whitespace_ and next != nullptr and next->type_ == XMLPart::CHARACTERS and StringUtil::IsWhitespace(next->data_))
                return getNext(next);
        }
    } catch (xercesc::RuntimeException &exc) {
        ConvertAndThrowException(exc);
    }

    return (not buffer_.empty() or body_has_more_contents_);
}


bool XMLParser::skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags,
                       XMLPart * const part, std::string * const skipped_data)
{
    XMLPart xml_part;
    bool return_value(false);
    if (skipped_data != nullptr)
        skipped_data->clear();

    while (getNext(&xml_part)) {
        if (xml_part.type_ == expected_type) {
            if (expected_type == XMLPart::OPENING_TAG or expected_type == XMLPart::CLOSING_TAG) {
                if (expected_tags.empty()) {
                    return_value = true;
                    break;
                } else if (expected_tags.find(xml_part.data_) != expected_tags.end()) {
                    return_value = true;
                    break;
                }
            } else {
                return_value = true;
                break;
            }
        }

        if (skipped_data != nullptr)
            *skipped_data += xml_part.toString();
    }

    if (part != nullptr) {
        part->type_ = xml_part.type_;
        part->data_ = xml_part.data_;
        part->attributes_ = xml_part.attributes_;
    }

    return return_value;
}


bool XMLParser::skipTo(const XMLPart::Type expected_type, const std::string &expected_tag, XMLPart * const part,
                       std::string * const skipped_data)
{
    std::set<std::string> expected_tags;
    if (not expected_tag.empty())
        expected_tags.emplace(expected_tag);
    return skipTo(expected_type, expected_tags, part, skipped_data);
}
