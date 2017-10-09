/** A test harness for the FileUtil::CountLines() function. */
#include <iostream>
#include "FileUtil.h"


int main(int argc, char *argv[]) {
    if (argc != 2)
        return 1;
    std::cout << argv[1] << " contains " << FileUtil::CountLines(argv[1]) << " line(s).\n";
}
