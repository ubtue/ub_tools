/** \file   PdfUtil.cc
 *  \brief  Implementation of functions relating to PDF documents.
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
#include "PdfUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "util.h"


namespace PdfUtil {


std::string ExtractText(const std::string &pdf_document) {
    static std::string pdftotext_path;
    if (pdftotext_path.empty())
        pdftotext_path = ExecUtil::LocateOrDie("pdftotext");

    const FileUtil::AutoTempFile auto_temp_file1;
    const std::string &input_filename(auto_temp_file1.getFilePath());
    if (not FileUtil::WriteString(input_filename, pdf_document))
        Error("in PdfUtil::ExtractText: can't write document to \"" + input_filename + "\"!");

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());

    const int retval(ExecUtil::Exec(pdftotext_path,
                                    { "-enc", "UTF-8", "-nopgbrk", input_filename, output_filename }));
    if (retval != 0)
        Error("in PdfUtil::ExtractText: failed to execute \"" + pdftotext_path + "\"!");
    std::string extracted_text;
    if (not FileUtil::ReadString(output_filename, &extracted_text))
        Error("in PdfUtil::ExtractText: failed to read extracted text from \"" + output_filename + "\"!");

    return extracted_text;
}


bool PdfFileContainsNoText(const std::string &path) {
    static std::string pdffonts_path;
    if (pdffonts_path.empty())
        pdffonts_path = ExecUtil::LocateOrDie("pdffonts");

    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const int retval(ExecUtil::Exec(pdffonts_path, { path }, "", output_filename));
    if (retval == 0) {
        std::string output;
        if (not FileUtil::ReadString(output_filename, &output))
            return false;
        return output.length() == 188; // Header only?
    }

    return retval == 0;
}


bool PdfDocContainsNoText(const std::string &document) {
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const FileUtil::AutoDeleteFile auto_delete(output_filename);
    if (not FileUtil::WriteString(output_filename, document))
        return false;
    return PdfFileContainsNoText(output_filename);
}


bool GetTextFromImagePDF(const std::string &pdf_document, const std::string &tesseract_language_code,
                         std::string * const extracted_text)
{
    extracted_text->clear();

    static std::string pdf_images_script_path;
    if (pdf_images_script_path.empty())
        pdf_images_script_path = ExecUtil::LocateOrDie("pdf_images_to_text.sh");

    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &input_filename(auto_temp_file.getFilePath());
    if (not FileUtil::WriteString(input_filename, pdf_document))
        Error("failed to write the PDF to a temp file!");

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());
    static constexpr unsigned TIMEOUT(60); // in seconds

    const std::string UPDATE_DB_LOG_PATH("/usr/local/var/log/tufind/update_full_text_db/log");
    MiscUtil::LogRotate(UPDATE_DB_LOG_PATH, /* max_count = */5);

    if (ExecUtil::Exec(pdf_images_script_path, { input_filename, output_filename, tesseract_language_code },
                       /* new_stdin = */"", /* new_stdout = */UPDATE_DB_LOG_PATH,
                       /* new_stderr = */UPDATE_DB_LOG_PATH, TIMEOUT) != 0)
    {
        Warning("failed to execute conversion script \"" + pdf_images_script_path + "\" w/in "
                + std::to_string(TIMEOUT) + " seconds!");
        return false;
    }

    std::string plain_text;
    if (not FileUtil::ReadString(output_filename, extracted_text))
        Error("failed to read OCR output!");

    return not extracted_text->empty();
}


} // namespace PdfUtil
