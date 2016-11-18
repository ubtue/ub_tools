#include <FileUtil.h>

int main(int argc, char * argv[]) {
    if (argc != 2)
        return 1;
    FileUtil::AutoTempFile file(argv[1]);
}
