// Test harness for the HtmlParser class.
#include <iostream>
#include <cstdlib>
#include "FileUtil.h"
#include "HtmlParser.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " html_filename\n";
    std::exit(EXIT_FAILURE);
}


class Parser: public HtmlParser {
public:
    Parser(const std::string &document)
        : HtmlParser(document,
                     HtmlParser::EVERYTHING & ~(HtmlParser::WORD | HtmlParser::PUNCTUATION | HtmlParser::WHITESPACE)) { }
    virtual void notify(const HtmlParser::Chunk &chunk);
};


void Parser::notify(const HtmlParser::Chunk &chunk) {
    std::cout << chunk.toString() << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[1];
    if (argc != 2)
        Usage();

    try {
        const std::string input_filename(argv[1]);
        std::string html_document;
        if (not FileUtil::ReadString(input_filename, &html_document))
            ERROR("failed to read an HTML document from \"" + input_filename + "\"!");

        Parser parser(html_document);
        parser.parse();
    } catch (const std::exception &x) {
        ERROR(x.what());
    }
}
