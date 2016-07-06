/** \file    XmlParser.h
 *  \brief   Declaration of an XML parser class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2008 Project iVia.
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

#ifndef XML_PARSER_H
#define XML_PARSER_H


#include <map>
#include <stdexcept>
#include <libxml2/libxml/parser.h>


/** \class  XmlParser
 *  \brief  A parser for XML documents.
 *
 *  This class provides a simple XML parser.  To use it, you should create a subclass that specifies
 *  which tokens should generate events (by setting the notification_mask) and then overriding the
 *  notify member function to take an appropriate action whenever an event occurs.
 *
 *  See the accompanying xml_parser_test program for an example.
 */
class XmlParser {
    const char * const filename_;
    const char * const memory_;
    const size_t memory_size_;
    const bool convert_to_iso8859_15_;
    const unsigned notification_mask_;
    unsigned lineno_;
    const xmlChar *current_xml_text_;
public:
    static const unsigned START_DOCUMENT           = 1u << 0u;
    static const unsigned END_DOCUMENT             = 1u << 1u;
    static const unsigned START_ELEMENT            = 1u << 2u;
    static const unsigned END_ELEMENT              = 1u << 3u;
    static const unsigned CHARACTERS               = 1u << 4u;
    static const unsigned IGNORABLE_WHITESPACE     = 1u << 5u;
    static const unsigned WARNING                  = 1u << 6u;
    static const unsigned ERROR                    = 1u << 7u;
    static const unsigned FATAL_ERROR              = 1u << 8u;
    static const unsigned EVERYTHING               = 0xFFFF;

    class AttributeMap: public std::map<std::string, std::string> {
    public:
        /** \brief  Insert a value into an AttributeMap, replacing any old value.
         *  \param  name   The name of the key.
         *  \param  value  The value to be associated with "name".
         *  \return True if the attribute wasn't in the map yet, else false.
         *
         *  The pair (name, value) is stored in the AttributeMap.  If
         *  there is an existing value associated with name, it is
         *  not inserted.
         */
        bool insert(const std::string &name, const std::string &value);
    };

    struct Chunk {
        const unsigned type_;
        const std::string text_;
        const unsigned lineno_;
        AttributeMap *attribute_map_; // only non-nullptr if type_ == OPENING_TAG
    public:
        Chunk(const unsigned type, const std::string &text, const unsigned lineno,
              AttributeMap * const attribute_map = nullptr)
            : type_(type), text_(text), lineno_(lineno), attribute_map_(attribute_map) { }
        ~Chunk() { delete attribute_map_; }

        /** \brief  Attempts to retrieve the value of a specified attribute.
         *  \param  attrib_name   The name of the attrbute whose value we would like to look up.
         *  \param  attrib_value  On a successful return, the value of the requested attribute.
         *  \return True if the attribute was present and false otherwise.
         */
        bool getAttribute(const std::string &attrib_name, std::string * const attrib_value) const;
    };
public:
    explicit XmlParser(const std::string &filename, const bool convert_to_iso8859_15 = false,
                       unsigned notification_mask = EVERYTHING)
        : filename_(filename.c_str()), memory_(nullptr), memory_size_(0),
          convert_to_iso8859_15_(convert_to_iso8859_15), notification_mask_(notification_mask),
          lineno_(1), current_xml_text_(nullptr) { }
    XmlParser(const char * const memory, const size_t memory_size, const bool convert_to_iso8859_15 = false,
              unsigned notification_mask = EVERYTHING)
        : filename_(nullptr), memory_(memory), memory_size_(memory_size),
          convert_to_iso8859_15_(convert_to_iso8859_15), notification_mask_(notification_mask),
          lineno_(1), current_xml_text_(nullptr) { }
    virtual ~XmlParser() { }
    bool parse();

    /** \brief Override this member function in order to perform custom processing.
     *  \note  Warning:  This function gets called via a C language callback which
     *                   means that you cannot throw exceptions in your implementation
     *                   of notify!
     */
    virtual void notify(const Chunk &chunk) = 0;
private:
    XmlParser();                                // intentionally unimplemented
    XmlParser(const XmlParser &rhs);            // intentionally unimplemented
    XmlParser &operator=(const XmlParser &rhs); // intentionally unimplemented

    bool reentrantXmlSAXParseFile(const xmlSAXHandlerPtr sax, const std::string &filename);
    bool reentrantXmlSAXParseMemory(const xmlSAXHandlerPtr sax, const char * const memory, const size_t memory_size);
    void initSaxHandler(xmlSAXHandlerPtr const sax, unsigned notification_mask);
    static void startDocumentHandler(void * user_data);
    static void endDocumentHandler(void * user_data);
    static void startElementHandler(void * user_data, const xmlChar *element_name, const xmlChar **attribs);
    static void endElementHandler(void * user_data, const xmlChar *element_name);
    static void charactersHandler(void * user_data, const xmlChar *chars, int len);
    static void ignorableWhitespaceHandler(void * user_data, const xmlChar *chars, int len);
    static void warningHandler(void * user_data, const char *msg, ...);
    static void errorHandler(void * user_data, const char *msg, ...);
    static void fatalErrorHandler(void * user_data, const char *msg, ...);
};


#endif // ifndef XML_PARSER_H


