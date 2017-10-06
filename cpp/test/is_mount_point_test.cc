#include <iostream>
#include <cstdlib>
#include <FileUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " path_to_a_directory\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2)
        Usage();

    std::cerr << argv[1] << " is " << (FileUtil::IsMountPoint(argv[1]) ? "a" : "not a") << " mount point.\n";
}
