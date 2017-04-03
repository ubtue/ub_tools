/** \brief A test harness for the JSON::Parser class.
 */
#include <iostream>
#include <cstdlib>
#include "FileUtil.h"
#include "JSON.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--print] json_input_file\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    bool print(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--print") != 0)
            Usage();
        print = true;
    }

    const std::string json_input_filename(argv[argc == 2 ? 1 : 2]);
    std::string json_document;
    if (not FileUtil::ReadString(json_input_filename, &json_document))
        Error("could not read \"" + json_input_filename + "\"!");

    JSON::Parser parser(json_document);
    JSON::JSONNode *tree;
    if (not parser.parse(&tree)) {
        std::cerr << ::progname << ": " << parser.getErrorMessage() << '\n';
        delete tree;
        return EXIT_FAILURE;
    }

    if (print)
        std::cout << tree->toString() << '\n';

    delete tree;
}
