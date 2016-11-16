/** \file    XmlWriter.h
 *  \brief   Declaration of the XmlWriter class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2007 Project iVia.
 *  Copyright 2005-2007 The Regents of The University of California.
 *  Copyright 2015-2016 Universitätsbibliothek Tübingen
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

#ifndef XML_WRITER_H
#define XML_WRITER_H


#include <list>
#include <stack>
#include <string>
#include "File.h"


/** \class  XmlWriter
 *  \brief  An XML generator class.
 *
 *  See the accompanying tests/XmlWriterTest.cc program for a usage example.
 */
class XmlWriter {
    File * const output_file_;
    std::string *output_string_;
    std::stack<std::string> active_tags_;
    const unsigned indent_amount_;
    unsigned nesting_level_;
public:
    enum TextConversionType { NoConversion, ConvertFromIso8859_15 };
    enum XmlDeclarationWriteBehaviour { WriteTheXmlDeclaration, DoNotWriteTheXmlDeclaration };
    typedef std::list< std::pair<std::string, std::string> > Attributes;
private:
    const TextConversionType text_conversion_type_;
    Attributes next_attributes_;
public:
    /** \brief  Instantiate an XmlWriter object.
     *  \param  output_file                      Where to write the generated XML to.
     *  \param  xml_declaration_write_behaviour  Whether to write an XML declaration or not.
     *  \param  indent_amount                    How many leading spaces to add per indentation level.
     *  \param  text_conversion_type             What kind, if any, of text conversion to apply on output.
     */
    explicit XmlWriter(File * const output_file,
                       const XmlDeclarationWriteBehaviour xml_declaration_write_behaviour = WriteTheXmlDeclaration,
                       const unsigned indent_amount = 0,
                       const TextConversionType text_conversion_type = NoConversion);

    /** \brief  Instantiate an XmlWriter object.
     *  \param  output_string                    Where to write the generated XML to.
     *  \param  xml_declaration_write_behaviour  Whether to write an XML declaration or not.
     *  \param  indent_amount                    How many leading spaces to add per indentation level.
     *  \param  text_conversion_type             What kind, if any, of text conversion to apply on output.
     */
    explicit XmlWriter(std::string * const output_string,
                       const XmlDeclarationWriteBehaviour xml_declaration_write_behaviour = WriteTheXmlDeclaration,
                       const unsigned indent_amount = 0,
                       const TextConversionType text_conversion_type = NoConversion);

    /** Destroyes an XmlWriter object, closing any still open tags. */
    virtual ~XmlWriter() { closeAllTags(); }

    File *getAssociatedOutputFile() const { return output_file_; }

    /** \brief    Adds another attribute to be used the next time the one-argument version of openTag() gets called.
     *  \warning  If the two-argument version of openTag() gets called all prior calls to this function will be
     *            ignored!
     */
    void addAttribute(const std::string &name, const std::string &value = "")
        { next_attributes_.push_back(std::make_pair(name, value) ); }

    /** Writes an open tag at the current indentation level. Uses the attributes queued up by calls to addAttribute(),
        if any. */
    void openTag(const std::string &tag_name, const bool suppress_newline = false);

    /**  Writes an open tag at the current indentation level. Does not use the attributes queued up by calls to
         addAttribute(). */
    void openTag(const std::string &tag_name, const Attributes &attribs, const bool suppress_newline = false);

    /** Write character data. */
    void write(const std::string &characters) { (*this) << characters; }

    /** Write character data between an opening and closing tag pair. */
    void writeTagsWithData(const std::string &tag_name, const std::string &characters,
                           const bool suppress_indent = false)
    {
        openTag(tag_name, suppress_indent);
        write(characters);
        closeTag(tag_name, suppress_indent);
    }

    /** Write character data between an opening and closing tag pair. */
    void writeTagsWithEscapedData(const std::string &tag_name, const Attributes &attribs,
                                  const std::string &characters, const bool suppress_indent = false,
                                  const XmlWriter::TextConversionType text_conversion_type = NoConversion)
    {
        openTag(tag_name, attribs, suppress_indent);
        write(XmlWriter::XmlEscape(characters, text_conversion_type));
        closeTag(tag_name, suppress_indent);
    }

    /** Write character data between an opening and closing tag pair. */
    void writeTagsWithEscapedData(const std::string &tag_name, const std::string &characters,
                                  const bool suppress_indent = false,
                                  const XmlWriter::TextConversionType text_conversion_type = NoConversion)
    {
        openTag(tag_name, suppress_indent);
        write(XmlWriter::XmlEscape(characters, text_conversion_type));
        closeTag(tag_name, suppress_indent);
    }

    /** Write character data between an opening and closing tag pair. */
    void writeTagsWithData(const std::string &tag_name, const Attributes &attribs, const std::string &characters,
                           const bool suppress_indent = false)
    {
        openTag(tag_name, attribs, suppress_indent);
        write(characters);
        closeTag(tag_name, suppress_indent);
    }

    /** \brief  Writes a closing tag at the approriate indentation level.
     *  \param  tag_name         If empty, we close the last open tag otherwise we close tags until we find a tag that
     *                           matches tag_name which we also close.
     *  \param  suppress_indent  If true we don't emit any leading spaces o/w we indent to the previous indentation level.
     */
    void closeTag(const std::string &tag_name = "", const bool suppress_indent = false);

    /** \brief  Writes a closing tag for the last open tag.
     *  \param  suppress_indent  If true we don't emit any leading spaces o/w we indent to the previous indentation level.
     */
    void closeTag(const bool suppress_indent) { closeTag("", suppress_indent); }

    /** Calls closeTag() until all open tags are closed. */
    void closeAllTags();

    /** Emits the number of spaces corresponding to the current nesting level to the output file. */
    void indent();

    XmlWriter &operator<<(const std::string &s);
    XmlWriter &operator<<(const char * const s) { return operator<<(std::string(s)); }
    XmlWriter &operator<<(const char ch);
    XmlWriter &operator<<(const int i);
    XmlWriter &operator<<(const unsigned u);
    XmlWriter &operator<<(const double d);

    /** Support function for  I/O manipulators taking 0 arguments. */
    XmlWriter &operator<<(XmlWriter &(*xml_writer)(XmlWriter &)) { return xml_writer(*this); }

    // I/O manipulators:
    static XmlWriter &indent(XmlWriter &xml_writer);
    static XmlWriter &endl(XmlWriter &xml_writer);

    /** \brief  Escapes text for XML generation.
     *  \param  unescaped_text        The text that may optionally contain ampersands, single and double quotes or angle brackets.
     *  \param  text_conversion_type  Used to optionally convert from Latin-9 to UTF-8.
     *  \param  additional_escapes    If desired, escape some additional characters (newline and carriage return are sometimes desired)
     *  \return The string with the XML metacharacters escaped.
     */
    static std::string XmlEscape(const std::string &unescaped_text, const XmlWriter::TextConversionType text_conversion_type,
                                 const std::string &additional_escapes = "");
private:
    XmlWriter();                                // intentionally unimplemented
    XmlWriter(const XmlWriter &rhs);            // intentionally unimplemented
    XmlWriter &operator=(const XmlWriter &rhs); // intentionally unimplemented
};


#endif // ifndef XML_WRITER_H
