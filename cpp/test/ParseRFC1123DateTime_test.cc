#include <iostream>
#include <cstring>
#include "TimeUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " rss_datetime\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string rss_datetime(argv[1]);
    time_t converted_time;
    if (not TimeUtil::ParseRFC1123DateTime(rss_datetime, &converted_time))
        LOG_ERROR("failed to convert \"" + rss_datetime + "\"!");
    std::cout << "converted_time as time_t: " << converted_time << '\n';
    std::cout << "Converted time is " << TimeUtil::TimeTToString(converted_time, TimeUtil::DEFAULT_FORMAT, TimeUtil::UTC) << '\n';
}
