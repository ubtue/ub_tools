#include <iostream>
#include <cstdlib>
#include "Archive.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " archive_file_name\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        ArchiveReader reader(argv[1]);
        ArchiveReader::EntryInfo file_info;
        while (reader.getNext(&file_info)) {
            std::cout << file_info.getFilename() << ":\n";
            if (file_info.isRegularFile()) {
                ssize_t total_count(0), count;
                char buffer[8192];
                while ((count = reader.read(buffer, sizeof buffer)) > 0)
                    total_count += count;
                if (count == -1)
                    Error("ArchiveReader::read() returned an error! (" + reader.getLastErrorMessage() + ")");
                std::cout << "  regular file (" << total_count << " bytes)\n";
            } else if (file_info.isDirectory())
                std::cout << "  directory\n";
            else
                std::cout << "  neither a regular file nor a directory\n";
        }
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
