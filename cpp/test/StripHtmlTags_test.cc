/** Test harness for the HtmlUtil::StripHtmlTags.
 */
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("(--replace-entities|--do-not-replace-entities) html_input_filename");
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
    if (argc != 3)
        Usage();

    bool replace_entities;
    if (std::strcmp(argv[1], "--replace-entities") == 0)
        replace_entities = true;
    else if (std::strcmp(argv[1], "--do-not-replace-entities") == 0)
        replace_entities = false;
    else
        Usage();

    const std::string file_contents(FileUtil::ReadStringOrDie(argv[2]));
    std::cout << HtmlUtil::StripHtmlTags(file_contents, replace_entities) << '\n';

    return EXIT_SUCCESS;
}
