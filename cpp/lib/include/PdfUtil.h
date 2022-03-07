/** \file   PdfUtil.h
 *  \brief  Functions relating to PDF documents.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <string>


namespace PdfUtil {


constexpr unsigned DEFAULT_PDF_EXTRACTION_TIMEOUT(60); // seconds
bool ExtractText(const std::string &pdf_document, std::string * const extracted_text, const std::string &start_page = "",
                 const std::string &end_page = "");

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


/** \brief Uses tesseract API to attempt OCR. */
bool GetTextFromImage(const std::string &img_path, const std::string &tesseract_language_code, std::string * const extracted_text);


/** \brief Uses pdfimages + tesseract API to attempt OCR. */
bool GetTextFromImagePDF(const std::string &pdf_document, const std::string &tesseract_language_code, std::string * const extracted_text,
                         const unsigned timeout = DEFAULT_PDF_EXTRACTION_TIMEOUT /* in s */);

/** \brief Convert pdf to image and then attempt tesseract OCR. */
bool GetOCRedTextFromPDF(const std::string &pdf_document_path, const std::string &tesseract_language_code,
                         std::string * const extracted_text, const unsigned timeout = DEFAULT_PDF_EXTRACTION_TIMEOUT);


/** \brief Get the output of the pdfinfo program */
bool ExtractPDFInfo(const std::string &pdf_document, std::string * const pdf_output);

/** \brief Try to extract given PDF as page wise HTML in a temporary directory */
bool ExtractHTMLAsPages(const std::string &pdf_document, const std::string &output_location);


} // namespace PdfUtil
