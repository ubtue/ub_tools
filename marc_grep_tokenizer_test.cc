#include <iostream>
#include <cstdlib>
#include "MarcGrepTokenizer.h"


int main(int argc, char *argv[]) {
    if (argc != 2) {
	std::cerr << "Usage: " << argv[0] << "query_string\n";
	return EXIT_FAILURE;
    }

    Tokenizer tokenizer(argv[1]);
    TokenType token;
    while ((token = tokenizer.getToken()) != END_OF_INPUT) {
	std::cout << Tokenizer::TokenTypeToString(token);
	if (token == STRING_CONSTANT)
	    std::cout << ": \"" << Tokenizer::EscapeString(tokenizer.getLastStringConstant()) << "\"\n";
	else if (token == UNSIGNED_CONSTANT)
	    std::cout << ": " << tokenizer.getLastUnsignedConstant() << '\n';
	else if (token == INVALID_INPUT) {
	    std::cout << '\n';
	    return EXIT_SUCCESS;
	} else
	    std::cout << '\n';
    }
    std::cout << "END_OF_INPUT\n";

    return EXIT_SUCCESS;
}
