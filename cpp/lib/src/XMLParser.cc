/** \file   XMLParser.cc
 *  \brief  Wrapper class for Xerces XML parser
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cassert>
#include <xercesc/framework/MemBufInputSource.hpp>
#include "FileUtil.h"
#include "Main.h"
#include "StringUtil.h"
#include "XmlUtil.h"


// Perform process-level init/deinit related to Xerces library.
static int SetupXercesPlatform() {
    RegisterProgramPrologueHandler(/* priority = */ 0, []() -> void { xercesc::XMLPlatformUtils::Initialize(); });

    RegisterProgramEpilogueHandler(/* priority = */ 0, []() -> void { xercesc::XMLPlatformUtils::Terminate(); });

    return 0;
}


const int throwaway(SetupXercesPlatform());


const XMLParser::Options XMLParser::DEFAULT_OPTIONS{
    /* do_namespaces_ = */ false,
    /* do_schema_ = */ false,
    /* ignore_whitespace_ = */ true,
    /* load_external_dtds_ = */ false,
};


void XMLParser::ConvertAndThrowException(const xercesc::RuntimeException &exc) {
    throw XMLParser::Error("Xerces RuntimeException: " + ToStdString(exc.getMessage()));
}


void XMLParser::ConvertAndThrowException(const xercesc::SAXParseException &exc) {
    throw XMLParser::Error("Xerces SAXParseException on line " + std::to_string(exc.getLineNumber()) + ": "
                           + ToStdString(exc.getMessage()));
}


void XMLParser::ConvertAndThrowException(const xercesc::XMLException &exc) {
    throw XMLParser::Error("Xerces XMLException on line " + std::to_string(exc.getSrcLine()) + ": " + ToStdString(exc.getMessage()));
}


std::string XMLParser::ToStdString(const XMLCh * const xmlch) {
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
    default:
        LOG_ERROR("we should *never* get here!");
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
    default:
        LOG_ERROR("we should *never* get here!");
    };
}


void XMLParser::Handler::startElement(const XMLCh * const name, xercesc::AttributeList &attributes) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::OPENING_TAG;
    xml_part.data_ = XMLParser::ToStdString(name);
    for (XMLSize_t i(0); i < attributes.getLength(); ++i)
        xml_part.attributes_[XMLParser::ToStdString(attributes.getName(i))] = XMLParser::ToStdString(attributes.getValue(i));

    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
    ++parser_->open_elements_;
}


void XMLParser::Handler::endElement(const XMLCh * const name) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CLOSING_TAG;
    xml_part.data_ = XMLParser::ToStdString(name);
    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
    --parser_->open_elements_;
}


void XMLParser::Handler::characters(const XMLCh * const chars, const XMLSize_t /*length*/) {
    XMLPart xml_part;
    xml_part.type_ = XMLPart::CHARACTERS;
    xml_part.data_ = XMLParser::ToStdString(chars);
    xml_part.offset_ = getOffset();
    parser_->appendToBuffer(xml_part);
}


void XMLParser::Handler::ignorableWhitespace(const XMLCh * const chars, const XMLSize_t length) {
    characters(chars, length);
}


void XMLParser::Handler::setDocumentLocator(const xercesc::Locator * const locator) {
    parser_->locator_ = locator;
}


XMLParser::XMLParser(const std::string &xml_filename_or_string, const Type type, const Options &options)
    : xml_filename_or_string_(xml_filename_or_string), type_(type), options_(options) {
    parser_ = new xercesc::SAXParser();

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


XMLParser::~XMLParser() {
    delete parser_;
    delete handler_;
    delete error_handler_;
}


void XMLParser::reset(const std::string &xml_filename_or_string, const Type type, const Options &options) {
    xml_filename_or_string_ = xml_filename_or_string;
    type_ = type;
    options_ = options;
    open_elements_ = 0;
    locator_ = nullptr;
    prolog_parsing_done_ = false;
    buffer_.clear();
}


bool XMLParser::peek(XMLPart * const xml_part) {
    if (getNext(xml_part)) {
        buffer_.emplace_front(*xml_part);
        return true;
    } else
        return false;
}


void XMLParser::seek(const off_t offset, const int whence) {
    if (whence == SEEK_SET) {
        if (offset < tell())
            rewind();

        XMLPart xml_part;
        while (getNext(&xml_part)) {
            if (xml_part.offset_ == offset) {
                buffer_.emplace_front(xml_part);
                return;
            } else if (xml_part.offset_ > offset)
                throw XMLParser::Error("no element found at offset: " + std::to_string(offset));
        }
    } else if (whence == SEEK_CUR)
        return seek(tell() + offset, SEEK_SET);
    else {
        off_t size(getMaxOffset());
        return seek(size + offset, SEEK_SET);
    }

    throw XMLParser::Error("offset not found: " + std::to_string(offset));
}


off_t XMLParser::tell() {
    XMLPart xml_part;
    if (peek(&xml_part))
        return xml_part.offset_;
    else
        return getMaxOffset();
}


off_t XMLParser::getMaxOffset() {
    switch (type_) {
    case XMLParser::XML_FILE:
        return FileUtil::GetFileSize(xml_filename_or_string_);
    case XMLParser::XML_STRING:
        return xml_filename_or_string_.length();
    default:
        LOG_ERROR("we should *never* get here!");
    }
}


bool XMLParser::getNext(XMLPart * const next, const bool combine_consecutive_characters, const std::set<std::string> &guard_opening_tags) {
    try {
        if (not prolog_parsing_done_) {
            parser_->setLoadExternalDTD(options_.load_external_dtds_);
            if (type_ == XML_FILE) {
                body_has_more_contents_ = parser_->parseFirst(xml_filename_or_string_.c_str(), token_);
                if (not body_has_more_contents_)
                    throw XMLParser::Error("error parsing document header: " + xml_filename_or_string_);
            } else if (type_ == XML_STRING) {
                xercesc::MemBufInputSource input_buffer((const XMLByte *)xml_filename_or_string_.c_str(), xml_filename_or_string_.size(),
                                                        "xml_string (in memory)");
                body_has_more_contents_ = parser_->parseFirst(input_buffer, token_);
                if (not body_has_more_contents_)
                    throw XMLParser::Error("error parsing document header: " + xml_filename_or_string_);
            } else
                throw XMLParser::Error("Undefined XMLParser::Type!");

            prolog_parsing_done_ = true;
        }

        if (buffer_.empty() and body_has_more_contents_)
            body_has_more_contents_ = parser_->parseNext(token_);

        if (not buffer_.empty()) {
            if (buffer_.front().type_ == XMLPart::OPENING_TAG or buffer_.front().type_ == XMLPart::CLOSING_TAG) {
                const auto alias_tag_and_canonical_tag(tag_aliases_to_canonical_tags_map_.find(buffer_.front().data_));
                if (alias_tag_and_canonical_tag != tag_aliases_to_canonical_tags_map_.cend())
                    buffer_.front().data_ = alias_tag_and_canonical_tag->second;

                if (buffer_.front().type_ == XMLPart::OPENING_TAG
                    and guard_opening_tags.find(buffer_.front().data_) != guard_opening_tags.cend())
                    return false;
            }

            if (next != nullptr)
                *next = buffer_.front();
            buffer_.pop_front();
            if (next != nullptr and next->type_ == XMLPart::CHARACTERS and combine_consecutive_characters) {
                XMLPart peek;
                while (getNext(&peek, /* combine_consecutive_characters = */ false) and peek.type_ == XMLPart::CHARACTERS)
                    next->data_ += peek.data_;
                if (peek.type_ != XMLPart::CHARACTERS)
                    buffer_.emplace_front(peek);
            }

            if (options_.ignore_whitespace_ and next != nullptr and next->type_ == XMLPart::CHARACTERS
                and StringUtil::IsWhitespace(next->data_))
                return getNext(next, combine_consecutive_characters, guard_opening_tags);
        }
    } catch (xercesc::RuntimeException &exc) {
        ConvertAndThrowException(exc);
    }

    return not buffer_.empty() or body_has_more_contents_;
}


bool XMLParser::skipTo(const XMLPart::Type expected_type, const std::set<std::string> &expected_tags, XMLPart * const part,
                       std::string * const skipped_data) {
    if (unlikely(expected_type != XMLPart::OPENING_TAG and expected_type != XMLPart::CLOSING_TAG))
        LOG_ERROR("Unexpected type: " + XMLPart::TypeToString(expected_type));

    XMLPart xml_part;
    bool return_value(false);
    while (getNext(&xml_part)) {
        if (xml_part.type_ == expected_type) {
            if (expected_type == XMLPart::OPENING_TAG or expected_type == XMLPart::CLOSING_TAG) {
                if (expected_tags.empty())
                    return_value = true;
                else if (expected_tags.find(xml_part.data_) != expected_tags.end()
                         or tag_aliases_to_canonical_tags_map_.find(xml_part.data_) != tag_aliases_to_canonical_tags_map_.cend())
                    return_value = true;
            } else
                return_value = true;
        }

        if (skipped_data != nullptr)
            *skipped_data += xml_part.toString();

        if (return_value)
            break;
    }

    if (return_value == true and part != nullptr) {
        part->type_ = xml_part.type_;
        part->data_ = xml_part.data_;
        part->attributes_ = xml_part.attributes_;
    }

    return return_value;
}


bool XMLParser::extractTextBetweenTags(const std::string &tag, std::string * const text, const std::set<std::string> &guard_tags) {
    text->clear();

    XMLPart xml_part;

    // Look for the opening tag:
    for (;;) {
        if (not peek(&xml_part))
            return false;

        if (not guard_tags.empty()
            and (guard_tags.find(xml_part.data_) != guard_tags.cend()
                 or tag_aliases_to_canonical_tags_map_.find(xml_part.data_) != tag_aliases_to_canonical_tags_map_.cend()))
            return false;

        assert(getNext(&xml_part));
        if (xml_part.data_ == tag) {
            if (unlikely(xml_part.type_ != XMLPart::OPENING_TAG))
                return false;
            break;
        }
    }

    // Extract the text:
    while (getNext(&xml_part)) {
        if (xml_part.data_ == tag and xml_part.type_ == XMLPart::CLOSING_TAG)
            return true;

        if (xml_part.type_ == XMLPart::CHARACTERS)
            text->append(xml_part.data_);
    }

    return false;
}
