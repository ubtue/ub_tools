// Test harness for TimeUtil::StringToDate.
#include <iostream>
#include <cstdlib>
#include "TimeUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " format test_string\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    try {
        const TimeUtil::Date date(TimeUtil::StringToDate(argv[1], argv[2]));
        std::cout << date.toString() << '\n';
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
