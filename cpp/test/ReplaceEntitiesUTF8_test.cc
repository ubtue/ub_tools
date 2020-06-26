#include <iostream>
#include <cstring>
#include "HtmlUtil.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("(--pass-through-unknown-entities|--delete-unknown-entities) text_with_html_entities");
}


int main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    HtmlUtil::UnknownEntityMode unknown_entity_mode;
    if (std::strcmp(argv[1], "--pass-through-unknown-entities") == 0)
        unknown_entity_mode = HtmlUtil::PASS_THROUGH_UNKNOWN_ENTITIES;
    else if (std::strcmp(argv[1], "--delete-unknown-entities") == 0)
        unknown_entity_mode = HtmlUtil::DELETE_UNKNOWN_ENTITIES;
    else
        Usage();

    std::cout << HtmlUtil::ReplaceEntitiesUTF8(argv[2], unknown_entity_mode) << '\n';
}
