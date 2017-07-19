#include <iostream>
#include "ExecUtil.h"


int main(int argc, char *argv[]) {
    for (int arg_no(1); arg_no < argc; ++arg_no)
        std::cout << argv[arg_no] << " -> " << ExecUtil::Which(argv[arg_no]) << '\n';
}
