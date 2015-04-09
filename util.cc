#include "util.h"
#include <iostream>
#include <cstdio>


char *progname; // Must be set in main() with "progname = argv[0];";


void Error(const std::string &msg) {
  std::cerr << progname << ": " << msg << '\n';
  std::exit(EXIT_FAILURE);
}


void Warning(const std::string &msg) {
  std::cerr << progname << ": " << msg << '\n';
}


bool ReadFile(const std::string &filename, std::string * const contents) {
    FILE *input(std::fopen(filename.c_str(), "r"));
    if (input == NULL)
	return false;

    contents->clear();
    while (not std::feof(input)) {
	char buf[BUFSIZ];
	const size_t byte_count(std::fread(buf, 1, sizeof buf, input));
	if (byte_count != sizeof(buf) and std::ferror(input)) {
	    std::fclose(input);
	    return false;
	}
	contents->append(buf, byte_count);
    }

    std::fclose(input);
    return true;
}
