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


const std::string TESSERACT(ExecUtil::Which("tesseract"));
const int TIMEOUT(20); // in seconds


int OCR(const std::string &input_document_path, const std::string &output_document_path,
        const std::string &language_codes)
{
    if (::access(TESSERACT.c_str(), X_OK) != 0)
        throw std::runtime_error("in OCR: can't execute \"" + TESSERACT + "\"!");

    if (not language_codes.empty() and language_codes.length() < 3)
        throw std::runtime_error("in OCR: incorrect language code \"" + language_codes + "\"!");

    std::vector<std::string> args{ TESSERACT, input_document_path, "stdout" };
    if (not language_codes.empty()) {
        args.emplace_back("-l");
        args.emplace_back(language_codes);
    }

    return ExecUtil::Exec(TESSERACT, args, "", output_document_path, "", TIMEOUT) == 0;
}


int OCR(const std::string &input_document_path, const std::string &language_codes, std::string * const output) {
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const int retval = OCR(input_document_path, output_filename, language_codes);
    if (retval == 0) {
        if (not ReadFile(output_filename, output))
            return -1;
    }

    return retval;
}


int OCR(const std::string &input_document, std::string * const output, const std::string &language_codes) {
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string input_filename(auto_temp_file.getFilePath());
    std::ofstream ocr_input(input_filename);
    if (ocr_input.fail())
        return -1;
    ocr_input.write(input_document.data(), input_document.size());
    if (ocr_input.fail())
        return -1;

    return OCR(input_filename, output, language_codes);
}
