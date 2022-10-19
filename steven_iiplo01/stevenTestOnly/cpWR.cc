#include <iostream>
#include <vector>
#include <dirent.h>
#include <regex>
#include "FileUtil.h"
#include "CopyUsingRegex.h"


int main() {
    CopyUsingRegex::CopyFiles("./", "./result/", R"(\.de*)");
    
    return EXIT_SUCCESS;
}

