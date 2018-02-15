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
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "PdfUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace PdfUtil {


std::string ExtractText(const std::string &pdf_document) {
    static std::string pdftotext_path;
    if (pdftotext_path.empty())
        pdftotext_path = ExecUtil::LocateOrDie("pdftotext");

    const FileUtil::AutoTempFile auto_temp_file1;
    const std::string &input_filename(auto_temp_file1.getFilePath());
    if (not FileUtil::WriteString(input_filename, pdf_document))
        logger->error("in PdfUtil::ExtractText: can't write document to \"" + input_filename + "\"!");

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());

    const int retval(ExecUtil::Exec(pdftotext_path,
                                    { "-enc", "UTF-8", "-nopgbrk", input_filename, output_filename }));
    if (retval != 0)
        logger->error("in PdfUtil::ExtractText: failed to execute \"" + pdftotext_path + "\"!");
    std::string extracted_text;
    if (not FileUtil::ReadString(output_filename, &extracted_text))
        logger->error("in PdfUtil::ExtractText: failed to read extracted text from \"" + output_filename + "\"!");

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


bool GetTextFromImage(const std::string &img_path, const std::string &tesseract_language_code,
                      std::string * const extracted_text)
{
    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
    if (api->Init(NULL, tesseract_language_code.c_str()))
        ERROR("Could not initialize tesseract!");

    Pix *image = pixRead(img_path.c_str());
    api->SetImage(image);
    *extracted_text = api->GetUTF8Text();

    api->End();
    pixDestroy(&image);

    return true;
}


bool GetTextFromImagePDF(const std::string &pdf_document, const std::string &tesseract_language_code,
                         std::string * const extracted_text, unsigned timeout)
{
    extracted_text->clear();

    static std::string pdf_images_script_path;
    if (pdf_images_script_path.empty())
        pdf_images_script_path = ExecUtil::LocateOrDie("pdfimages");

    const FileUtil::AutoTempDirectory auto_temp_dir;
    const std::string &output_dirname(auto_temp_dir.getDirectoryPath());
    const std::string input_filename(output_dirname + "/in.pdf");
    if (not FileUtil::WriteString(input_filename, pdf_document))
        ERROR("failed to write the PDF to a temp file!");

    if (ExecUtil::Exec(pdf_images_script_path, { input_filename, output_dirname + "/out" }, "", "", "", timeout) != 0)
        ERROR("failed to extract images from PDF file!");

    std::vector<std::string> pdf_image_filenames;
    if (FileUtil::GetFileNameList("out.*", &pdf_image_filenames, output_dirname) == 0)
        ERROR("PDF did not contain any images!");

    for (const std::string &pdf_image_filename : pdf_image_filenames) {
        std::string image_text;
        if (not GetTextFromImage(output_dirname + "/" + pdf_image_filename, tesseract_language_code, &image_text))
            ERROR("failed to extract text from image " + pdf_image_filename);
         *extracted_text += " " + image_text;
    }

    *extracted_text = StringUtil::TrimWhite(*extracted_text);
    return not extracted_text->empty();
}


} // namespace PdfUtil
