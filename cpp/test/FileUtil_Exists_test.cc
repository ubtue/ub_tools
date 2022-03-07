#include <iostream>
#include "FileUtil.h"
#include "util.h"


int main(int argc, char *argv[]) {
    if (argc != 2)
        LOG_ERROR("call with single filename argument!");

    if (FileUtil::Exists(argv[1]))
        std::cout << argv[1] << " exists\n";
    else
        std::cout << argv[1] << " does not exist\n";
}
