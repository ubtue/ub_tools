/** \file    XmlWriter.cc
 *  \brief   Implementation of class XmlWriter.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005-2007 Project iVia.
 *  Copyright 2005-2007 The Regents of The University of California.
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

#include "XmlWriter.h"
#include <stdexcept>
#include "Compiler.h"
#include "StringUtil.h"


XmlWriter::XmlWriter(File * const output_file,
                     const XmlDeclarationWriteBehaviour xml_declaration_write_behaviour, const unsigned indent_amount,
                     const TextConversionType text_conversion_type)
    : output_file_(output_file), output_string_(nullptr), indent_amount_(indent_amount), nesting_level_(0),
      text_conversion_type_(text_conversion_type)
{
    if (xml_declaration_write_behaviour == WriteTheXmlDeclaration)
        *output_file_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}


XmlWriter::XmlWriter(std::string * const output_string,
                     const XmlDeclarationWriteBehaviour xml_declaration_write_behaviour, const unsigned indent_amount,
                     const TextConversionType text_conversion_type)
    : output_file_(nullptr), output_string_(output_string), indent_amount_(indent_amount), nesting_level_(0),
      text_conversion_type_(text_conversion_type)
{
    if (xml_declaration_write_behaviour == WriteTheXmlDeclaration)
        *output_string_ = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}


namespace {


std::string EscapeAttribValue(const std::string &value, const XmlWriter::TextConversionType text_conversion_type) {
    std::string quote_escaped_string;
    for (std::string::const_iterator ch(value.begin()); ch != value.end(); ++ch) {
        if (*ch == '"')
            quote_escaped_string += "&quot;";
        else if (*ch == '&')
            quote_escaped_string += "&amp;";
        else
            quote_escaped_string += *ch;
    }

    if (text_conversion_type == XmlWriter::ConvertFromIso8859_15)
        return StringUtil::ISO8859_15ToUTF8(quote_escaped_string);
    else
        return quote_escaped_string;
}


} // unnamed namespace


void XmlWriter::openTag(const std::string &tag_name, const bool suppress_newline) {
    active_tags_.push(tag_name);
    indent();
    ++nesting_level_;

    if (output_file_ != nullptr)
        *output_file_ << '<' << tag_name;
    else
        *output_string_ += "<" + tag_name;
    for (Attributes::const_iterator attrib(next_attributes_.begin()); attrib != next_attributes_.end(); ++attrib) {
        if (output_file_ != nullptr)
            *output_file_ << ' ' << attrib->first << '=' << '"';
        else
            *output_string_ += " " + attrib->first + "=\"";

        if (attrib->second.empty()) {
            if (output_file_ != nullptr)
                *output_file_ << attrib->first;
            else
                *output_string_ += attrib->first;
        } else {
            if (output_file_ != nullptr)
                *output_file_ << EscapeAttribValue(attrib->second, text_conversion_type_);
            else
                *output_string_ += EscapeAttribValue(attrib->second, text_conversion_type_);
        }

        if (output_file_ != nullptr)
            *output_file_ << '"';
        else
            *output_string_ += '"';
    }

    if (output_file_ != nullptr)
        *output_file_ << (suppress_newline ? ">" : ">\n");
    else
        *output_string_ += (suppress_newline ? ">" : ">\n");

    next_attributes_.clear();
}


void XmlWriter::openTag(const std::string &tag_name, const Attributes &attribs, const bool suppress_newline) {
    next_attributes_.clear();
    next_attributes_ = attribs;
    openTag(tag_name, suppress_newline);
}


void XmlWriter::closeTag(const std::string &tag_name, const bool suppress_indent) {
    std::string last_closed_tag;
    do {
        if (unlikely(active_tags_.empty())) {
            if (tag_name.empty())
                throw std::runtime_error("in XmlWriter::closeTag: trying to close a tag when none are " "open!");
            else
                throw std::runtime_error("in XmlWriter::closeTag: trying to close a tag (" + tag_name + ") when none are open!");
        }

        --nesting_level_;
        if (not suppress_indent)
            indent();
        last_closed_tag = active_tags_.top();

        if (output_file_ != nullptr)
            *output_file_ << "</" << last_closed_tag << ">\n";
        else
            *output_string_ += "</" + last_closed_tag + ">\n";

        active_tags_.pop();
    } while (not tag_name.empty() and tag_name != last_closed_tag);
}


void XmlWriter::closeAllTags() {
    while (not active_tags_.empty()) {
        --nesting_level_;
        indent();

        if (output_file_ != nullptr)
            *output_file_ << "</" << active_tags_.top() << ">\n";
        else
            *output_string_ += "</" + active_tags_.top() + ">\n";

        active_tags_.pop();
    }
}


void XmlWriter::indent() {
    if (output_file_ != nullptr)
        *output_file_ << std::string(indent_amount_ * nesting_level_, ' ');
    else
        *output_string_ += std::string(indent_amount_ * nesting_level_, ' ');
}


XmlWriter &XmlWriter::operator<<(const std::string &s) {
    if (output_file_ != nullptr)
        *output_file_ << XmlWriter::XmlEscape(s, text_conversion_type_);
    else
        *output_string_ += XmlWriter::XmlEscape(s, text_conversion_type_);

    return *this;
}


XmlWriter &XmlWriter::operator<<(const char ch) {
    if (ch == '<') {
        if (output_file_ != nullptr)
            *output_file_ << "&lt;";
        else
            *output_string_ += "&lt;";
    } else if (ch == '>') {
        if (output_file_ != nullptr)
            *output_file_ << "&gt;";
        else
            *output_string_ += "&gt;";
    } else {
        if (text_conversion_type_ == ConvertFromIso8859_15) {
            const char zero_terminated_string[] = { ch, '\0' };
            if (output_file_ != nullptr)
                *output_file_ << StringUtil::ISO8859_15ToUTF8(std::string(zero_terminated_string));
            else
                *output_string_ += StringUtil::ISO8859_15ToUTF8(std::string(zero_terminated_string));
        } else {
            if (output_file_ != nullptr)
                *output_file_ << ch;
            else
                *output_string_ += ch;
        }
    }

    return *this;
}


XmlWriter &XmlWriter::operator<<(const int i) {
    if (output_file_ != nullptr)
        *output_file_ << i;
    else
        *output_string_ += StringUtil::ToString(i);

    return *this;
}


XmlWriter &XmlWriter::operator<<(const unsigned u) {
    if (output_file_ != nullptr)
        *output_file_ << u;
    else
        *output_string_ += StringUtil::ToString(u);

    return *this;
}


XmlWriter &XmlWriter::operator<<(const double d) {
    if (output_file_ != nullptr)
        *output_file_ << d;
    else
        *output_string_ += StringUtil::ToString(d);

    return *this;
}


XmlWriter &XmlWriter::indent(XmlWriter &xml_writer) {
    if (xml_writer.output_file_ != nullptr)
        *xml_writer.output_file_ << std::string(xml_writer.indent_amount_ * xml_writer.nesting_level_, ' ');
    else
        *xml_writer.output_string_ += std::string(xml_writer.indent_amount_ * xml_writer.nesting_level_, ' ');

    return xml_writer;
}


XmlWriter &XmlWriter::endl(XmlWriter &xml_writer) {
    if (xml_writer.output_file_ != nullptr)
        *xml_writer.output_file_ << File::endl;
    else
        *xml_writer.output_string_ += '\n';

    return xml_writer;
}


std::string XmlWriter::XmlEscape(const std::string &s, const TextConversionType text_conversion_type,
                                 const std::string &additional_escapes)
{
    std::string escaped_string;

    // for performance, set this here so empty() doesn't have to be checked all the time:
    const bool additional(not additional_escapes.empty());

    for (const auto ch : s) {
        if (ch == '<')
            escaped_string += "&lt;";
        else if (ch == '>')
            escaped_string += "&gt;";
        else if (ch == '&')
            escaped_string += "&amp;";
        else if (ch == '"')
            escaped_string += "&quot;";
        else if (ch == '\'')
            escaped_string += "&apos;";
        else if (additional and additional_escapes.find(ch) != std::string::npos)
            escaped_string += StringUtil::Format("&#%04d;", ch);
        else
            escaped_string += ch;
    }

    if (text_conversion_type == XmlWriter::ConvertFromIso8859_15)
        return StringUtil::ISO8859_15ToUTF8(escaped_string);
    else
        return escaped_string;
}
