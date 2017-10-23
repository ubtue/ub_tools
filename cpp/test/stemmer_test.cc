#include <iostream>
#include <cstdlib>
#include <Stemmer.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " word language_name_or_code\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    try {
        const Stemmer *stemmer(Stemmer::StemmerFactory(argv[2]));
        std::cout << stemmer->stem(argv[1]) << '\n';
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
