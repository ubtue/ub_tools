#include <iostream>
#include <cstdlib>
#include <TextUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " phrase\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    std::vector<std::string> words;
    if (not TextUtil::ChopIntoWords(argv[1], &words))
        Error("there was a character code conversion error!");

    for (const auto &word : words)
        std::cout << word << '\n';
}
