/** Test harness for the FileUtil::Directory class..
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " path [regex]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2 and argc != 3)
        Usage();

    try {
        FileUtil::Directory *directory;
        if (argc == 2)
            directory = new FileUtil::Directory(argv[1]);
        else
            directory = new FileUtil::Directory(argv[1], argv[2]);

        for (const auto entry : *directory)
            std::cout << entry.getName() << ", " << std::to_string(entry.getType()) << '\n';

        delete directory;
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
