/** \file    set_tad_access_flag.cc
 *  \brief   Sets a database entry for TAD accessability based on an email address.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cctype>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " (--update-all-users|user_ID)\n";
    std::exit(EXIT_FAILURE);
}


class PermissionParser {
    File * const input_;
    std::string last_string_constant_;
    unsigned current_line_number_;
public:
    enum TokenType { ALLOW, DENY, STRING_CONST, DASH, COLON, PIPE, COMMA, OPEN_SQUARE_BRACKET, CLOSE_SQUARE_BRACKET,
                     QUESTION_MARK, OTHER, END_OF_INPUT };
private:
    TokenType pushed_back_token_;
    bool token_has_been_pushed_back_;
public:
    explicit PermissionParser(File * const input)
        : input_(input), current_line_number_(1), token_has_been_pushed_back_(false) { }

    TokenType getToken();
    void ungetToken(const TokenType token);
    const std::string &getLastStringConstant() const { return last_string_constant_; }
    unsigned getCurrentLineNumber() const { return current_line_number_; }
    static std::string ToString(const TokenType token);
private:
    void skipToEndOfLine();
    void skipCommentsAndWhiteSpace();
    void readStringConstant();
    TokenType parseKeyword();
};


PermissionParser::TokenType PermissionParser::getToken() {
    if (token_has_been_pushed_back_) {
        token_has_been_pushed_back_ = false;
        return pushed_back_token_;
    }

    skipCommentsAndWhiteSpace();

    const int ch(input_->get());
    if (unlikely(ch == EOF))
        return END_OF_INPUT;

    if (ch == '-')
        return DASH;
    else if (ch == ':')
        return COLON;
    else if (ch == '|')
        return PIPE;
    else if (ch == ',')
        return COMMA;
    else if (ch == '[')
        return OPEN_SQUARE_BRACKET;
    else if (ch == ']')
        return CLOSE_SQUARE_BRACKET;
    else if (ch == '?')
        return QUESTION_MARK;
    else if (ch == '"') {
        readStringConstant();
        return STRING_CONST;
    } else if (ch == '!')
        return parseKeyword();
    else {
        skipToEndOfLine();
        return OTHER;
    }
}


void PermissionParser::ungetToken(const TokenType token) {
    if (unlikely(token_has_been_pushed_back_))
        throw std::runtime_error("can't push back two tokens in a row!");

    pushed_back_token_ = token;
    token_has_been_pushed_back_ = true;
}


void PermissionParser::skipToEndOfLine() {
    for (;;) {
        const int ch(input_->get());
        if (unlikely(ch == EOF))
            return;
        if (ch == '\n') {
            ++current_line_number_;
            return;
        }
    }
}
    

void PermissionParser::skipCommentsAndWhiteSpace() {
    while (not input_->eof()) {
        const int ch(input_->get());
        if (unlikely(ch == EOF))
            return;

        if (ch == '#') // Comment!
            skipToEndOfLine();
        else if (not std::isspace(ch)) {
            input_->putback(ch);
            return;
        } else if (ch == '\n')
            ++current_line_number_;
    }
}


void PermissionParser::readStringConstant() {
    last_string_constant_.clear();

    const unsigned starting_line_number(current_line_number_);
    for (;;) {
        const int ch(input_->get());
        if (unlikely(ch == EOF))
            throw std::runtime_error("unexpected EOF while trying to read a string constant which started on line "
                                     + std::to_string(starting_line_number) + "!");
        if (ch == '"')
            return;
        if (unlikely(ch == '\n'))
            ++current_line_number_;
        last_string_constant_ += static_cast<char>(ch);
    }
}


// Hopes to find "allow" or "deny", the only two keywords that our input file should contain.
PermissionParser::TokenType PermissionParser::parseKeyword() {
    std::string keyword;
    for (;;) {
        const int ch(input_->get());
        if (unlikely(ch == EOF))
            throw std::runtime_error("unexpected EOF while looking for a keyword!");
        if (std::islower(ch))
            keyword += static_cast<char>(ch);
        else {
            input_->putback(ch);
            if (keyword == "allow")
                return ALLOW;
            if (keyword == "deny")
                return DENY;
            throw std::runtime_error("unknown keyword \"" + keyword + "\" on line "
                                     + std::to_string(current_line_number_) + "!");
        }
    }
}


std::string PermissionParser::ToString(const TokenType token) {
    switch (token) {
    case ALLOW:
        return "ALLOW";
    case DENY:
        return "DENY";
    case STRING_CONST:
        return "STRING_CONST";
    case DASH:
        return "DASH";
    case COLON:
        return "COLON";
    case PIPE:
        return "PIPE";
    case COMMA:
        return "COMMA";
    case OPEN_SQUARE_BRACKET:
        return "OPEN_SQUARE_BRACKET";
    case CLOSE_SQUARE_BRACKET:
        return "CLOSE_SQUARE_BRACKET";
    case QUESTION_MARK:
        return "QUESTION_MARK";
    case OTHER:
        return "OTHER";
    case END_OF_INPUT:
        return "END_OF_INPUT";
    }

    Error("in PermissionParser::ToString: we should *never* get here!");
}


class Pattern {
    std::string pattern_;
    bool allow_;
public:
    Pattern(const std::string &pattern, const bool allow): pattern_(pattern), allow_(allow) { }
    bool matched(const std::string &test_string) const;
    bool allow() const { return allow_; }
};


bool Pattern::matched(const std::string &test_string) const {
    return StringUtil::EndsWith(test_string, pattern_);
}


// Positions the token stream to just before the next hyphen or, if no more hyphens are left,
// at the end of the input stream.
void SkipToNextDashOrEndOfInput(PermissionParser * const parser) {
    for (;;) {
        const PermissionParser::TokenType token = parser->getToken();
        if (token == PermissionParser::DASH) {
            parser->ungetToken(token);
            return;
        }
        if (token == PermissionParser::END_OF_INPUT)
            return;
    }
}


// Parses a single rule.  There are two types of rules.  Those with a single email pattern and those
// with a comma-separated list of patterns in square brackets.  Each rule is either an "allow" or a "deny" rule.
void ParseRule(PermissionParser * const parser, std::vector<Pattern> * const patterns) {
    PermissionParser::TokenType token(parser->getToken());
    if (unlikely(token != PermissionParser::ALLOW and token != PermissionParser::DENY))
        Error("on line " + std::to_string(parser->getCurrentLineNumber()) + " expected either ALLOW or DENY!");
    const bool allow(token == PermissionParser::ALLOW);

    token = parser->getToken();
    if (token == PermissionParser::STRING_CONST) {
        patterns->emplace_back(parser->getLastStringConstant(), allow);
        SkipToNextDashOrEndOfInput(parser);
    } else if (token == PermissionParser::QUESTION_MARK) {
        token = parser->getToken();
        if (unlikely(token != PermissionParser::OPEN_SQUARE_BRACKET))
            Error("on line "  + std::to_string(parser->getCurrentLineNumber()) + ": expected '[' but found "
                  + PermissionParser::ToString(token) + "!");
        for (;;) { // Parse the comma-separated list.
            token = parser->getToken();
            if (unlikely(token != PermissionParser::STRING_CONST))
                Error("on line "  + std::to_string(parser->getCurrentLineNumber())
                      + ": expected a string constant but found " + PermissionParser::ToString(token) + "!");
            patterns->emplace_back(parser->getLastStringConstant(), allow);

            token = parser->getToken();
            if (token == PermissionParser::CLOSE_SQUARE_BRACKET) {
                SkipToNextDashOrEndOfInput(parser);
                return;
            } else if (unlikely(token != PermissionParser::COMMA))
                Error("on line "  + std::to_string(parser->getCurrentLineNumber())
                      + ": expected ']' or ',' but found " + PermissionParser::ToString(token) + "!");
        }
    } else
        Error("on line " + std::to_string(parser->getCurrentLineNumber()) + " unexpected token "
              + PermissionParser::ToString(token) + "!");
}


void ParseEmailRules(File * const input, std::vector<Pattern> * const patterns) {
    PermissionParser parser(input);

    for (;;) {
        const PermissionParser::TokenType token(parser.getToken());
        if (token == PermissionParser::END_OF_INPUT)
            return;
        if (token == PermissionParser::DASH)
            ParseRule(&parser, patterns);
        else
            Error("unexpected token " + PermissionParser::ToString(token) + " on line "
                  + std::to_string(parser.getCurrentLineNumber()) + "!");
    }
}


bool CanUseTAD(const std::string &email_address, const std::vector<Pattern> &patterns) {
    for (const auto &pattern : patterns) {
        if (pattern.matched(email_address))
            return pattern.allow();
    }

    return false;
}


const std::string EMAIL_RULES_FILE("/usr/local/var/lib/tuelib/tad_email_acl.yaml");


void UpdateSingleUser(DbConnection * const db_connection, const std::vector<Pattern> &patterns,
                      const std::string &user_ID)
{
    db_connection->queryOrDie("SELECT email FROM user WHERE id=" + user_ID);
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        Error("No email address found for user ID " + user_ID + "!");
    const std::string email_address(result_set.getNextRow()["email"]);

    db_connection->queryOrDie("UPDATE ixtheo_user SET can_use_tad="
                              + std::string(CanUseTAD(email_address, patterns) ? "TRUE" : "FALSE")
                              + " WHERE id=" + user_ID);
}


void UpdateAllUsers(DbConnection * const db_connection, const std::vector<Pattern> &patterns) {
    db_connection->queryOrDie("SELECT id FROM ixtheo_user");
    DbResultSet result_set(db_connection->getLastResultSet());

    while (const DbRow row = result_set.getNextRow())
        UpdateSingleUser(db_connection, patterns, row["id"]);
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 2)
        Usage();

    try {
        const std::string flag_or_user_ID(argv[1]);

        std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(EMAIL_RULES_FILE));
        std::vector<Pattern> patterns;
        ParseEmailRules(input.get(), &patterns);

        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        if (flag_or_user_ID == "--update-all-users")
            UpdateAllUsers(&db_connection, patterns);
        else
            UpdateSingleUser(&db_connection, patterns, flag_or_user_ID);

    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
