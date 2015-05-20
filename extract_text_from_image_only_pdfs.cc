#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include "PdfUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " pdf_image_file_name\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 2)
	Usage();

    if (::access(argv[1], R_OK) != 0)
	Error("can't read \"" + std::string(argv[1]) + "\"!");

    if (not PdfDocContainsNoText(argv[1]))
	Error("input file \"" + std::string(argv[1]) + "\" contains text!");
}
