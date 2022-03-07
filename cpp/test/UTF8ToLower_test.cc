/** \brief Test harness for TextUtil::UTF8ToLower. */
#include <iostream>
#include <string>
#include <clocale>
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

    try {
        std::setlocale(LC_ALL, "UTF8");
        std::string s(argv[1]);
        std::cout << TextUtil::UTF8ToLower(&s) << '\n';
    } catch (const std::exception &x) {
        logger->error(x.what());
    }
}
