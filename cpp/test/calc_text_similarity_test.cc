#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " [--ignore-case] text1 text2\n";
    std::exit(EXIT_FAILURE);
}


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool ignore_case(false);
    if (std::strcmp(argv[1], "--ignore-case") == 0) {
        ignore_case = true;
        --argc, ++argv;
    }

    std::cout << TextUtil::CalcTextSimilarity(argv[1], argv[2], ignore_case) << '\n';

    return EXIT_SUCCESS;
}
