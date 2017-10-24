/** Test harness for the FileUtil::Directory class..
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " [--display-contexts] path [regex]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool display_contexts(false);
    if (std::strcmp(argv[1], "--display-contexts") == 0) {
        display_contexts = true;
        --argc, ++argv;
    }

    if (argc != 2 and argc != 3)
        Usage();

    try {
        FileUtil::Directory *directory;
        if (argc == 2)
            directory = new FileUtil::Directory(argv[1]);
        else
            directory = new FileUtil::Directory(argv[1], argv[2]);

        for (const auto entry : *directory)
            std::cout << entry.getName() << ", " << std::to_string(entry.getType())
                      << (display_contexts ? ", " + entry.getSELinuxContext().toString() : "")
                      << '\n';

        delete directory;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
