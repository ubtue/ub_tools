/** \file    MarcXmlWriter.cc
 *  \brief   Implementation of class MarcXmlWriter.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  \copyright 2016-2019 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MarcXmlWriter.h"


MarcXmlWriter::MarcXmlWriter(File* const output_file, const bool suppress_header_and_tailer, const unsigned indent_amount,
                             const TextConversionType text_conversion_type)
    : XmlWriter(output_file, XmlWriter::WriteTheXmlDeclaration, indent_amount, text_conversion_type),
      suppress_header_and_tailer_(suppress_header_and_tailer) {
    if (not suppress_header_and_tailer_)
        openTag("collection", { std::make_pair("xmlns", "http://www.loc.gov/MARC21/slim"),
                                std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                                std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd") });
}


MarcXmlWriter::MarcXmlWriter(std::string* const output_string, const bool suppress_header_and_tailer, const unsigned indent_amount,
                             const TextConversionType text_conversion_type)
    : XmlWriter(output_string, XmlWriter::WriteTheXmlDeclaration, indent_amount, text_conversion_type),
      suppress_header_and_tailer_(suppress_header_and_tailer) {
    if (not suppress_header_and_tailer_)
        openTag("collection", { std::make_pair("xmlns", "http://www.loc.gov/MARC21/slim"),
                                std::make_pair("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"),
                                std::make_pair("xsi:schemaLocation", "http://www.loc.gov/standards/marcxml/schema/MARC21slim.xsd") });
}


MarcXmlWriter::~MarcXmlWriter() {
    if (not suppress_header_and_tailer_)
        closeTag("collection");
}
