#include <iostream>
#include "FileUtil.h"
#include "util.h"


int main(int argc, char *argv[]) {
    if (argc != 2)
        LOG_ERROR("call with single filename argument!");

    for (auto line : FileUtil::ReadLines(argv[1]))
        std::cout << line << '\n';
}
