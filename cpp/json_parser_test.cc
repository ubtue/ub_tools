/** \brief A test harness for the JSON::Parser class.
 */
#include <iostream>
#include <cstdlib>
#include "FileUtil.h"
#include "JSON.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--print] json_input_file [lookup_path [default]]\n";
    std::exit(EXIT_FAILURE);
}


int main(int /*argc*/, char *argv[]) {
    ::progname = *argv;
    ++argv;

    if (*argv == nullptr)
        Usage();

    bool print(false);
    if (std::strcmp(*argv, "--print") == 0) {
        print = true;
        ++argv;
    }

    if (*argv == nullptr)
        Usage();

    const std::string json_input_filename(*argv);
    ++argv;

    std::string lookup_path;
    if (*argv != nullptr) {
        lookup_path = *argv;
        ++argv;
    }

    std::string default_value;
    if (*argv != nullptr) {
        default_value = *argv;
        ++argv;
    }

    try {
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

        if (not lookup_path.empty())
            std::cerr << lookup_path << ": "
                      << JSON::LookupString(lookup_path, tree, default_value.empty() ? nullptr : &default_value)
                      << '\n';

        delete tree;
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
