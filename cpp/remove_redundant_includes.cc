/** \brief A tool to find and eliminate unused C++ includes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--report-only] file1 [file2 .. fileN]\n";
    std::exit(EXIT_FAILURE);
}


enum TokenType {
    STRING_CONSTANT, IDENT, OTHER_CHAR, CHARACTER_CONSTANT, END_OF_INPUT
};


class Scanner {
    File &input_;
    int pushed_back_chars_[2];
    unsigned pushed_back_char_count_;
    std::string last_ident_;
    char last_other_char_;
    std::string last_string_constant_;
    unsigned line_no_;
public:
    explicit Scanner(File * const input): input_(*input), pushed_back_char_count_(0), line_no_(1) { }
    TokenType getToken();
    inline const std::string &getLastIdent() const { return last_ident_; }
    inline char getLastOtherChar() const { return last_other_char_; }
    inline const std::string &getLastStringConstant() const { return last_string_constant_; }
    inline unsigned getCurrentLineNo() const { return line_no_; }
    inline void rewind() { line_no_ = 1; pushed_back_char_count_ = 0; }
private:
    void error(const std::string &msg) const;
    int get();
    void unget(int ch);
    void skipWhiteSpaceAndComments();
    void skipCharacterConstant();
    void extractStringConstant();
    void extractIdent();
};


TokenType Scanner::getToken() {
    skipWhiteSpaceAndComments();

    int ch(get());
    if (unlikely(ch == EOF))
        return END_OF_INPUT;

    if (ch == '\'') {
        skipCharacterConstant();
        return CHARACTER_CONSTANT;
    } else if (ch == '"') {
        extractStringConstant();
        return STRING_CONSTANT;
    } else if (ch == '_' or StringUtil::IsAsciiLetter(ch)) {
        unget(ch);
        extractIdent();
        return IDENT;
    }

    last_other_char_ = static_cast<char>(ch);
    return OTHER_CHAR;
}


void Scanner::error(const std::string &msg) const {
    errno = 0;
    logger->error("on line #" + std::to_string(line_no_) + ": " + msg);
}


int Scanner::get() {
    if (pushed_back_char_count_ > 0) {
        --pushed_back_char_count_;
        if (unlikely(pushed_back_chars_[pushed_back_char_count_] == '\n'))
            ++line_no_;
        return pushed_back_chars_[pushed_back_char_count_];
    }

    const int ch(input_.get());
    if (unlikely(ch == '\n'))
        ++line_no_;
    return ch;
}


void Scanner::unget(int ch) {
    if (unlikely(pushed_back_char_count_ == 2))
        logger->error("in Scanner::unget: can't unget more than 2 characters in a row!");
    pushed_back_chars_[pushed_back_char_count_++] = ch;
    if (unlikely(ch == '\n'))
        --line_no_;
}


void Scanner::skipWhiteSpaceAndComments() {
    int ch;
skip_white_space:
    for (;;) {
        ch = get();
        if (ch != ' ' and ch != '\t' and ch != '\n')
            break;
    }

    if (ch == '/') {
        ch = get();
        if (ch == '/') { // Single-line comment.
            // Skip to end-of-line:
            do
                ch = get();
            while (ch != '\n' and unlikely(ch != EOF));
            if (unlikely(ch == EOF))
                return;
        } else if (ch == '*') { // C-style comment.
            bool star_seen(false);
            for (;;) {
                ch = get();
                if (unlikely(ch == EOF))
                    logger->error("in Scanner::skipWhiteSpaceAndComments: unexpected EOF while looking for the end "
                                  "of a C-style comment!");
                if (star_seen) {
                    if (ch == '/')
                        break; // End of comment.
                }
                star_seen = ch == '*';
            }
        } else {
            unget(ch);
            unget('/');
            return;
        }
        goto skip_white_space;
    }

    if (likely(ch != EOF))
        unget(ch);
}


void Scanner::skipCharacterConstant() {
    int ch(get());
    if (unlikely(ch == EOF))
        error("unexpected EOF while parsing a character constant!");
    if (unlikely(ch == '\\')) {
        ch = get();
        if (unlikely(ch == EOF))
            error("unexpected EOF while parsing a character constant! (2)");
    }
    ch = get();
    if (unlikely(ch == EOF))
        error("unexpected EOF while parsing a character constant! (3)");
    if (unlikely(ch != '\''))
        error("expected closing quote at end of a character constant, found '"
              + std::string(1, static_cast<char>(ch)) + "' instead!");
}


void Scanner::extractIdent() {
    last_ident_.clear();
    for (;;) {
        const int ch(get());
        if (unlikely(ch == EOF))
            error("unexpected EOF while parsing an identifier or a keyword!");
        if (ch == '_' or StringUtil::IsAsciiLetter(ch) or StringUtil::IsDigit(ch))
            last_ident_ += static_cast<char>(ch);
        else {
            unget(ch);
            return;
        }
    }
}


void Scanner::extractStringConstant() {
    last_string_constant_.clear();
    bool escaped(false);
    for (;;) {
        int ch(get());
        if (unlikely(ch == EOF))
            error("unexpected EOF while parsing a string constant!");
        else if (unlikely(escaped))
            escaped = false;
        else if (unlikely(ch == '"'))
            return;
        else if (unlikely(ch == '\\'))
            escaped = true;
        last_string_constant_ += static_cast<char>(ch);
    }
}


void ExtractIncludes(Scanner * const scanner, std::vector<std::string> * const includes) {
    includes->clear();

    for (;;) {
        TokenType token(scanner->getToken());
        if (unlikely(token == END_OF_INPUT))
            return;
        if (token == OTHER_CHAR and scanner->getLastOtherChar() == '#') {
            token = scanner->getToken();
            if (token == IDENT and scanner->getLastIdent() == "include") {
                token = scanner->getToken();
                if (token == STRING_CONSTANT)
                    includes->emplace_back(scanner->getLastStringConstant());
            }
        }
    }
}


void RemoveUsedNamespaces(Scanner * const scanner, std::set<std::string> * const namespaces) {
    for (;;) {
        TokenType token(scanner->getToken());
        if (unlikely(token == END_OF_INPUT))
            return;
        if (token == IDENT) {
            const std::string ident(scanner->getLastIdent());
            if (namespaces->find(ident) == namespaces->cend())
                continue;
            token = scanner->getToken();
            if (unlikely(token == END_OF_INPUT))
                return;
            if (token == IDENT)
                namespaces->erase(ident);
            else if (token == OTHER_CHAR) {
                const char ch(scanner->getLastOtherChar());
                if (ch == ':' or ch == '&' or ch == '*')
                    namespaces->erase(ident);
            }
        }
    }
}


void ProcessFile(const bool report_only, File * const input, File * const /*output*/) {
    Scanner scanner(input);

    std::vector<std::string> includes;
    ExtractIncludes(&scanner, &includes);
    if (report_only) {
        std::cout << "Found the following local includes:\n";
        for (const auto &include : includes)
            std::cout << '\t' << include << '\n';
    }

    std::set<std::string> namespaces;
    for (const auto &include : includes) {
        if (include != "util.h" and StringUtil::EndsWith(include, ".h"))
            namespaces.emplace(include.substr(0, include.length() - 2));
    }
    
    input->rewind();
    scanner.rewind();

    RemoveUsedNamespaces(&scanner, &namespaces);
    if (not namespaces.empty()) {
        std::cout << "Unused namespaces:\n";
        for (const auto &name_space : namespaces)
            std::cout << '\t' << name_space << '\n';
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool report_only(false);
    if (std::strcmp(argv[1], "--report-only") == 0) {
        report_only = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    try {
        for (int arg_no(1); arg_no < argc; ++arg_no) {
            const std::string source_filename(argv[arg_no]);
            std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(source_filename));
            FileUtil::AutoTempFile temp_file;
            std::unique_ptr<File> output(FileUtil::OpenInputFileOrDie(temp_file.getFilePath()));
            ProcessFile(report_only, input.get(), output.get());
            output->flush();
            output->close();
        }
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
