/** \file   PdfUtil.h
 *  \brief  Functions relating to PDF documents.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef PDF_UTIL_H
#define PDF_UTIL_H


#include <string>


/** \brief Returns whether a document contains text or not.
 *
 *  If this returns false it is likely that the document contains only images.
 */
bool PdfFileContainsNoText(const std::string &path);


/** \brief Returns whether a document contains text or not.
 *
 *  If this returns false it is likely that the document contains only images.
 */
bool PdfDocContainsNoText(const std::string &document);


#endif // ifndef PDF_UTIL_H
