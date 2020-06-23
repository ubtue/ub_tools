#include <iostream>
#include <cstring>
#include "HtmlUtil.h"
#include "util.h"


int main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("text_with_html_entities");

    std::cout << HtmlUtil::ReplaceEntitiesUTF8(argv[1]) << '\n';
}
