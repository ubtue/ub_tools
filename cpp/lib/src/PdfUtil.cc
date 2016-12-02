/** \file   PdfUtil.cc
 *  \brief  Implementation of functions relating to PDF documents.
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
#include "PdfUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


bool PdfFileContainsNoText(const std::string &path) {
    static std::string pdffonts_path;
    if (pdffonts_path.empty()) {
        pdffonts_path = ExecUtil::Which("pdffonts");
        if (pdffonts_path.empty())
            Error("can't find \"pdffonts\" in our PATH environment!");
    }
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    std::vector<std::string> args{ path };
    const int retval(ExecUtil::Exec(pdffonts_path, args, "", output_filename));
    if (retval == 0) {
        std::string output;
        if (not ReadFile(output_filename, &output))
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
