/** \brief Writer for marc files.
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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


#include "File.h"
#include "XmlWriter.h"


// Forward declaration.
class MarcRecord;


namespace MarcWriter {

/**
 * \brief writes the given record to the output file,
 * but only if the record contains more than one field.
 */
void Write(MarcRecord &record, File * const output);

/**
 * \brief writes the given record to the output file using a XML writer.
 */
void Write(MarcRecord &record, XmlWriter * const xml_writer);


} // namespace MarcWriter


#endif // MARC_WRITER_H
