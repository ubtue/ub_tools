#include <iostream>
#include <string>
#include <vector>
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

    const std::string original_utf8_string(argv[1]);
    std::cout << "Original string has " << original_utf8_string.length() << " bytes.\n";
    std::vector<uint32_t> utf32_sequence;

    TextUtil::UTF8ToUTF32Decoder utf8_to_utf32_decoder;
    for (const char ch : original_utf8_string) {
        if (not utf8_to_utf32_decoder.addByte(ch))
            utf32_sequence.emplace_back(utf8_to_utf32_decoder.getUTF32Char());
    }
    std::cout << "We produced " << utf32_sequence.size() << " UTF-32 characters.\n";

    std::string converted_utf8_string;
    for (const uint32_t utf32_char : utf32_sequence)
        converted_utf8_string += TextUtil::UTF32ToUTF8(utf32_char);

    if (converted_utf8_string == original_utf8_string)
        std::cout << "Whoohoo!\n";
    else
        std::cout << "WTF? (\"" << converted_utf8_string << "\")\n";
}
