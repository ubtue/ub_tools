/** \brief Displays some facts about our local std::string implementation. */
#include <iostream>
#include <string>


int main() {
    std::string s;
    std::cout << "Capacity of an empty string: " << s.capacity() << '\n';
    std::cout << "Sizeof string: " << sizeof(s) << '\n';
#ifdef _GLIBCXX_USE_CXX11_ABI
    std::cout << "_GLIBCXX_USE_CXX11_ABI: "<< _GLIBCXX_USE_CXX11_ABI << '\n';
#else
    std::cout << "_GLIBCXX_USE_CXX11_ABI has not been defined.\n";
#endif
}
