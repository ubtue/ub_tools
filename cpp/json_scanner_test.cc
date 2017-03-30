/** \brief A test harness for the JSON::Scanner class.
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

    JSON::Scanner scanner(json_document);
    for (;;) {
        const JSON::TokenType token(scanner.getToken());
        switch (token) {
        case JSON::COMMA:
            std::cout << "COMMA\n";
            break;
        case JSON::COLON:
            std::cout << "COLON\n";
            break;
        case JSON::OPEN_BRACE:
            std::cout << "OPEN_BRACE\n";
            break;
        case JSON::CLOSE_BRACE:
            std::cout << "CLOSE_BRACE\n";
            break;
        case JSON::OPEN_BRACKET:
            std::cout << "OPEN_BRACKET\n";
            break;
        case JSON::CLOSE_BRACKET:
            std::cout << "CLOSE_BRACKET\n";
            break;
        case JSON::TRUE_CONST:
            std::cout << "true\n";
            break;
        case JSON::FALSE_CONST:
            std::cout << "false\n";
            break;
        case JSON::NULL_CONST:
            std::cout << "null\n";
            break;
        case JSON::INTEGER_CONST:
            std::cout << "integer: " << scanner.getLastIntegerConstant() << '\n';
            break;
        case JSON::DOUBLE_CONST:
            std::cout << "double: " << scanner.getLastDoubleConstant() << '\n';
            break;
        case JSON::STRING_CONST:
            std::cout << "string: " << scanner.getLastStringConstant() << '\n';
            break;
        case JSON::END_OF_INPUT:
            std::cout << "END_OF_INPUT\n";
            return EXIT_SUCCESS;
        case JSON::ERROR:
            std::cout << "ERROR(" << scanner.getLineNumber() << "): " << scanner.getLastErrorMessage() << '\n';
            return EXIT_FAILURE;
        }
    }
}
