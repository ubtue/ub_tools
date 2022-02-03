/** Test harness for TextUtil::CStyleUnescape().
 */
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <TextUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " escaped_string\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    std::cout << "Unescaped string: \"" << TextUtil::CStyleUnescape(argv[1]) << "\"\n";
}
