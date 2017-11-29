#include <iostream>
#include <cstdlib>
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " string_wth_c_style_escapes\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::cout << '"' << StringUtil::CollapseAndTrimWhitespace(StringUtil::CStyleUnescape(argv[1])) << "\"\n";
}
