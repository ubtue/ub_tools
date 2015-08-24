#include <FileUtil.h>

int main() {
    FileUtil::AutoTempFile file("./this_file_shouldnt_exist");
}
