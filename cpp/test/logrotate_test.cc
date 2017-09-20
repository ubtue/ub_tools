/** Test harness for the MiscUtil::LogRotate function.
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " log_file [max_count]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc != 2 and argc != 3)
        Usage();

    try {
        if (argc == 2)
            MiscUtil::LogRotate(argv[1]);
        else
            MiscUtil::LogRotate(argv[1], StringUtil::ToUnsigned(argv[2]));
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
