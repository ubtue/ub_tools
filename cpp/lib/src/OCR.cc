/** \file   OCR.h
 *  \brief  Utility functions for performing of OCR.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "OCR.h"
#include <fstream>
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


const int TIMEOUT(20); // in seconds


int OCR(const std::string &input_document_path, const std::string &output_document_path, const std::string &language_codes) {
    static const std::string TESSERACT(ExecUtil::Which("tesseract"));
    if (::access(TESSERACT.c_str(), X_OK) != 0)
        throw std::runtime_error("in OCR: can't execute \"" + TESSERACT + "\"!");

    if (not language_codes.empty() and language_codes.length() < 3)
        throw std::runtime_error("in OCR: incorrect language code \"" + language_codes + "\"!");

    std::vector<std::string> args{ input_document_path, "stdout" };
    if (not language_codes.empty()) {
        args.emplace_back("-l");
        args.emplace_back(language_codes);
    }

    return ExecUtil::Exec(TESSERACT, args, "", output_document_path, "", TIMEOUT);
}


int OCR(const std::string &input_document_path, const std::string &language_codes, std::string * const output) {
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const int retval = OCR(input_document_path, output_filename, language_codes);
    if (retval == 0) {
        if (not FileUtil::ReadString(output_filename, output))
            return -1;
    }

    return retval;
}


int OCR(const std::string &input_document, std::string * const output, const std::string &language_codes) {
    const FileUtil::AutoTempFile input_temp_file;
    if (not FileUtil::WriteString(input_temp_file.getFilePath(), input_document))
        return -1;
    const FileUtil::AutoTempFile output_temp_file;
    const int exit_code(OCR(input_temp_file.getFilePath(), output_temp_file.getFilePath(), language_codes));
    if (exit_code != 0)
        return exit_code;
    if (not FileUtil::ReadString(output_temp_file.getFilePath(), output))
        return -1;
    return 0;
}
