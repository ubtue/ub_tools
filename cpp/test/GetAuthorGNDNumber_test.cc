// Test harness for LobidUtil::GetAuthorGNDNumber.
//
#include <iostream>
#include "LobidUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 2 and argc != 3)
        ::Usage("author_name [additional_Lucene_query_params]");

    std::string additional_Lucene_query_params;
    if (argc == 3)
        additional_Lucene_query_params = argv[2];

    std::cout << LobidUtil::GetAuthorGNDNumber(argv[1], additional_Lucene_query_params) << '\n';

    return EXIT_SUCCESS;
}
