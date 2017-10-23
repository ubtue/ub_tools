#include <iostream>
#include "util.h"


int main() {
    const int x = *(volatile int *)0;
    logger->error("We should *never* see this!" + std::to_string(x));
}
