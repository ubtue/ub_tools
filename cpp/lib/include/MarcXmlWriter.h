/** \file    MarcXmlWriter.h
 *  \brief   Declaration of the MarcXmlWriter class.
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
#pragma once


#include "XmlWriter.h"


class MarcXmlWriter : public XmlWriter {
    bool suppress_header_and_tailer_;

public:
    /** \brief  Instantiate a MarcXmlWriter object.
     *  \param  output_file                 Where to write the generated XML to.
     *  \param  suppress_header_and_tailer  Do not write the XML prolog and the opening and closing colection tags.
     *  \param  indent_amount               How many leading spaces to add per indentation level.
     *  \param  text_conversion_type        What kind, if any, of text conversion to apply on output.
     */
    explicit MarcXmlWriter(File* const output_file, const bool suppress_header_and_tailer = false, const unsigned indent_amount = 0,
                           const TextConversionType text_conversion_type = NoConversion);

    /** \brief  Instantiate a MarcXmlWriter object.
     *  \param  output_string               Where to write the generated XML to.
     *  \param  suppress_header_and_tailer  Do not write the XML prolog and the opening and closing colection tags.
     *  \param  indent_amount               How many leading spaces to add per indentation level.
     *  \param  text_conversion_type        What kind, if any, of text conversion to apply on output.
     */
    explicit MarcXmlWriter(std::string* const output_string, const bool suppress_header_and_tailer = false,
                           const unsigned indent_amount = 0, const TextConversionType text_conversion_type = NoConversion);

    /** Destroyes an MarcXmlWriter object, closing any still open tags. */
    virtual ~MarcXmlWriter() override;
};
