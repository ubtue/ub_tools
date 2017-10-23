/** \file    MiscUtil.cc
 *  \brief   Declarations of miscellaneous utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2016,2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "MiscUtil.h"
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <cctype>
#include <unistd.h>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace MiscUtil {


char HexDigit(const unsigned value) {
    switch (value) {
    case 0:
        return '0';
    case 1:
        return '1';
    case 2:
        return '2';
    case 3:
        return '3';
    case 4:
        return '4';
    case 5:
        return '5';
    case 6:
        return '6';
    case 7:
        return '7';
    case 8:
        return '8';
    case 9:
        return '9';
    case 0xA:
        return 'A';
    case 0xB:
        return 'B';
    case 0xC:
        return 'C';
    case 0xD:
        return 'D';
    case 0xE:
        return 'E';
    case 0xF:
        return 'F';
    default:
        logger->error("in MiscUtil::HexDigit: invalid value " + std::to_string(value) + "!");
    }
}


namespace {


class TemplateScanner {
    std::string last_variable_name_, last_string_constant_, last_error_message_;
    unsigned line_no_;
    std::istream &input_;
    std::ostream &output_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
    bool in_syntax_;
public:
    enum TokenType { END_OF_INPUT, IF, ELSE, ENDIF, DEFINED, LOOP, ENDLOOP, VARIABLE_NAME, OPEN_PAREN, CLOSE_PAREN,
                     COMMA, EQUALS, NOT_EQUALS, STRING_CONSTANT, AND, OR, END_OF_SYNTAX, ERROR };
public:
    TemplateScanner(std::istream &input, std::ostream &output)
        : line_no_(1), input_(input), output_(output), in_syntax_(false) { }

    /** If "emit_output" non-syntax elements of the input will be copied to the output. */
    TokenType getToken(const bool emit_output);

    /** Only call this immediately after getToken() has returned VARIABLE_NAME. */
    const std::string &getLastVariableName() const { return last_variable_name_; }

    /** Only call this immediately after getToken() has returned STRING_CONSTANT. */
    const std::string &getLastStringConstant() const { return last_string_constant_; }

    /** Only call this immediately after getToken() has returned ERROR. */
    const std::string &getLastErrorMessage() const { return last_error_message_; }

    unsigned getLineNo() const { return line_no_; }

    /** \brief Repositions the input stream and sets the appropriate line number for that position. */
    void seek(const std::istream::streampos stream_position, const unsigned line_no);

    /** Skips over blank characters in the input stream w/o emitting anything to the output stream. */
    void skipWhitespace();

    std::istream::streampos getInputStreamPos() const { return input_.tellg(); }

    /** \return A string representation of "token". */
    static std::string TokenTypeToString(const TokenType token);
private:
    std::string extractKeywordCandidate();
    std::string extractVariableName();
    void extractStringConstant();

    /** \return Either a keyword token type or ERROR if we failed to recognise the keyword. */
    static TokenType MapStringToKeywordToken(const std::string &keyword_candidate);
};


TemplateScanner::TokenType TemplateScanner::getToken(const bool emit_output) {
    if (unlikely(input_.eof()))
        return END_OF_INPUT;
    if (unlikely(not last_error_message_.empty()))
        logger->error("in TemplateScanner::getToken: attempting to continue scanning after an error occurred!");

    for (;;) {
        int ch(input_.get());
        if (unlikely(ch == EOF)) {
            if (unlikely(in_syntax_))
                throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: unexpected EOF on line "
                                         + std::to_string(line_no_) + "!");
            return END_OF_INPUT;
        }
        if (ch == '\n')
            ++line_no_;

        if (in_syntax_) {
            if (ch == '(')
                return OPEN_PAREN;
            if (ch == ')')
                return CLOSE_PAREN;
            if (ch == ',')
                return COMMA;
            if (ch == '}') {
                in_syntax_ = false;
                return END_OF_SYNTAX;
            }
            if (ch == '=') {
                ch = input_.get();
                if (likely(ch == '='))
                    return EQUALS;
                else
                    throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: expected '=' after '=' on line "
                                             + std::to_string(line_no_) + "!");
            }
            if (ch == '!') {
                ch = input_.get();
                if (likely(ch == '='))
                    return NOT_EQUALS;
                else
                    throw std::runtime_error("in MiscUtil::TemplateScanner::getToken: expected '=' after '!' on line "
                                             + std::to_string(line_no_) + "!");
            }

            if (ch >= 'A' and ch <= 'Z') {
                input_.unget();
                const std::string keyword_candidate(extractKeywordCandidate());
                const TokenType token(MapStringToKeywordToken(keyword_candidate));
                if (unlikely(token == ERROR))
                    last_error_message_ = "unknown keyword " + keyword_candidate + " on line "
                                          + std::to_string(line_no_);
                return token;
            } else if (ch >= 'a' and ch <= 'z') {
                input_.unget();
                last_variable_name_ = extractVariableName();
                return VARIABLE_NAME;
            } else if (ch == '"') {
                extractStringConstant();
                return STRING_CONSTANT;
            }
        } else if (ch == '{') {
            if (input_.peek() == '{') {
                input_.get();
                output_.put('{');
            } else
                in_syntax_ = true;
        } else if (emit_output)
            output_.put(static_cast<char>(ch));
    }
}


std::string TemplateScanner::extractKeywordCandidate() {
    std::string keyword_candidate;
    int ch;
    while (ch = input_.get(), ch >= 'A' and ch <= 'Z')
        keyword_candidate += static_cast<char>(ch);
    input_.unget();

    return keyword_candidate;
}


void TemplateScanner::seek(const std::istream::streampos stream_position, const unsigned line_no) {
    input_.seekg(stream_position);
    if (unlikely(not input_))
        logger->error("in TemplateScanner::seek: this should never happen!");
    line_no_ = line_no;
}


void TemplateScanner::skipWhitespace() {
    for (int ch(input_.get()); ch != EOF and isspace(ch); ch = input_.get())
        /* Intentionally empty! */;
    input_.unget();
}


std::string TemplateScanner::TokenTypeToString(const TemplateScanner::TokenType token) {
    switch (token) {
    case END_OF_INPUT:
        return "END_OF_INPUT";
    case IF:
        return "IF";
    case ELSE:
        return "ELSE";
    case ENDIF:
        return "ENDIF";
    case DEFINED:
        return "DEFINED";
    case LOOP:
        return "LOOP";
    case ENDLOOP:
        return "ENDLOOP";
    case VARIABLE_NAME:
        return "VARIABLE_NAME";
    case OPEN_PAREN:
        return "OPEN_PAREN";
    case CLOSE_PAREN:
        return "CLOSE_PAREN";
    case COMMA:
        return "COMMA";
    case EQUALS:
        return "EQUALS";
    case NOT_EQUALS:
        return "NOT_EQUALS";
    case STRING_CONSTANT:
        return "STRING_CONSTANT";
    case AND:
        return "AND";
    case OR:
        return "OR";
    case END_OF_SYNTAX:
        return "END_OF_SYNTAX";
    case ERROR:
        return "ERROR";
    }

    logger->error("in TemplateScanner::TokenTypeToString: we should *never* get here!");
}


std::string TemplateScanner::extractVariableName() {
    std::string variable_name;
    int ch;
    while (ch = input_.get(), (ch >= 'a' and ch <= 'z') or ch == '_' or (ch >= '0' and ch <= '9'))
        variable_name += static_cast<char>(ch);
    input_.unget();

    return variable_name;
}


void TemplateScanner::extractStringConstant() {
    last_string_constant_.clear();

    int ch;
    while ((ch = input_.get()) != '"') {
        switch (ch) {
        case EOF:
            throw std::runtime_error("in TemplateScanner::extractStringConstant: unexpected EOF while parsing a "
                                     "string constant on line " + std::to_string(line_no_) + "!");
        case '\\': {
                ch = input_.get();
                if (unlikely(ch == EOF))
                    throw std::runtime_error("in TemplateScanner::extractStringConstant: unexpected EOF while "
                                             "parsing a string constant on line " + std::to_string(line_no_) + "!");
                if (ch == '\\')
                    last_string_constant_ += '\\';
                else if (ch == 'n')
                    last_string_constant_ += '\n';
                else if (ch == '"')
                    last_string_constant_ += '"';
                else
                    throw std::runtime_error("in TemplateScanner::extractStringConstant: illegal character after "
                                             "backslash in a string constant on line " + std::to_string(line_no_)
                                             + "!");
                break;
        } default:
            last_string_constant_ += static_cast<char>(ch);
        }
    }
}


TemplateScanner::TokenType TemplateScanner::MapStringToKeywordToken(const std::string &keyword_candidate) {
    static const std::map<std::string, TokenType> keywords_to_tokens_map{
        { "IF",      IF      },
        { "ELSE",    ELSE    },
        { "ENDIF",   ENDIF   },
        { "DEFINED", DEFINED },
        { "LOOP",    LOOP    },
        { "ENDLOOP", ENDLOOP },
        { "AND",     AND     },
        { "OR",      OR      },
    };

    const auto key_and_value(keywords_to_tokens_map.find(keyword_candidate));
    return key_and_value == keywords_to_tokens_map.cend() ? ERROR : key_and_value->second;
}


class Scope {
public:
    enum Type { TOP_LEVEL, IF, LOOP };
private:
    Type type_;
    unsigned start_line_number_, iteration_count_, loop_count_;
    std::istream::streampos start_stream_pos_;
    std::set<std::string> loop_vars_;
public:
    Type getType() const { return type_; }

    /** \return where an IF or a LOOP started. */
    unsigned getStartLineNumber() const { return start_line_number_; }

    bool isLoopVariable(const std::string &variable_name) const;
    unsigned getCurrentIterationCount() const;
    unsigned getLoopCount() const;
    void incIterationCount();
    std::istream::streampos getStartStreamPos() const;

    static Scope MakeTopLevelScope() { return Scope(TOP_LEVEL); }
    static Scope MakeIfScope(const unsigned start_line_number) { return Scope(IF, start_line_number); }
    static Scope MakeLoopScope(const unsigned start_line_number, const std::istream::streampos start_stream_pos,
                               const std::set<std::string> &loop_vars, const unsigned loop_count)
    { return Scope(LOOP, start_line_number, start_stream_pos, loop_vars, loop_count); }
private:
    explicit Scope(const Type type): type_(type) { }
    explicit Scope(const Type type, const unsigned start_line_number)
        : type_(type), start_line_number_(start_line_number) { }
    explicit Scope(const Type type, const unsigned start_line_number, const std::istream::streampos start_stream_pos,
                   const std::set<std::string> &loop_vars, const unsigned loop_count)
        : type_(type), start_line_number_(start_line_number), iteration_count_(0), loop_count_(loop_count),
          start_stream_pos_(start_stream_pos), loop_vars_(loop_vars) { }
    static std::string TypeToString(const Type type);
};


bool Scope::isLoopVariable(const std::string &variable_name) const {
    if (unlikely(type_ != LOOP))
        logger->error("in MiscUtil::Scope::isLoopVariable: this should never happen! (type is "
                      + TypeToString(type_) + ", variable is \"" + variable_name + "\")");

    return loop_vars_.find(variable_name) != loop_vars_.cend();
}


unsigned Scope::getCurrentIterationCount() const {
    if (unlikely(type_ != LOOP))
        logger->error("in MiscUtil::Scope::getCurrentIterationCount: this should never happen! (type is "
                      + TypeToString(type_) + ")");

    return iteration_count_;
}


unsigned Scope::getLoopCount() const {
    if (unlikely(type_ != LOOP))
        logger->error("in MiscUtil::Scope::getLoopCount: this should never happen!");

    return loop_count_;
}


void Scope::incIterationCount() {
    if (unlikely(type_ != LOOP))
        logger->error("in MiscUtil::Scope::incIterationCount: this should never happen!");

    ++iteration_count_;
}


std::istream::streampos Scope::getStartStreamPos() const {
    if (unlikely(type_ != LOOP))
        logger->error("in MiscUtil::Scope::getStartStreamPos: this should never happen!");

        return start_stream_pos_;
}


std::string Scope::TypeToString(const Type type) {
    switch (type) {
    case TOP_LEVEL:
        return "TOP_LEVEL";
    case IF:
        return "IF";
    case LOOP:
        return "LOOP";
    }

    logger->error("in Scope::TypeToString: we should *never* get here!");
}


// Returns true, if "variable_name" exists and can be accessed as a scalar based on the current scope.
bool GetScalarValue(const std::string &variable_name,
                    const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                    const std::vector<Scope> &active_scopes, std::string * const value)
{
    const auto &name_and_values(names_to_values_map.find(variable_name));
    if (name_and_values == names_to_values_map.cend())
        return false;

    if (name_and_values->second.size() == 1) { // We can always access this variable in a scalar context!
        *value = name_and_values->second[0];
        return true;
    }

    // Now deal w/ multivalued variables:
    for (auto scope(active_scopes.crbegin()); scope != active_scopes.crend(); ++scope) {
        if (scope->isLoopVariable(variable_name)) {
            *value = name_and_values->second[scope->getCurrentIterationCount()];
            return true;
        }
    }

    return false;
}


bool ParseIfCondition(TemplateScanner * const scanner,
                      const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                      const std::vector<Scope> &active_scopes, const bool parse_only = false)
{
    scanner->skipWhitespace();
    TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::DEFINED and token != TemplateScanner::VARIABLE_NAME))
        throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                 + std::to_string(scanner->getLineNo())
                                 + " DEFINED or variable name expected but found "
                                 + TemplateScanner::TokenTypeToString(token) + " instead!");

    bool expression_value;
    if (token == TemplateScanner::DEFINED) {
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::OPEN_PAREN))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " '(' expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");

        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " variable name expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
        expression_value = names_to_values_map.find(scanner->getLastVariableName())
                           != names_to_values_map.cend();

        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::CLOSE_PAREN))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo()) + " '(' expected but found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
    } else { // Comparison.
        std::string variable_name(scanner->getLastVariableName());
        std::string lhs;
        if (not parse_only and unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &lhs)))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        scanner->skipWhitespace();
        const TemplateScanner::TokenType operator_token(scanner->getToken(/* emit_output = */false));
        if (unlikely(operator_token != TemplateScanner::EQUALS and operator_token != TemplateScanner::NOT_EQUALS))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " \"==\" or \"!=\" expected after variable name!");

        scanner->skipWhitespace();
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME and token != TemplateScanner::STRING_CONSTANT))
            throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " variable name or string constant expected after comparison operator! ("
                                     "Found " + TemplateScanner::TokenTypeToString(token) + " instead.)");
        std::string rhs;
        if (token == TemplateScanner::STRING_CONSTANT)
            rhs = scanner->getLastStringConstant();
        else {
            variable_name = scanner->getLastVariableName();
            if (unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &rhs)))
                throw std::runtime_error("in MiscUtil::ParseIfCondition: error on line "
                                         + std::to_string(scanner->getLineNo())
                                         + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        }

        expression_value = (rhs == lhs);
        if (operator_token == TemplateScanner::NOT_EQUALS)
            expression_value = !expression_value;
    }

    return expression_value;
}


// \return The value of the conditional expression.
bool ParseIf(TemplateScanner * const scanner,
             const std::map<std::string, std::vector<std::string>> &names_to_values_map,
             const std::vector<Scope> &active_scopes)
{
    const bool condition1(ParseIfCondition(scanner, names_to_values_map, active_scopes));

    scanner->skipWhitespace();
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token == TemplateScanner::END_OF_SYNTAX))
        return condition1;

    if (unlikely(token != TemplateScanner::AND and token != TemplateScanner::OR))
        throw std::runtime_error("in MiscUtil::ParseIf: error on line " + std::to_string(scanner->getLineNo())
                                 + " '}' expected but found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");

    // Syntax check only:
    const auto start_of_condition(scanner->getInputStreamPos());
    const unsigned start_of_condition_lineno(scanner->getLineNo());
    ParseIfCondition(scanner, names_to_values_map, active_scopes, /*parse_only =*/true);

    if (token == TemplateScanner::AND) {
        if (not condition1)
            return false;
    } else { // token == TemplateScanner::OR
        if (condition1)
            return true;
    }
    scanner->seek(start_of_condition, start_of_condition_lineno);
    return ParseIfCondition(scanner, names_to_values_map, active_scopes);
}


/** \return False if "variable_name_candidate" is an unknown variable name, else true. */
bool GetVariableCardinality(const std::string variable_name_candidate,
                            const std::map<std::string, std::vector<std::string>> &names_to_values_map,
                            unsigned * const cardinality)
{
    const auto name_and_values(names_to_values_map.find(variable_name_candidate));
    if (unlikely(name_and_values == names_to_values_map.cend()))
        return false;
    *cardinality = name_and_values->second.size();
    return true;
}


void ParseLoop(TemplateScanner * const scanner, std::set<std::string> * const loop_vars, unsigned * const loop_count,
               const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    scanner->skipWhitespace();

    TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::VARIABLE_NAME))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + ": variable name expected, found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");
    std::string variable_name_candidate(scanner->getLastVariableName());
    if (unlikely(not GetVariableCardinality(variable_name_candidate, names_to_values_map, loop_count)))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + ": undefined loop variable \"" + variable_name_candidate + "\"!");
    loop_vars->insert(variable_name_candidate);

    while ((token = scanner->getToken(/* emit_output = */false)) == TemplateScanner::COMMA) {
        token = scanner->getToken(/* emit_output = */false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + ": variable name expected after comma, found "
                                     + TemplateScanner::TokenTypeToString(token) + " instead!");
        variable_name_candidate = scanner->getLastVariableName();
        unsigned cardinality;
        if (unlikely(not GetVariableCardinality(variable_name_candidate, names_to_values_map, &cardinality)))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + ": undefined loop variable \"" + variable_name_candidate + "\"!"
                                     + " (Possible loop variables are: " + StringUtil::Join(*loop_vars, ", ") + ")");
        if (unlikely(cardinality != *loop_count))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + " loop variables do not all have the same cardinality!");
        loop_vars->insert(variable_name_candidate);
    }

    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + " expected '}' at end of LOOP construct but found "
                                 + TemplateScanner::TokenTypeToString(token) + " instead!");
}


void ProcessEndOfSyntax(const std::string &name_of_syntactic_construct, TemplateScanner * const scanner) {
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */false));
    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("in MiscUtil::ProcessEndOfSyntax: error on line "
                                 + std::to_string(scanner->getLineNo()) + " expected '}' after "
                                 + name_of_syntactic_construct + " but found "
                                 + TemplateScanner::TokenTypeToString(token) + "!");
}


void SkipToToken(TemplateScanner * const scanner, TemplateScanner::TokenType target_token) {
    TemplateScanner::TokenType token;
    while ((token = scanner->getToken(false)) != target_token and token != TemplateScanner::END_OF_INPUT)
    { /* do nothing */ }
    if (token == TemplateScanner::END_OF_INPUT)
        throw std::runtime_error("in MiscUtil::SkipToToken: error on line "
                                 + std::to_string(scanner->getLineNo())
                                 + " expected '" + TemplateScanner::TokenTypeToString(token)
                                 + "' but none was found.");
}


} // unnamed namespace


void ExpandTemplate(std::istream &input, std::ostream &output,
                    const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    TemplateScanner scanner(input, output);
    std::vector<Scope> scopes;
    scopes.push_back(Scope::MakeTopLevelScope());

    std::stack<bool> skipping;
    TemplateScanner::TokenType token;
    while ((token = scanner.getToken(skipping.empty() or not skipping.top())) != TemplateScanner::END_OF_INPUT) {
        if (unlikely(token == TemplateScanner::ERROR))
            throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                     + std::to_string(scanner.getLineNo()) + ": " + scanner.getLastErrorMessage());
        if (token == TemplateScanner::IF) {
            const unsigned start_line_no(scanner.getLineNo());
            skipping.push(not ParseIf(&scanner, names_to_values_map, scopes));
            scopes.push_back(Scope::MakeIfScope(start_line_no));
        } else if (token == TemplateScanner::ELSE) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ELSE found w/o corresponding earlier IF!");
            skipping.top() = not skipping.top();
            ProcessEndOfSyntax("ELSE", &scanner);
        } else if (token == TemplateScanner::ENDIF) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ENDIF found w/o corresponding earlier IF!");
            scopes.pop_back();
            skipping.pop();
            ProcessEndOfSyntax("ENDIF", &scanner);
        } else if (token == TemplateScanner::LOOP) {
            std::set<std::string> loop_vars;
            unsigned loop_count;
            try {
                ParseLoop(&scanner, &loop_vars, &loop_count, names_to_values_map);
            } catch (const std::exception &x) {
                throw std::runtime_error("in MiscUtil::ExpandTemplate: " + std::string(x.what()));
            }
            const unsigned start_line_no(scanner.getLineNo());
            if (likely(loop_count > 0))
                scopes.push_back(Scope::MakeLoopScope(start_line_no, scanner.getInputStreamPos(), loop_vars,
                                                      loop_count));
            else
                SkipToToken(&scanner, TemplateScanner::ENDLOOP);

        } else if (token == TemplateScanner::ENDLOOP) {
            Scope &current_scope(scopes.back());
            if (unlikely(current_scope.getType() != Scope::LOOP))
                throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                         + std::to_string(scanner.getLineNo())
                                         + ": ENDLOOP found w/o corresponding earlier LOOP!");
            ProcessEndOfSyntax("ENDLOOP", &scanner);

            current_scope.incIterationCount();
            if (current_scope.getCurrentIterationCount() >= current_scope.getLoopCount())
                scopes.pop_back();
            else
                scanner.seek(current_scope.getStartStreamPos(), current_scope.getStartLineNumber());
        } else if (token == TemplateScanner::VARIABLE_NAME) {
            const std::string &last_variable_name(scanner.getLastVariableName());
            if (skipping.empty() or not skipping.top()) {
                std::string variable_value;
                if (not GetScalarValue(last_variable_name, names_to_values_map, scopes, &variable_value))
                    throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                             + std::to_string(scanner.getLineNo()) + ": found unexpected variable \""
                                             + last_variable_name + "\"!");
                output << variable_value;
            }
            ProcessEndOfSyntax("variable expansion", &scanner);
        }
    }

    const Scope &scope(scopes.back());
    switch (scope.getType()) {
    case Scope::TOP_LEVEL:
        return;
    case Scope::IF:
        throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                 + std::to_string(scanner.getLineNo()) + ": IF started on line "
                                 + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    case Scope::LOOP:
        throw std::runtime_error("in MiscUtil::ExpandTemplate: error on line "
                                 + std::to_string(scanner.getLineNo()) + ": LOOP started on line "
                                 + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    }
}


std::string ExpandTemplate(const std::string &template_string,
                           const std::map<std::string, std::vector<std::string>> &names_to_values_map)
{
    std::istringstream input(template_string);
    std::ostringstream expanded_template;
    ExpandTemplate(input, expanded_template, names_to_values_map);
    return expanded_template.str();
}


char GeneratePPNChecksumDigit(const std::string &ppn_without_checksum_digit) {
    if (unlikely(ppn_without_checksum_digit.length() != 8))
        throw std::runtime_error("in MiscUtil::GeneratePPNChecksumDigit: argument's length is not 8!");

    unsigned checksum(0);
    for (unsigned i(0); i < 8; ++i)
        checksum += (9 - i) * (ppn_without_checksum_digit[i] - '0');
    checksum = (11 - (checksum % 11)) % 11;

    return checksum == 10 ? 'X' : '0' + checksum;
}


bool IsValidPPN(const std::string &ppn_candidate) {
    if (ppn_candidate.length() != 9)
        return false;

    for (unsigned i(0); i < 8; ++i) {
        if (not StringUtil::IsDigit(ppn_candidate[i]))
            return false;
    }

    return ppn_candidate[8] == GeneratePPNChecksumDigit(ppn_candidate.substr(0, 8));
}


std::string GetEnv(const char * const name) {
    const char * const value(::getenv(name));
    if (value == nullptr)
        throw std::runtime_error("in MiscUtil::GetEnv: ::getenv(\"" + std::string(name) + "\") failed!");

    return value;
}


std::string SafeGetEnv(const char * const name) {
    const char * const value(::getenv(name));
    return value == nullptr ? "" : value;
}


void SetEnv(const std::string &name, const std::string &value, const bool overwrite) {
    if (unlikely(::setenv(name.c_str(), value.c_str(), overwrite ? 1 : 0) != 0))
        throw std::runtime_error("in MiscUtil::SetEnv: setenv(3) failed!");
}


bool EnvironmentVariableExists(const std::string &name) {
    const char * const value(::getenv(name.c_str()));
    return value != nullptr;
}


std::string GetUserName() {
    char username[200];
    if (unlikely(::getlogin_r(username, sizeof username) != 0))
        return "*unknown user* [" + std::string(::strerror(errno)) + "]";
    return username;
}


bool IsPossibleISSN(std::string issn_candidate) {
    if (issn_candidate.length() != 8 and issn_candidate.length() != 9)
        return false;
    if (issn_candidate.length() == 9) {
        if (issn_candidate[4] != '-')
            return false;
        issn_candidate = issn_candidate.substr(0, 4) + issn_candidate.substr(5, 4); // Remove hyphen.
    }

    //
    // For an explanation of how to determine the checksum digit, have a look at
    // https://en.wikipedia.org/wiki/International_Standard_Serial_Number#Code_format&oldid=767018094
    //

    unsigned sum(0), position(8);
    for (unsigned i(0); i < 7; ++i) {
        const char ch(issn_candidate[i]);
        if (not StringUtil::IsDigit(ch))
            return false;
        sum += position * (ch - '0');
        --position;
    }
    const unsigned modulus(sum % 11);

    char check_digit;
    if (modulus == 0)
        check_digit = '0';
    else {
        unsigned digit(11 - modulus);
        if (digit == 10)
            check_digit = 'X';
        else
            check_digit = '0' + digit;
    }

    return std::toupper(issn_candidate[7]) == check_digit;
}


bool NormaliseISSN(const std::string &issn_candidate, std::string * const normalised_issn) {
    if (issn_candidate.length() == 9) {
        *normalised_issn = issn_candidate;
        return true;
    }

    if (issn_candidate.length() == 8) {
        *normalised_issn = issn_candidate.substr(0, 4) + '-' + issn_candidate.substr(4, 4);
        return true;
    }

    return false;
}


std::string StringMapToString(const std::map<std::string, std::string> &map) {
    std::string map_as_string;
    for (const auto &key_and_value : map)
        map_as_string += key_and_value.first + "=" + key_and_value.second + ", ";

    return map_as_string.empty() ? "[]" : "[" + map_as_string.substr(0, map_as_string.length() - 2) + "]";
}


static unsigned GetLogSuffix(const std::string &log_file_prefix, const std::string &filename) {
    if (filename == log_file_prefix)
        return 0;

    unsigned generation;
    if (unlikely(not StringUtil::ToUnsigned(filename.substr(log_file_prefix.length() + 1), &generation)))
        logger->error("in GetLogSuffix(MiscUtil.cc): bad conversion, filename = \"" + filename + "\"!");

    return generation;
}


class LogCompare {
    const std::string log_file_prefix_;
public:
    explicit LogCompare(const std::string &log_file_prefix) : log_file_prefix_(log_file_prefix) { }
    bool operator()(const std::string &filename1, const std::string &filename2)
        { return GetLogSuffix(log_file_prefix_, filename1) < GetLogSuffix(log_file_prefix_, filename2); }
};


static std::string IncrementFile(const std::string &log_file_prefix, const std::string &filename) {
    if (filename == log_file_prefix)
        return log_file_prefix + ".1";

    return log_file_prefix + "." + std::to_string(GetLogSuffix(log_file_prefix, filename) + 1);
}


void LogRotate(const std::string &log_file_prefix, const unsigned max_count) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(log_file_prefix, &dirname, &basename);
    if (basename.empty()) {
        basename = dirname;
        dirname  = ".";
    }
    if (dirname.empty())
        dirname = ".";

    std::vector<std::string> filenames;
    FileUtil::Directory directory(dirname, "^" + basename + "(\\.[0-9]+)?$");
    for (const auto entry : directory) {
        if (entry.getType() == DT_REG)
            filenames.emplace_back(entry.getName());;
    }

    std::sort(filenames.begin(), filenames.end(), LogCompare(basename));

    if (max_count > 0) {
        while (filenames.size() > max_count) {
            const std::string path_to_delete(dirname + "/" + filenames.back());
            if (unlikely(not FileUtil::DeleteFile(path_to_delete)))
                logger->error("in MiscUtil::LogRotate: failed to delete \"" + path_to_delete + "\"!");
            filenames.pop_back();
        }
    }

    for (auto filename(filenames.rbegin()); filename != filenames.rend(); ++filename) {
        if (unlikely(not FileUtil::RenameFile(dirname + "/" + *filename,
                                              dirname + "/" + IncrementFile(basename, *filename))))
            logger->error("in MiscUtil::LogRotate:: failed to rename \"" + dirname + "/" + *filename + "\" to \""
                          + dirname + "/" + IncrementFile(basename, *filename) + "\"!");
    }
}


} // namespace MiscUtil
