/** \brief Reports various bits of information about a Kyotocabinet data base.
 *
 */


#include <iostream>
#include <map>
#include <cstdlib>
#include <kchashdb.h>
#include "util.h"


static void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "usage: " << ::progname << " path_to_kyotocabinet_database\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();
    const std::string db_filename(argv[1]);

    kyotocabinet::HashDB db;
    if (not db.open(db_filename, kyotocabinet::HashDB::OREADER))
        logger->error("Failed to open database \"" + db_filename + "\" for reading ("
                      + std::string(db.error().message()) + ")!");

    std::map<std::string, std::string> status_info;
    if (not db.status(&status_info))
        logger->error("Failed to get status info on \"" + db_filename + "\" ("
                      + std::string(db.error().message()) + ")!");

    for (const auto &key_and_value : status_info)
        std::cout << key_and_value.first << ": " << key_and_value.second << '\n';
}
