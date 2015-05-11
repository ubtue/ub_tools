#include <iostream>
#include <cstdlib>
#include <kchashdb.h>
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " db_path key\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3)
	Usage();

    kyotocabinet::HashDB db;
    if (not db.open(argv[1], kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::ONOLOCK))
	Error("Failed to open database \"" + std::string(argv[1]) + "\" for reading ("
	      + std::string(db.error().message()) + ")!");

    std::string data;
    if (not db.get(argv[2], &data))
	Error("Lookup failed: " + std::string(db.error().message()));

    std::cout << data;

    db.close();
}
