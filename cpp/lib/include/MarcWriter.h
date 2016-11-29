/** \brief Interface declarations for MARC writer classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbibliothek Tübingen.  All rights reserved.
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

#ifndef MARC_WRITER_H
#define MARC_WRITER_H


#include <memory>
#include "File.h"
#include "MarcXmlWriter.h"


// Forward declaration.
class MarcRecord;


class MarcWriter {
public:
    enum WriterType { XML, BINARY, AUTO };
public:
    virtual ~MarcWriter() { }

    virtual void write(const MarcRecord &record) = 0;

    /** \return a reference to the underlying, assocaiated file. */
    virtual File &getFile() = 0;

    /** \note If you pass in AUTO for "writer_type", "output_filename" must end in ".mrc" or ".xml"! */
    static std::unique_ptr<MarcWriter> Factory(const std::string &output_filename, WriterType writer_type = AUTO);
};


class BinaryMarcWriter: public MarcWriter {
    File * const output_;
public:
    explicit BinaryMarcWriter(File * const output): output_(output) { }

    virtual void write(const MarcRecord &record) final;

    /** \return a reference to the underlying, assocaiated file. */
    virtual File &getFile() final { return *output_; }
};


class XmlMarcWriter: public MarcWriter {
    MarcXmlWriter *xml_writer_;
public:
    explicit XmlMarcWriter(File * const output_file, const unsigned indent_amount = 0,
                           const XmlWriter::TextConversionType text_conversion_type = XmlWriter::NoConversion);
    virtual ~XmlMarcWriter() final { delete xml_writer_; }

    virtual void write(const MarcRecord &record) final;

    /** \return a reference to the underlying, assocaiated file. */
    virtual File &getFile() final { return *xml_writer_->getAssociatedOutputFile(); }
};


#endif // MARC_WRITER_H
