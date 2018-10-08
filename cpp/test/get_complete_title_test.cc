/** Test harness for the MARC::Record::getCompleteTitle member function..
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "MARC.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const auto reader(MARC::Reader::Factory(argv[1]));
    while (const auto record = reader->read())
        std::cout << record.getCompleteTitle() << '\n';

    return EXIT_SUCCESS;
}
