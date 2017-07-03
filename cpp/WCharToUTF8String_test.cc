#include <iostream>
#include <cstdlib>
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " code_point\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const unsigned code_point(StringUtil::ToUnsigned(argv[1]));
    std::string decoded_char;
    if (not TextUtil::WCharToUTF8String(static_cast<wchar_t>(code_point), &decoded_char))
        Error("failed to decode the code point!");
    else
        std::cout << "The code point was decoded to \"" + decoded_char + "\". (Length was "
                  << decoded_char.length() << ".)\n";
}
