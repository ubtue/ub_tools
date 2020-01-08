#include <iostream>
#include <cstdlib>
#include "UrlUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("unencoded_string");
}


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    std::cout << UrlUtil::UrlEncode(argv[1]) << '\n';
    return EXIT_SUCCESS;
}
