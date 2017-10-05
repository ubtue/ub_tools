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
            Error("can't read \"" + input_filename + "\"!");

        std::string pdf;
        if (not FileUtil::ReadString(input_filename, &pdf))
            Error("failed to read document from \"" + input_filename + "\"!");

        if (not PdfUtil::PdfDocContainsNoText(pdf))
            Error("input file \"" + input_filename + "\" contains text!");

        char output_filename[] = "OCR_OUT_XXXXXX";
        const int output_fd(::mkstemp(output_filename));
        if (output_fd == -1)
            Error("failed to create a temporary file!");
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
            Error("failed to execute conversion script!");

        std::string extracted_text;
        if (not FileUtil::ReadString(output_filename, &extracted_text))
            Error("failed to read contents of \"" + std::string(output_filename) + "\"!");

        if (extracted_text.empty())
            Error("No text was extracted from \"" + input_filename + "\"!");

        std::cout.write(extracted_text.data(), extracted_text.size());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
