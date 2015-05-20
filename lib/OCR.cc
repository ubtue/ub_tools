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
#include "util.h"


const std::string TESSERACT("/usr/bin/tesseract");
const int TIMEOUT(20); // in seconds


int OCR(const std::string &input_document_path, const std::string &output_document_path,
	const std::string &language_codes)
{
    if (::access(TESSERACT.c_str(), X_OK) != 0)
	throw std::runtime_error("in OCR: can't execute \"" + TESSERACT + "\"!");

    if (language_codes.length() < 3)
	throw std::runtime_error("in OCR: missing or incorrect language code \"" + language_codes + "\"!");

    std::vector<std::string> args{ TESSERACT, input_document_path, "stdout", "-l", language_codes };

    return Exec(TESSERACT, args, output_document_path, TIMEOUT) == 0;
}


int OCR(const std::string &input_document_path, const std::string &language_codes, std::string * const output) {
    char filename_template[] = "/tmp/ORCXXXXXX";
    const std::string output_filename(::mktemp(filename_template));
    const int retval = OCR(input_document_path, output_filename, language_codes);
    if (retval == 0) {
	if (not ReadFile(output_filename, output)) {
	    ::unlink(output_filename.c_str());
	    return -1;
	}
    }
    ::unlink(output_filename.c_str());

    return retval;
}


int OCR(const std::string &input_document, std::string * const output, const std::string &language_codes) {
    char filename_template[] = "/tmp/ORCXXXXXX";
    const std::string input_filename(::mktemp(filename_template));
    std::ofstream ocr_input(input_filename);
    if (ocr_input.fail())
	return -1;
    ocr_input.write(input_document.data(), input_document.size());
    if (ocr_input.fail()) {
	::unlink(input_filename.c_str());
	return -1;
    }

    const int retval = OCR(input_filename, output, language_codes);
    ::unlink(input_filename.c_str());

    return retval;
}
