#include <iostream>
#include <cstdlib>
#include "Archive.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " archive_file_name file_to_add1 [file_to_add2 .. file_to_addN]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    try {
        ArchiveWriter writer(argv[1]);
        for (int arg_no(2); arg_no < argc; ++arg_no) {
            const std::string member_filename(argv[arg_no]);
            std::cout << "About to add \"" + member_filename + "\".\n";
            writer.add(member_filename);
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
