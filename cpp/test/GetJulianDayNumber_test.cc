#include <iomanip>
#include <iostream>
#include "TimeUtil.h"


int main() {
    std::cout << std::setprecision(10) << TimeUtil::GetJulianDayNumber() << '\n';
    return 0;
}
