#include <iostream>
#include <string>
#include <cstdlib>
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " utf8_text\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::string s(argv[1]);
    if (not TextUtil::TrimLastCharFromUTF8Sequence(&s)) {
        std::cerr << "Rats!\n";
        return EXIT_FAILURE;
    } else
        std::cout << s << '\n';
}

