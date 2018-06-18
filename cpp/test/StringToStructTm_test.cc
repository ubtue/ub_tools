// Test harness for TimeUtil::StringToStructTm.
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
        const struct tm tm(TimeUtil::StringToStructTm(argv[1], argv[2]));
        std::cout << TimeUtil::TimeTToString(TimeUtil::TimeGm(tm)) << '\n';
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
