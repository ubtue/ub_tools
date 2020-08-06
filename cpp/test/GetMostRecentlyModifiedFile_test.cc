#include <iostream>
#include <cstdlib>
#include "FileUtil.h"


int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " directory\n";
        return EXIT_FAILURE;
    }

    std::string most_recently_modified_file;
    timespec last_modification_time;
    if (FileUtil::GetMostRecentlyModifiedFile(argv[1], &most_recently_modified_file, &last_modification_time))
        std::cout << most_recently_modified_file << '\n';
    else
        std::cout << "No files found!\n";

    return EXIT_SUCCESS;
}
