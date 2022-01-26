/** \file   PdfUtil.cc
 *  \brief  Implementation of functions relating to PDF documents.
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
#include "PdfUtil.h"
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace PdfUtil {


bool ExtractText(const std::string &pdf_document, std::string * const extracted_text,
                 const std::string &start_page, const std::string &end_page)
{
    static std::string pdftotext_path;
    if (pdftotext_path.empty())
        pdftotext_path = ExecUtil::LocateOrDie("pdftotext");

    const FileUtil::AutoTempFile auto_temp_file1;
    const std::string &input_filename(auto_temp_file1.getFilePath());
    if (not FileUtil::WriteString(input_filename, pdf_document)) {
        LOG_WARNING("can't write document to \"" + input_filename + "\"!");
        return false;
    }

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());
    std::vector<std::string> pdftotext_params { "-enc", "UTF-8", "-nopgbrk" };
    if (not start_page.empty())
        pdftotext_params.insert(pdftotext_params.end(), { "-f", start_page });
    if (not end_page.empty())
        pdftotext_params.insert(pdftotext_params.end(), { "-l", end_page });
    pdftotext_params.insert(pdftotext_params.end(), { input_filename, output_filename });
    const int retval(ExecUtil::Exec(pdftotext_path, pdftotext_params));
    if (retval != 0) {
        LOG_WARNING("failed to execute \"" + pdftotext_path + "\"!");
        return false;
    }

    if (not FileUtil::ReadString(output_filename, extracted_text)) {
        LOG_WARNING("failed to read extracted text from \"" + output_filename + "\"!");
        return false;
    }

    return not extracted_text->empty();
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
    if (not FileUtil::WriteString(output_filename, document))
        return false;
    return PdfFileContainsNoText(output_filename);
}


bool GetTextFromImage(const std::string &img_path, const std::string &tesseract_language_code,
                      std::string * const extracted_text)
{
    static std::string tesseract_path(ExecUtil::LocateOrDie("tesseract"));
    extracted_text->clear();
    std::string stderr_output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(
            tesseract_path, { img_path, "stdout" /*tesseract arg to redirect*/,
            "-l", tesseract_language_code,
            "--oem", "1" /*unlike beforehand use LSTM extract instead of legacy */},
            extracted_text, &stderr_output, 0 /* timeout */, SIGKILL,
            { {"OMP_THREAD_LIMIT", "1" } }, /* address tesseract 4 IPC problems */
            "" /* working dir */)
        )
        LOG_WARNING("While processing " + img_path + ": " + stderr_output);

    return not extracted_text->empty();
}


bool GetTextFromImagePDF(const std::string &pdf_document, const std::string &tesseract_language_code,
                         std::string * const extracted_text, const unsigned timeout)
{
    extracted_text->clear();

    static std::string pdf_images_script_path(ExecUtil::LocateOrDie("pdfimages"));

    const FileUtil::AutoTempDirectory auto_temp_dir;
    const std::string &output_dirname(auto_temp_dir.getDirectoryPath());
    const std::string input_filename(output_dirname + "/in.pdf");
    if (not FileUtil::WriteString(input_filename, pdf_document)) {
        LOG_WARNING("failed to write the PDF to a temp file!");
        return false;
    }

    if (ExecUtil::Exec(pdf_images_script_path, { input_filename, output_dirname + "/out" }, "", "", "", timeout) != 0) {
        LOG_WARNING("failed to extract images from PDF file!");
        return false;
    }

    std::vector<std::string> pdf_image_filenames;
    if (FileUtil::GetFileNameList("out.*", &pdf_image_filenames, output_dirname) == 0) {
        LOG_WARNING("PDF did not contain any images!");
        return false;
    }

    for (const std::string &pdf_image_filename : pdf_image_filenames) {
        std::string image_text;
        if (not GetTextFromImage(output_dirname + "/" + pdf_image_filename, tesseract_language_code, &image_text)) {
            LOG_WARNING("failed to extract text from image " + pdf_image_filename);
            return false;
        }
        *extracted_text += " " + image_text;
    }

    *extracted_text = StringUtil::TrimWhite(*extracted_text);
    return not extracted_text->empty();
}


bool GetOCRedTextFromPDF(const std::string &pdf_document_path, const std::string &tesseract_language_code,
                         std::string * const extracted_text, const unsigned timeout)
{
    extracted_text->clear();
    static std::string pdf_to_image_command(ExecUtil::LocateOrDie("convert"));
    const FileUtil::AutoTempDirectory auto_temp_dir;
    const std::string &image_dirname(auto_temp_dir.getDirectoryPath());
    const std::string temp_image_location = image_dirname + "/img.tiff";
    if (ExecUtil::Exec(pdf_to_image_command, { "-density", "300", pdf_document_path, "-depth", "8", "-strip",
                                               "-background", "white", "-alpha", "off", temp_image_location
                                             }, "", "", "", timeout) != 0) {
        LOG_WARNING("failed to convert PDF to image!");
        return false;
    }
    if (not GetTextFromImage(temp_image_location, tesseract_language_code, extracted_text))
        LOG_WARNING("failed to extract OCRed text");

    *extracted_text = StringUtil::TrimWhite(*extracted_text);
    return not extracted_text->empty();
}


bool ExtractPDFInfo(const std::string &pdf_document, std::string * const pdf_output) {
    static std::string pdfinfo_path;
    if (pdfinfo_path.empty())
        pdfinfo_path = ExecUtil::LocateOrDie("pdfinfo");
    const FileUtil::AutoTempFile auto_temp_file1;
    const std::string &input_filename(auto_temp_file1.getFilePath());
    if (not FileUtil::WriteString(input_filename, pdf_document)) {
        LOG_WARNING("can't write document to \"" + input_filename + "\"!");
        return false;
    }
    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &pdfinfo_output_filename(auto_temp_file2.getFilePath());

    const std::vector<std::string> pdfinfo_params { input_filename };
    const int retval(ExecUtil::Exec(pdfinfo_path, pdfinfo_params, pdfinfo_output_filename /* stdout */,
                     pdfinfo_output_filename /* stderr */));
    if (retval != 0) {
        LOG_WARNING("failed to execute \"" + pdfinfo_path + "\"!");
        return false;
    }
    std::string pdfinfo_output;
    if (unlikely(not FileUtil::ReadString(pdfinfo_output_filename, pdf_output)))
        LOG_ERROR("Unable to extract pdfinfo output");

    return true;
}


bool ExtractHTMLAsPages(const std::string &pdf_document, const std::string &output_dirname) {
    static std::string pdftohtml_path;
    if (pdftohtml_path.empty())
        pdftohtml_path = ExecUtil::LocateOrDie("pdftohtml");

    std::vector<std::string> pdftohtml_params { "-i" /* ignore images */,
                                                "-c" /* generate complex output */,
                                                "-hidden" /* force hidden text extraction */,
                                                "-fontfullname" /* outputs the font name without any substitutions */
                                              };
    const std::string pdf_temp_link(output_dirname + '/' + FileUtil::GetBasename(pdf_document));
    FileUtil::CreateSymlink(pdf_document, pdf_temp_link);
    pdftohtml_params.emplace_back(pdf_temp_link);
    ExecUtil::ExecOrDie(pdftohtml_path, pdftohtml_params, "" /* stdin */, "" /* stdout */, "" /* stderr */, 0 /* timeout */,
                        SIGKILL, std::unordered_map<std::string, std::string>() /* env */, output_dirname /* working dir */);

    // Clean up HTML
    static std::string tidy_path;
    if (tidy_path.empty())
        tidy_path = ExecUtil::LocateOrDie("tidy");

    FileUtil::Directory html_pages(output_dirname, ".*\\.html$");
    for (const auto &html_page : html_pages) {
        const std::vector<std::string> tidy_params({ "-modify" /* write back to original file */, "-quiet", html_page.getName() });
        // return code 1 means there were only warnings
        const int tidy_retval(ExecUtil::Exec(tidy_path, tidy_params,  "" /* stdin */, "" /* stdout */, "" /* stderr */, 0 /* timeout */,
                        SIGKILL, std::unordered_map<std::string, std::string>() /* env */, output_dirname /* working dir */));
        if (tidy_retval != 0 and tidy_retval != 1)
            LOG_ERROR("Error while cleaning up " + html_page.getName());

    }

    return true;
}


} // namespace PdfUtil
