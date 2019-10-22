#include <iostream>
#include <fstream>
#include "FileUtil.h"

int main() {
    FileUtil::AutoTempDir temp_dir;
    const std::string dir_path(temp_dir.getDirPath());
    std::cerr << "Created " << temp_dir.getDirPath() << '\n';
    std::cerr << "Creating testfile\n";
    std::ofstream testfile(dir_path + "/testfile");
    testfile << "Test Write";
}

