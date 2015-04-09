#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "SimpleDB.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " db_path key\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3)
	Usage();

    try {
	const SimpleDB db(argv[1], SimpleDB::OPEN_RDONLY);

	size_t data_size;
	const void * const data(db.binaryGetData(argv[2], &data_size));
	if (data == NULL)
	    return EXIT_FAILURE;

	if (std::fwrite(data, 1, data_size, stdout) != data_size)
	    return EXIT_FAILURE;
    } catch (const std::exception &e) {
	Error(std::string("caught exception: ") + e.what());
    }
}
