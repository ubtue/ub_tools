#include <iostream>
#include <cstdlib>
#include "EmailSender.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " sender recipient subject message_body\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
	Usage();

    if (not EmailSender::SendEmail(argv[1], argv[2], argv[3], argv[4]))
	Error("We suck!");
}
