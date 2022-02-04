#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include "StringUtil.h"
#include "WallClockTimer.h"
#include "util.h"


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " time_to_sleep_in_seconds\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    WallClockTimer timer;
    timer.start();
    ::sleep(StringUtil::ToUnsigned(argv[1]));
    timer.stop();

    std::cout << "Approximately " << static_cast<unsigned>(timer.getTime()) << " seconds have elapsed.\n";
}
