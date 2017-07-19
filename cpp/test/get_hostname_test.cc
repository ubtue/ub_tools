#include <iostream>
#include <DnsUtil.h>


int main() {
    std::cout << DnsUtil::GetHostname() << '\n';
}
