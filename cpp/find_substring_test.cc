/** Test harness for TextUtil::FindSubstring().
 */
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <TextUtil.h>
#include <StringUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " haystack needle\n";
    std::cerr << "       \"haystack\" and \"needle\" must be colon-separated lists of words.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::vector<std::string> haystack;
    StringUtil::Split(argv[1], ':', &haystack);
    std::vector<std::string> needle;
    StringUtil::Split(argv[2], ':', &needle);

    const std::vector<std::string>::const_iterator start_iter(TextUtil::FindSubstring(haystack, needle));
    if (start_iter == haystack.cend())
        std::cout << "Needle was not found in haystack!\n";
    else
        std::cout << "Needle was found at offset " << (start_iter - haystack.cbegin()) << " in haystack.\n";
}

