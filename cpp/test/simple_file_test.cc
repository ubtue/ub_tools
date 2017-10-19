/** Test harness for the File class.
 */
#include <iostream>
#include <cstdlib>
#include <cinttypes>
#include <File.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " input_filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string input_filename(argv[1]);
    File input(input_filename, "r");
    if (not input)
        logger->error("can't open \"" + input_filename + "\" for reading!");

    try {
        uint64_t count(0);
        char dummy;
        while ((dummy = input.get()) != EOF)
            ++count;
        std::cout << "Read " << count << " bytes from \"" + input_filename + "\".\n";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }

}

