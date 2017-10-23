/** \file   extract_text_from_image_only_pdfs.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <libgen.h>
#include <unistd.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "PdfUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " pdf_image_file_name [language_code_or_codes]\n";
    std::cerr << "       When no language code has been specified, \"deu\" is used as a default.\n";
    std::exit(EXIT_FAILURE);
}


const std::string BASH_HELPER("pdf_images_to_text.sh");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc != 2 and argc != 3)
            Usage();
        const std::string input_filename(argv[1]);

        if (::access(input_filename.c_str(), R_OK) != 0)
            logger->error("can't read \"" + input_filename + "\"!");

        std::string pdf;
        if (not FileUtil::ReadString(input_filename, &pdf))
            logger->error("failed to read document from \"" + input_filename + "\"!");

        if (not PdfUtil::PdfDocContainsNoText(pdf))
            logger->error("input file \"" + input_filename + "\" contains text!");

        char output_filename[] = "OCR_OUT_XXXXXX";
        const int output_fd(::mkstemp(output_filename));
        if (output_fd == -1)
            logger->error("failed to create a temporary file!");
        const FileUtil::AutoDeleteFile auto_deleter(output_filename);

        #pragma GCC diagnostic ignored "-Wvla"
        char path[std::strlen(argv[0]) + 1];
        #pragma GCC diagnostic warning "-Wvla"
        std::strcpy(path, argv[0]);
        const std::string dir_path(::dirname(path));

        std::vector<std::string> args { input_filename, output_filename };
        if (argc == 3)
            args.emplace_back(argv[2]);

        if (ExecUtil::Exec(dir_path + "/" + BASH_HELPER, args, "", "", "", 20 /* seconds */) != 0)
            logger->error("failed to execute conversion script!");

        std::string extracted_text;
        if (not FileUtil::ReadString(output_filename, &extracted_text))
            logger->error("failed to read contents of \"" + std::string(output_filename) + "\"!");

        if (extracted_text.empty())
            logger->error("No text was extracted from \"" + input_filename + "\"!");

        std::cout.write(extracted_text.data(), extracted_text.size());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
