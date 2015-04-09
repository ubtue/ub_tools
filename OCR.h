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


#endif // ifndef OCR_H
