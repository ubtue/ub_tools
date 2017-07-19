#include <iostream>
#include <istream>
#include <iterator>
#include <cstdlib>
#include "GzStream.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " mode\n";
    std::cerr << "       Where \"mode\" has to be either \"compress\" or \"decompress\".\n";
    std::cerr << "       Data is either read from (compress) or written to (decompress) stdout.\n";
    std::cerr << "       The compressed or uncompressed data is then written to stdout.\n";
    std::exit(EXIT_FAILURE);
}


// Read stdin until EOF.
std::string SnarfUpStdin() {
    std::cin >> std::noskipws;
    std::istream_iterator<char> it(std::cin);
    std::istream_iterator<char> end;
    return std::string(it, end);
}


void Compress() {
    const std::string uncompressed_data(SnarfUpStdin());
    std::cout << GzStream::CompressString(uncompressed_data);
}


void Decompress() {
    const std::string compressed_data(SnarfUpStdin());
    std::cout << GzStream::DecompressString(compressed_data);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();
    const std::string mode(argv[1]);

    if (mode == "compress")
        Compress();
    else if (mode == "decompress")
        Decompress();
    else
        Usage();
}
