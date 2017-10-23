/** Test harness for the FileUtil::ConcatFiles() function.
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " target source1 [source2 .. sourceN]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 3)
        Usage();

    try {
        std::vector<std::string> source_files;
        for (int arg_no(2); arg_no < argc; ++arg_no)
            source_files.emplace_back(argv[arg_no]);
        const size_t result_size(FileUtil::ConcatFiles(argv[1], source_files));
        std::cerr << "Created " << argv[0] << " with a length of " << result_size << " bytes.\n";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
