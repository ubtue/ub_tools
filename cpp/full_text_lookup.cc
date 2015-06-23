#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <kchashdb.h>
#include "util.h"


const std::string DB_PATH("/var/lib/tuelib/full_text.db");

bool GetIdFromCGI(std::string * const id) {
    const char * const query = ::getenv("QUERY_STRING");
    const size_t ID_OFFSET(3);
    if (query == NULL or std::strlen(query) < (ID_OFFSET + 1))
	return false;

    *id = query + ID_OFFSET;
    return true;
}


void Lookup(const std::string &id) {
    try {
	kyotocabinet::HashDB db;
	if (not db.open(DB_PATH, kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::ONOLOCK))
	    Error("Failed to open database \"" + DB_PATH + "\" for reading ("
		  + std::string(db.error().message()) + ")!");

	std::string data;
	if (not db.get(id, &data))
	    Error("Lookup failed: " + std::string(db.error().message()));
	
	std::cout << data;
    } catch (const std::exception &e) {
	Error(std::string("caught exception: ") + e.what());
    }
}


int main(int argc, char* argv[]) {
    progname = argv[0];

    std::string id;
    if (argc == 2)
	id = argv[1];
    else if (not GetIdFromCGI(&id)) {
	std::cerr << "ERROR: couldn't parse input!\n";
	return EXIT_FAILURE;
    }

    Lookup(id);
}
