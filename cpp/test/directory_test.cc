/** Test harness for the FileUtil::Directory class..
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("[--recurse] [--display-contexts] path [regex]");
}


void ScanDir(const bool recurse, const bool display_contexts, const std::string &directory_path, const std::string &regex) {
    FileUtil::Directory directory(directory_path, regex);

    for (const auto entry : directory) {
        const auto entry_name(entry.getName());
        std::cout << entry_name << ", " << std::to_string(entry.getType())
                  << (display_contexts ? ", " + entry.getSELinuxFileContext().toString() : "") << '\n';
        if (recurse and entry.getType() == DT_DIR) {
            if (entry_name != "." and entry_name != "..")
                ScanDir(recurse, display_contexts, entry.getFullName(), regex);
        }
    }
}


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool recurse(false);
    if (std::strcmp(argv[1], "--recurse") == 0) {
        recurse = true;
        --argc, ++argv;
    }

    bool display_contexts(false);
    if (std::strcmp(argv[1], "--display-contexts") == 0) {
        display_contexts = true;
        --argc, ++argv;
    }

    if (argc != 2 and argc != 3)
        Usage();

    ScanDir(recurse, display_contexts, argv[1], argc == 3 ? argv[2] : ".*");

    return EXIT_SUCCESS;
}
