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
    std::vector<uint32_t> utf32_sequence;

    TextUtil::UTF8ToUTF32Decoder utf8_to_utf32_decoder;
    for (const char ch : original_utf8_string) {
        if (not utf8_to_utf32_decoder.addByte(ch))
            utf32_sequence.emplace_back(utf8_to_utf32_decoder.getUTF32Char());
    }

    std::string converted_utf8_string;
    for (const uint32_t utf32_char : utf32_sequence)
        converted_utf8_string += TextUtil::UTF32ToUTF8(utf32_char);

    std::cout << (converted_utf8_string == original_utf8_string ? "Whoohoo!\n" : "WTF?\n");
}
