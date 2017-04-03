/** \brief A test harness for the JSON::Parser class.
 */
#include <iostream>
#include <cstdlib>
#include "FileUtil.h"
#include "JSON.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " json_input_file\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string json_input_filename(argv[1]);
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

    delete tree;
}
