#include <iostream>
#include <cstdlib>
#include "BibleReferenceParser.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << progname << " bible_reference_candidate [expected_pair1 expected_pair2 ..."
              << " expected_pairN]\n";
    std::cerr << "       When the expected pairs, where start and end have to be separated with a colon, are\n";
    std::cerr << "       provided, the program returns a non-zero exit code if not all pairs have been matched!\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc < 2)
        Usage();

    const std::string book_code("01");
    std::set<std::pair<std::string, std::string>> start_end;
    if (not ParseBibleReference(argv[1], book_code, &start_end)) {
        if (argc == 2)
            std::cerr << "Bad bible reference: " << argv[1] << '\n';
        std::exit(EXIT_FAILURE);
    }

    std::set<std::string> parsed_pairs;
    for (const auto &pair : start_end) {
        const std::string parsed_pair(pair.first + ":" + pair.second);
        if (argc == 2)
            std::cout << parsed_pair << '\n';
        else
            parsed_pairs.insert(parsed_pair);
    }

    if (argc == 2)
        return EXIT_SUCCESS;

    unsigned matched_count(0);
    for (int arg_no(2); arg_no < argc; ++arg_no) {
        if (parsed_pairs.find(argv[arg_no]) == parsed_pairs.end())
            return EXIT_FAILURE;
        ++matched_count;
    }

    return (matched_count == parsed_pairs.size()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
