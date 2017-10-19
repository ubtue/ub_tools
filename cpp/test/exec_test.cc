#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include "ExecUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " [--new-stdout filename][--timeout-in-seconds seconds] path "
              << "[arg1 arg2 ... argN]\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    std::string new_stdout;
    unsigned timeout_in_seconds(0);
    int arg_no(1);
    while (arg_no < argc and StringUtil::StartsWith(argv[arg_no], "--")) {
        if (std::strcmp(argv[arg_no], "--new-stdout") == 0) {
            ++arg_no;
            if (arg_no == argc)
                Usage();
            new_stdout = argv[arg_no++];
        } else if (std::strcmp(argv[arg_no], "--timeout-in-seconds") == 0) {
            ++arg_no;
            if (arg_no == argc)
                Usage();
            if (not StringUtil::ToUnsigned(argv[arg_no++], &timeout_in_seconds))
                logger->error("The argument following \"--timeout-in-seconds\" must be a non-negative integer!");
        } else
            Usage();
    }

    if (arg_no == argc)
        Usage();

    const std::string command(argv[arg_no++]);

    std::vector<std::string> args;
    while (arg_no < argc)
        args.emplace_back(argv[arg_no++]);

    try {
        const int retcode(ExecUtil::Exec(command, args, "", new_stdout, "", timeout_in_seconds));
        if (retcode != 0) {
            std::cerr << "The executed script or binary failed with exit code " << retcode << "!\n";
            return EXIT_FAILURE;
        } else
            std::cerr << "The executed script or binary succeeded!\n";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
