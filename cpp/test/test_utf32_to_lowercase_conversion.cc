#include <iostream>
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

    std::wstring utf32_chars;
    if (not TextUtil::UTF8ToWCharString(argv[1], &utf32_chars))
        LOG_ERROR("failed to convert text to UTF32!");

    for (auto &ch : utf32_chars)
        ch = TextUtil::UTF32ToLower(ch);

    std::string utf8_string;
    if (not TextUtil::WCharToUTF8String(utf32_chars, &utf8_string))
        LOG_ERROR("failed to convert wstring to UTF8!");

    std::cout << utf8_string << '\n';
}
