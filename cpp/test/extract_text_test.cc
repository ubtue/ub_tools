#include <iostream>
#include <cstdlib>
#include <FileUtil.h>
#include <PdfUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " pdf_file\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    try {
        const std::string input_filename(argv[1]);
        std::string pdf_document;
        if (not FileUtil::ReadString(input_filename, &pdf_document))
            logger->error("failed to read \"" + input_filename + "\"!");
        std::cout << PdfUtil::ExtractText(pdf_document) << '\n';
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
