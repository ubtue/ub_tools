// Test harness for the HtmlParser class.
#include <iostream>
#include <cstdlib>
#include "FileUtil.h"
#include "HtmlParser.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--http-header-charset=charset] html_filename\n";
    std::exit(EXIT_FAILURE);
}


class Parser : public HtmlParser {
public:
    Parser(const std::string &document, const std::string &http_header_charset)
        : HtmlParser(document, http_header_charset,
                     HtmlParser::EVERYTHING & ~(HtmlParser::WORD | HtmlParser::PUNCTUATION | HtmlParser::WHITESPACE)) { }
    virtual void notify(const HtmlParser::Chunk &chunk);
};


void Parser::notify(const HtmlParser::Chunk &chunk) {
    std::cout << chunk.toString() << '\n';
}


int main(int argc, char *argv[]) {
    ::progname = argv[1];

    std::string http_header_charset;
    if (argc == 3) {
        if (not StringUtil::StartsWith(argv[1], "--http-header-charset="))
            Usage();
        http_header_charset = argv[1] + __builtin_strlen("--http-header-charset=");
        --argc, ++argv;
    }

    if (argc != 2)
        Usage();

    try {
        const std::string input_filename(argv[1]);
        std::string html_document;
        if (not FileUtil::ReadString(input_filename, &html_document))
            LOG_ERROR("failed to read an HTML document from \"" + input_filename + "\"!");

        Parser parser(html_document, http_header_charset);
        parser.parse();
        std::cout << "Used HTTP header charset \"" << parser.getHttpHeaderCharset() << "\".\n";
        std::cout << "Used document local charset \"" << parser.getDocumentLocalCharset() << "\".\n";
    } catch (const std::exception &x) {
        LOG_ERROR(x.what());
    }
}
