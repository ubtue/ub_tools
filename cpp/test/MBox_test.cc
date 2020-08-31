#include <iostream>
#include <cstdlib>
#include "MBox.h"
#include "TimeUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("mbox_filename");

    MBox mbox(argv[1]);
    for (const auto &message : mbox) {
        std::cout << "reception time: " << TimeUtil::TimeTToString(message.getReceptionTime()) << '\n';
        std::cout << "original host:  " << message.getOriginalHost() << '\n';
        std::cout << "sender:         " << message.getSender() << '\n';
        std::cout << "subject:        " << message.getSubject() << '\n';
        std::cout << "message body:   " << message.getMessageBody() << '\n';
        std::cout << '\n';
    }

    return EXIT_SUCCESS;
}
