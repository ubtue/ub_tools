/** \file   OCR.h
 *  \brief  Utility functions for performing of OCR.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef OCR_H
#define OCR_H


#include <string>


/** \brief OCR the input document, assumed to be in language or languages "language_codes".
 *
 *  \param input_document_path   The path to the document that we'd like to OCR.
 *  \param output_document_path  Where to put the extracted text.
 *  \param language_codes        A list of one or more 3-character ISO 639-2 language codes separated by plus signs.
 *  \return Exit code of the child process.  0 upon success.
 */
int OCR(const std::string &input_document_path, const std::string &output_document_path,
        const std::string &language_codes = "deu");


/** \brief OCR the input document, assumed to be in language or languages "language_codes".
 *
 *  \param input_document_path   The path to the document that we'd like to OCR.
 *  \param language_codes        A list of one or more 3-character ISO 639-2 language codes separated by plus signs.
 *  \param output                Where to return the extracted text.
 *  \return Exit code of the child process.  0 upon success.
 */
int OCR(const std::string &input_document_path, const std::string &language_codes, std::string * const output);


/** \brief OCR the input document, assumed to be in language or languages "language_codes".
 *
 *  \param input_document  The document that we'd like to OCR.
 *  \param output          Where to return the extracted text.
 *  \param language_codes  A list of one or more 3-character ISO 639-2 language codes separated by plus signs.
 *  \return Exit code of the child process.  0 upon success.
 */
int OCR(const std::string &input_document, std::string * const output, const std::string &language_codes = "");


#endif // ifndef OCR_H
