/** \file   PdfUtil.h
 *  \brief  Functions relating to PDF documents.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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


namespace PdfUtil {


std::string ExtractText(const std::string &pdf_document);


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


/** \brief Uses tesseract to attempt OCR. */
bool GetTextFromImagePDF(const std::string &pdf_document, const std::string &tesseract_language_code,
                         std::string * const extracted_text);


} // namespace PdfUtil


#endif // ifndef PDF_UTIL_H
