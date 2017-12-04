#include <iostream>
#include <cstdlib>
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " string_wth_c_style_escapes\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::string string_without_c_style_escapes(argv[1]);
    TextUtil::CStyleUnescape(&string_without_c_style_escapes);
    std::cout << '"' << TextUtil::CollapseAndTrimWhitespace(string_without_c_style_escapes) << "\"\n";
}
