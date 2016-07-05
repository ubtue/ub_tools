#include <iostream>
#include <cstdlib>
#include "UrlUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " unencoded\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::cout << UrlUtil::UrlEncode(argv[1]) << '\n';
}
