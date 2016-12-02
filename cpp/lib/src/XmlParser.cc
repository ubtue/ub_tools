/** \file    XmlParser.cc
 *  \brief   Implementation of an XML parser class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "XmlParser.h"
#include <cstdarg>
#include <libxml2/libxml/parserInternals.h>
#include "StringUtil.h"


bool XmlParser::AttributeMap::insert(const std::string &name, const std::string &value) {
    const iterator iter = find(name);
    if (iter != end())
        return false;

    std::map<std::string, std::string>::insert(std::pair<std::string, std::string>(name, value));
    return true;
}


bool XmlParser::Chunk::getAttribute(const std::string &attrib_name, std::string * const attrib_value) const {
    attrib_value->clear();

    const AttributeMap::const_iterator name_and_value(attribute_map_->find(attrib_name));
    if (name_and_value == attribute_map_->end())
        return false;
    *attrib_value = name_and_value->second;
    return true;
}


bool XmlParser::reentrantXmlSAXParseFile(const xmlSAXHandlerPtr sax, const std::string &filename) {
    xmlParserCtxtPtr parser_ctxt = ::xmlCreateFileParserCtxt(filename.c_str());
    if (parser_ctxt == nullptr)
        return false;

    parser_ctxt->sax = sax;
    parser_ctxt->userData = this;

    ::xmlParseDocument(parser_ctxt);

    const bool well_formed = parser_ctxt->wellFormed;

    ::xmlFreeDoc(parser_ctxt->myDoc);
    parser_ctxt->myDoc = nullptr;

    if (sax != nullptr)
        parser_ctxt->sax = nullptr;

    ::xmlFreeParserCtxt(parser_ctxt);
    ::xmlCleanupParser();

    return well_formed;
}


bool XmlParser::reentrantXmlSAXParseMemory(const xmlSAXHandlerPtr sax, const char * const memory,
                                           const size_t memory_size)
{
    xmlParserCtxtPtr memory_parser_ctxt = ::xmlCreateMemoryParserCtxt(memory, memory_size);
    if (memory_parser_ctxt == nullptr)
        return false;

    memory_parser_ctxt->sax = sax;
    memory_parser_ctxt->userData = this;

    ::xmlParseDocument(memory_parser_ctxt);

    const bool well_formed = memory_parser_ctxt->wellFormed;

    ::xmlFreeDoc(memory_parser_ctxt->myDoc);
    memory_parser_ctxt->myDoc = nullptr;

    if (sax != nullptr)
        memory_parser_ctxt->sax = nullptr;

    ::xmlFreeParserCtxt(memory_parser_ctxt);
    ::xmlCleanupParser();

    return well_formed;
}


bool XmlParser::parse() {
    xmlSAXHandler sax_handler;
    initSaxHandler(&sax_handler, notification_mask_);

    if (filename_ != nullptr)
        return reentrantXmlSAXParseFile(&sax_handler, filename_);
    else
        return reentrantXmlSAXParseMemory(&sax_handler, memory_, memory_size_);
}


void XmlParser::initSaxHandler(xmlSAXHandlerPtr const sax_handler, unsigned notification_mask) {
    // Set all function pointers to nullptr:
    ::memset(sax_handler, '\0', sizeof *sax_handler);

    // Explicitly set function handlers for "events" that we would like to
    // be notified of:
    if (notification_mask & XmlParser::START_DOCUMENT)
        sax_handler->startDocument = XmlParser::startDocumentHandler;
    if (notification_mask & XmlParser::END_DOCUMENT)
        sax_handler->endDocument = XmlParser::endDocumentHandler;
    if (notification_mask & XmlParser::START_ELEMENT)
        sax_handler->startElement = XmlParser::startElementHandler;
    if (notification_mask & XmlParser::END_ELEMENT)
        sax_handler->endElement = XmlParser::endElementHandler;

    // We always want to install the character handler so that we can
    // count newlines to determine the current line number when
    // calling notify.
    sax_handler->characters = XmlParser::charactersHandler;

    // We always want to install the character handler so that we can
    // count newlines to determine the current line number when
    // calling notify.
    sax_handler->ignorableWhitespace = XmlParser::ignorableWhitespaceHandler;

    if (notification_mask & XmlParser::WARNING)
        sax_handler->warning = XmlParser::warningHandler;
    if (notification_mask & XmlParser::ERROR)
        sax_handler->error = XmlParser::errorHandler;
    if (notification_mask & XmlParser::FATAL_ERROR)
        sax_handler->fatalError = XmlParser::fatalErrorHandler;
}


void XmlParser::startDocumentHandler(void * user_data) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    Chunk chunk(XmlParser::START_DOCUMENT, "", xml_parser->lineno_);
    xml_parser->notify(chunk);
}


void XmlParser::endDocumentHandler(void * user_data) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    Chunk chunk(XmlParser::END_DOCUMENT, "", xml_parser->lineno_);
    xml_parser->notify(chunk);
}


void XmlParser::startElementHandler(void * user_data, const xmlChar *element_name, const xmlChar **attribs) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    std::string name;
    if (xml_parser->convert_to_iso8859_15_)
        name = StringUtil::UTF8ToISO8859_15(reinterpret_cast<const char *>(element_name));
    else
        name = reinterpret_cast<const char *>(element_name);

    Chunk chunk(XmlParser::START_ELEMENT, name, xml_parser->lineno_);
    chunk.attribute_map_ = new AttributeMap;
    for (const xmlChar **attrib = attribs; attrib != nullptr and *attrib != nullptr; ++attrib) {
        std::string attrib_name, attrib_value;
        if (xml_parser->convert_to_iso8859_15_) {
            attrib_name  = StringUtil::UTF8ToISO8859_15(reinterpret_cast<const char *>(*attrib++));
            attrib_value = StringUtil::UTF8ToISO8859_15(reinterpret_cast<const char *>(*attrib));
        }
        else {
            attrib_name  = reinterpret_cast<const char *>(*attrib++);
            attrib_value = reinterpret_cast<const char *>(*attrib);
        }
        chunk.attribute_map_->insert(attrib_name, attrib_value);
    }

    xml_parser->notify(chunk);
}


void XmlParser::endElementHandler(void * user_data, const xmlChar *element_name) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    std::string name;
    if (xml_parser->convert_to_iso8859_15_)
        name = StringUtil::UTF8ToISO8859_15(reinterpret_cast<const char *>(element_name));
    else
        name = reinterpret_cast<const char *>(element_name);

    Chunk chunk(XmlParser::END_ELEMENT, name, xml_parser->lineno_);
    xml_parser->notify(chunk);
}


void XmlParser::charactersHandler(void * user_data, const xmlChar *chars, int len) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    // NOTE: This makes an assumption that the C API XML parser does not move the XML around in memory.
    // This saves a pointer to the XML being processed so we can, among other things, display it in
    // error messages to help w/ debugging.
    xml_parser->current_xml_text_ = chars;

    std::string characters(reinterpret_cast<const char *>(chars), len);
    if (xml_parser->convert_to_iso8859_15_)
        characters = StringUtil::UTF8ToISO8859_15(characters);

    bool newline_is_last_char = characters[characters.length() - 1] == '\n';
    unsigned newline_count(0);
    for (std::string::const_iterator ch(characters.begin()); ch != characters.end(); ++ch)
        if (*ch == '\n')
            ++newline_count;
    xml_parser->lineno_ += newline_count;

    if (xml_parser->notification_mask_ & XmlParser::CHARACTERS) {
        Chunk chunk(XmlParser::CHARACTERS, characters,
                    newline_is_last_char ? xml_parser->lineno_ - 1 : xml_parser->lineno_);
        xml_parser->notify(chunk);
    }
}


void XmlParser::ignorableWhitespaceHandler(void * user_data, const xmlChar *chars, int len) {
    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    std::string characters(reinterpret_cast<const char *>(chars), len);
    if (xml_parser->convert_to_iso8859_15_)
        characters = StringUtil::UTF8ToISO8859_15(characters);

    bool newline_is_last_char = characters[characters.length() - 1] == '\n';
    unsigned newline_count(0);
    for (std::string::const_iterator ch(characters.begin()); ch != characters.end(); ++ch)
        if (*ch == '\n')
            ++newline_count;
    xml_parser->lineno_ += newline_count;

    if (xml_parser->notification_mask_ & XmlParser::IGNORABLE_WHITESPACE) {
        Chunk chunk(XmlParser::IGNORABLE_WHITESPACE, characters,
                    newline_is_last_char ? xml_parser->lineno_ - 1 : xml_parser->lineno_);
        xml_parser->notify(chunk);
    }
}


void XmlParser::warningHandler(void * user_data, const char *fmt, ...) {
    char warning[1024];
    va_list args;
    va_start(args, fmt);
    ::vsnprintf(warning, sizeof(warning), fmt, args);
    va_end(args);

    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    Chunk chunk(XmlParser::WARNING, warning, xml_parser->lineno_);
    xml_parser->notify(chunk);
}


void XmlParser::errorHandler(void * user_data, const char *fmt, ...) {
    char error[1024];
    va_list args;
    va_start(args, fmt);
    ::vsnprintf(error, sizeof(error), fmt, args);
    va_end(args);

    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    std::string text(reinterpret_cast<const char *>(xml_parser->current_xml_text_), 40);
    if (xml_parser->convert_to_iso8859_15_)
        text = StringUtil::UTF8ToISO8859_15(text);

    Chunk chunk(XmlParser::ERROR, std::string(error) + ": " + text, xml_parser->lineno_);
    xml_parser->notify(chunk);
}


void XmlParser::fatalErrorHandler(void * user_data, const char *fmt, ...) {
    char error[1024];
    va_list args;
    va_start(args, fmt);
    ::vsnprintf(error, sizeof(error), fmt, args);
    va_end(args);

    XmlParser * const xml_parser = reinterpret_cast<XmlParser * const>(user_data);

    Chunk chunk(XmlParser::FATAL_ERROR, error, xml_parser->lineno_);
    xml_parser->notify(chunk);
}
