/** \file    Template.cc
 *  \brief   Implementation of template-processing utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2016-2021 Universitätsbibliothek Tübingen
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
#include "Template.h"
#include <ios>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include "Compiler.h"
#include "DnsUtil.h"
#include "RegexMatcher.h"
#include "UrlUtil.h"
#include "util.h"


namespace Template {


ArrayValue::ArrayValue(const std::string &name, const std::vector<std::string> &values): Value(name) {
    for (unsigned i(0); i < values.size(); ++i)
        values_.emplace_back(std::shared_ptr<Value>(new ScalarValue(name + "[" + std::to_string(i) + "]", values[i])));
}


const std::shared_ptr<Value> &ArrayValue::operator[](const size_t index) const {
    if (unlikely(index >= values_.size()))
        throw std::out_of_range("in ArrayValue::operator[]: index (" + std::to_string(index) + ") >= size ("
                                + std::to_string(values_.size()) + ")!");
    return values_[index];
}


const Value *ArrayValue::getValueAt(const size_t index) const {
    return unlikely(index >= values_.size()) ? nullptr : values_[index].get();
}


namespace {


typedef const Function *PtrToConstFunction;


bool IsFunctionName(const std::string &name, const std::vector<Function *> &functions, PtrToConstFunction *function) {
    for (auto func : functions) {
        if (func->getName() == name) {
            *function = func;
            return true;
        }
    }

    *function = nullptr;
    return false;
}


class TemplateScanner {
    std::string last_variable_name_, last_string_constant_, last_error_message_;
    const Function *last_function_;
    unsigned line_no_;
    std::istream &input_;
    std::ostream &output_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
    bool in_syntax_;
    const std::vector<Function *> &functions_;

public:
    enum TokenType {
        END_OF_INPUT,
        IF,
        ELSE,
        ENDIF,
        DEFINED,
        LOOP,
        ENDLOOP,
        VARIABLE_NAME,
        FUNCTION_NAME,
        OPEN_PAREN,
        CLOSE_PAREN,
        COMMA,
        EQUALS,
        NOT_EQUALS,
        STRING_CONSTANT,
        AND,
        OR,
        END_OF_SYNTAX,
        ERROR
    };

public:
    TemplateScanner(std::istream &input, std::ostream &output, const std::vector<Function *> &functions)
        : last_function_(nullptr), line_no_(1), input_(input), output_(output), in_syntax_(false), functions_(functions) { }

    /** If "emit_output" non-syntax elements of the input will be copied to the output. */
    TokenType getToken(const bool emit_output);

    /** Only call this immediately after getToken() has returned VARIABLE_NAME. */
    inline const std::string &getLastVariableName() const { return last_variable_name_; }

    /** Only call this immediately after getToken() has returned FUNCTION_NAME. */
    inline const Function *getLastFunction() const { return last_function_; }

    /** Only call this immediately after getToken() has returned STRING_CONSTANT. */
    inline const std::string &getLastStringConstant() const { return last_string_constant_; }

    /** Only call this immediately after getToken() has returned ERROR. */
    inline const std::string &getLastErrorMessage() const { return last_error_message_; }

    inline unsigned getLineNo() const { return line_no_; }

    /** \brief Repositions the input stream and sets the appropriate line number for that position. */
    void seek(const std::streampos stream_position, const unsigned line_no);

    /** Skips over blank characters in the input stream w/o emitting anything to the output stream. */
    void skipWhitespace();

    inline std::streampos getInputStreamPos() const { return input_.tellg(); }

    /** \return A string representation of "token". */
    static std::string TokenTypeToString(const TokenType token);

private:
    std::string extractKeywordOrFunctionNameCandidate();
    std::string extractVariableName();
    void extractStringConstant();

    /** \return Either a keyword token type or ERROR if we failed to recognise the keyword. */
    static TokenType MapStringToKeywordToken(const std::string &keyword_candidate);
};


TemplateScanner::TokenType TemplateScanner::getToken(const bool emit_output) {
    if (unlikely(input_.eof()))
        return END_OF_INPUT;
    if (unlikely(not last_error_message_.empty()))
        LOG_ERROR("attempting to continue scanning after an error occurred!");

    for (;;) {
        int ch(input_.get());
        if (unlikely(ch == EOF)) {
            if (unlikely(in_syntax_))
                throw std::runtime_error("in Template::TemplateScanner::getToken: unexpected EOF on line " + std::to_string(line_no_)
                                         + "!");
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
                    throw std::runtime_error("in Template::TemplateScanner::getToken: expected '=' after '=' on line "
                                             + std::to_string(line_no_) + "!");
            }
            if (ch == '!') {
                ch = input_.get();
                if (likely(ch == '='))
                    return NOT_EQUALS;
                else
                    throw std::runtime_error("in Template::TemplateScanner::getToken: expected '=' after '!' on line "
                                             + std::to_string(line_no_) + "!");
            }

            if (ch >= 'A' and ch <= 'Z') {
                input_.unget();
                const std::string keyword_or_function_name_candidate(extractKeywordOrFunctionNameCandidate());
                const TokenType token(MapStringToKeywordToken(keyword_or_function_name_candidate));
                if (unlikely(token == ERROR)) {
                    if (IsFunctionName(keyword_or_function_name_candidate, functions_, &last_function_))
                        return FUNCTION_NAME;

                    last_error_message_ =
                        "unknown keyword/function name " + keyword_or_function_name_candidate + " on line " + std::to_string(line_no_);
                }
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
                if (emit_output)
                    output_.put('{');
            } else
                in_syntax_ = true;
        } else if (emit_output)
            output_.put(static_cast<char>(ch));
    }
}


std::string TemplateScanner::extractKeywordOrFunctionNameCandidate() {
    std::string keyword_or_function_name_candidate;
    int ch;
    while (ch = input_.get(), (ch >= 'A' and ch <= 'Z') or (ch >= 'a' and ch <= 'z'))
        keyword_or_function_name_candidate += static_cast<char>(ch);
    input_.unget();

    return keyword_or_function_name_candidate;
}


void TemplateScanner::seek(const std::streampos stream_position, const unsigned line_no) {
    input_.seekg(stream_position);
    if (unlikely(not input_))
        LOG_ERROR("this should never happen!");
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
    case FUNCTION_NAME:
        return "FUNCTION_NAME";
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

    LOG_ERROR("we should *never* get here!");
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
            throw std::runtime_error(
                "in TemplateScanner::extractStringConstant: unexpected EOF while parsing a "
                "string constant on line "
                + std::to_string(line_no_) + "!");
        case '\\': {
            ch = input_.get();
            if (unlikely(ch == EOF))
                throw std::runtime_error(
                    "in TemplateScanner::extractStringConstant: unexpected EOF while "
                    "parsing a string constant on line "
                    + std::to_string(line_no_) + "!");
            if (ch == '\\')
                last_string_constant_ += '\\';
            else if (ch == 'n')
                last_string_constant_ += '\n';
            else if (ch == '"')
                last_string_constant_ += '"';
            else
                throw std::runtime_error(
                    "in TemplateScanner::extractStringConstant: illegal character after "
                    "backslash in a string constant on line "
                    + std::to_string(line_no_) + "!");
            break;
        }
        default:
            last_string_constant_ += static_cast<char>(ch);
        }
    }
}


TemplateScanner::TokenType TemplateScanner::MapStringToKeywordToken(const std::string &keyword_candidate) {
    static const std::map<std::string, TokenType> keywords_to_tokens_map{
        { "IF", IF },     { "ELSE", ELSE },       { "ENDIF", ENDIF }, { "DEFINED", DEFINED },
        { "LOOP", LOOP }, { "ENDLOOP", ENDLOOP }, { "AND", AND },     { "OR", OR },
    };

    const auto key_and_value(keywords_to_tokens_map.find(keyword_candidate));
    return key_and_value == keywords_to_tokens_map.cend() ? ERROR : key_and_value->second;
}


class Scope {
public:
    enum Type { TOP_LEVEL, IF, LOOP };

private:
    Type type_;
    bool skipping_;
    unsigned start_line_number_, iteration_count_, loop_count_;
    std::streampos start_stream_pos_;
    std::set<std::string> loop_vars_;

public:
    Type getType() const { return type_; }

    bool skipping() const { return skipping_; }
    void toggleSkipping() { skipping_ = not skipping_; }

    /** \return where an IF or a LOOP started. */
    unsigned getStartLineNumber() const { return start_line_number_; }

    bool isLoopVariable(const std::string &variable_name) const;
    unsigned getCurrentIterationCount() const;
    unsigned getLoopCount() const;
    void incIterationCount();
    std::streampos getStartStreamPos() const;

    static Scope MakeTopLevelScope() { return Scope(TOP_LEVEL); }
    static Scope MakeIfScope(const unsigned start_line_number, const bool skipping) { return Scope(IF, start_line_number, skipping); }
    static Scope MakeLoopScope(const unsigned start_line_number, const std::streampos start_stream_pos,
                               const std::set<std::string> &loop_vars, const unsigned loop_count) {
        return Scope(LOOP, start_line_number, start_stream_pos, loop_vars, loop_count);
    }

private:
    explicit Scope(const Type type): type_(type), skipping_(false) { }
    explicit Scope(const Type type, const unsigned start_line_number, const bool skipping)
        : type_(type), skipping_(skipping), start_line_number_(start_line_number) { }
    explicit Scope(const Type type, const unsigned start_line_number, const std::streampos start_stream_pos,
                   const std::set<std::string> &loop_vars, const unsigned loop_count)
        : type_(type), skipping_(false), start_line_number_(start_line_number), iteration_count_(0), loop_count_(loop_count),
          start_stream_pos_(start_stream_pos), loop_vars_(loop_vars) { }
    static std::string TypeToString(const Type type);
};


bool Scope::isLoopVariable(const std::string &variable_name) const {
    if (unlikely(type_ != LOOP))
        LOG_ERROR("this should never happen! (type is " + TypeToString(type_) + ", variable is \"" + variable_name + "\")");

    return loop_vars_.find(variable_name) != loop_vars_.cend();
}


unsigned Scope::getCurrentIterationCount() const {
    if (unlikely(type_ != LOOP))
        LOG_ERROR("this should never happen! (type is " + TypeToString(type_) + ")");

    return iteration_count_;
}


unsigned Scope::getLoopCount() const {
    if (unlikely(type_ != LOOP))
        LOG_ERROR("this should never happen!");

    return loop_count_;
}


void Scope::incIterationCount() {
    if (unlikely(type_ != LOOP))
        LOG_ERROR("this should never happen!");

    ++iteration_count_;
}


std::streampos Scope::getStartStreamPos() const {
    if (unlikely(type_ != LOOP))
        LOG_ERROR("this should never happen!");

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

    LOG_ERROR("we should *never* get here!");
}


const Value *GetArrayValue(const std::vector<Scope> &active_scopes, const std::string &variable_name, const Value *value) {
    for (const auto &scope : active_scopes) {
        if (scope.getType() == Scope::LOOP and scope.isLoopVariable(variable_name)) {
            const ArrayValue * const array(dynamic_cast<const ArrayValue *>(value));
            if (array == nullptr)
                return nullptr;
            value = array->getValueAt(scope.getCurrentIterationCount());
        }
    }

    return value;
}


// Returns NULL if "variable_name" does not exists or the value as seen within the active scope.
const Value *GetScopedValue(const std::string &variable_name, const Map &names_to_values_map, const std::vector<Scope> &active_scopes) {
    const auto &name_and_values(names_to_values_map.find(variable_name));
    if (name_and_values == names_to_values_map.end())
        return nullptr;

    // If we have a scalar we have no problem:
    if (ScalarValue *scalar = dynamic_cast<ScalarValue *>(name_and_values->second.get()))
        return scalar;

    // Now deal w/ multivalued variables:
    return GetArrayValue(active_scopes, variable_name, name_and_values->second.get());
}


// Returns true, if "variable_name" exists and can be accessed as a scalar based on the current scope.
bool GetScalarValue(const std::string &variable_name, const Map &names_to_values_map, const std::vector<Scope> &active_scopes,
                    std::string * const value) {
    const Value *scoped_value(GetScopedValue(variable_name, names_to_values_map, active_scopes));
    if (scoped_value == nullptr)
        return false;

    // If we have a scalar we have no problem:
    if (const ScalarValue *scalar = dynamic_cast<const ScalarValue *>(scoped_value)) {
        *value = scalar->getValue();
        return true;
    }

    // If we get here, "value" is array-valued.
    return false;
}


bool ParseIfCondition(TemplateScanner * const scanner, const Map &names_to_values_map, const std::vector<Scope> &active_scopes,
                      const bool parse_only = false) {
    scanner->skipWhitespace();
    TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */ false));
    if (unlikely(token != TemplateScanner::DEFINED and token != TemplateScanner::VARIABLE_NAME))
        throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                 + " DEFINED or variable name expected but found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");

    bool expression_value;
    if (token == TemplateScanner::DEFINED) {
        token = scanner->getToken(/* emit_output = */ false);
        if (unlikely(token != TemplateScanner::OPEN_PAREN))
            throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                     + " '(' expected but found " + TemplateScanner::TokenTypeToString(token) + " instead!");

        token = scanner->getToken(/* emit_output = */ false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                     + " variable name expected but found " + TemplateScanner::TokenTypeToString(token) + " instead!");
        expression_value = names_to_values_map.find(scanner->getLastVariableName()) != names_to_values_map.end();

        token = scanner->getToken(/* emit_output = */ false);
        if (unlikely(token != TemplateScanner::CLOSE_PAREN))
            throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                     + " '(' expected but found " + TemplateScanner::TokenTypeToString(token) + " instead!");
    } else { // Comparison.
        std::string variable_name(scanner->getLastVariableName());
        std::string lhs;
        if (not parse_only and unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &lhs)))
            throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                     + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        scanner->skipWhitespace();
        const TemplateScanner::TokenType operator_token(scanner->getToken(/* emit_output = */ false));
        if (unlikely(operator_token != TemplateScanner::EQUALS and operator_token != TemplateScanner::NOT_EQUALS))
            throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                     + " \"==\" or \"!=\" expected after variable name!");

        scanner->skipWhitespace();
        token = scanner->getToken(/* emit_output = */ false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME and token != TemplateScanner::STRING_CONSTANT))
            throw std::runtime_error("in Template::ParseIfCondition: error on line "
                                     + std::to_string(scanner->getLineNo())
                                     + " variable name or string constant expected after comparison operator! ("
                                     "Found " + TemplateScanner::TokenTypeToString(token) + " instead.)");
        std::string rhs;
        if (token == TemplateScanner::STRING_CONSTANT)
            rhs = scanner->getLastStringConstant();
        else {
            variable_name = scanner->getLastVariableName();
            if (unlikely(not GetScalarValue(variable_name, names_to_values_map, active_scopes, &rhs)))
                throw std::runtime_error("in Template::ParseIfCondition: error on line " + std::to_string(scanner->getLineNo())
                                         + " unknown or non-scalar variable name \"" + variable_name + "\"!");
        }

        expression_value = (rhs == lhs);
        if (operator_token == TemplateScanner::NOT_EQUALS)
            expression_value = !expression_value;
    }

    return expression_value;
}


// \return The value of the conditional expression.
bool ParseIf(TemplateScanner * const scanner, const Map &names_to_values_map, const std::vector<Scope> &active_scopes) {
    const bool condition1(ParseIfCondition(scanner, names_to_values_map, active_scopes));

    scanner->skipWhitespace();
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */ false));
    if (unlikely(token == TemplateScanner::END_OF_SYNTAX))
        return condition1;

    if (unlikely(token != TemplateScanner::AND and token != TemplateScanner::OR))
        throw std::runtime_error("in Template::ParseIf: error on line " + std::to_string(scanner->getLineNo()) + " '}' expected but found "
                                 + TemplateScanner::TokenTypeToString(token) + " instead!");

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


void ParseLoop(TemplateScanner * const scanner, std::set<std::string> * const loop_vars, size_t * const loop_count,
               const Map &names_to_values_map, const std::vector<Scope> &active_scopes) {
    scanner->skipWhitespace();

    TemplateScanner::TokenType token;
    *loop_count = 0;
    do {
        token = scanner->getToken(/* emit_output = */ false);
        if (unlikely(token != TemplateScanner::VARIABLE_NAME))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + ": variable name expected after comma or LOOP, found " + TemplateScanner::TokenTypeToString(token)
                                     + " instead!");
        const std::string variable_name_candidate(scanner->getLastVariableName());
        const auto name_and_values(names_to_values_map.find(variable_name_candidate));
        if (unlikely(name_and_values == names_to_values_map.end()))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": undefined loop variable \""
                                     + variable_name_candidate + "\"!");
        const Value *value(GetArrayValue(active_scopes, variable_name_candidate, name_and_values->second.get()));
        const ArrayValue * const array_value(dynamic_cast<const ArrayValue * const>(value));
        if (unlikely(array_value == nullptr))
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": loop variable \""
                                     + variable_name_candidate + "\" is scalar in this context!");
        if (*loop_count == 0)
            *loop_count = array_value->size();
        else if (*loop_count != array_value->size())
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + ": all loop variables must have the same cardinality!");
        loop_vars->emplace(variable_name_candidate);
    } while ((token = scanner->getToken(/* emit_output = */ false)) == TemplateScanner::COMMA);

    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                 + " expected '}' at end of LOOP construct but found " + TemplateScanner::TokenTypeToString(token)
                                 + " instead!");
}


void ParseFunctionCall(TemplateScanner * const scanner, const Map &names_to_values_map, const std::vector<Scope> &active_scopes,
                       const Function * const function, std::ostream &output, const bool emit_output) {
    scanner->skipWhitespace();
    TemplateScanner::TokenType token(scanner->getToken(emit_output));
    if (token == TemplateScanner::OPEN_PAREN)
        std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": expected opening parenthesis after function name!");

    // Collect the function arguments:
    std::vector<const Value *> args;
    for (;;) {
        token = scanner->getToken(emit_output);
        if (token == TemplateScanner::CLOSE_PAREN) {
            if (args.empty())
                break;
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo())
                                     + ": unexpected closing parenthesis in function call!");
        } else if (token == TemplateScanner::VARIABLE_NAME) {
            const std::string &variable_name(scanner->getLastVariableName());
            const Value *value(GetScopedValue(variable_name, names_to_values_map, active_scopes));
            if (value == nullptr)
                throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": function argument variable \""
                                         + variable_name + " is not a known variable!");
            args.emplace_back(value);
        } else
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": unexpected junk in function call! (1)");

        token = scanner->getToken(emit_output);
        if (token == TemplateScanner::CLOSE_PAREN)
            break; // End of argument list.
        if (token != TemplateScanner::COMMA)
            throw std::runtime_error("error on line " + std::to_string(scanner->getLineNo()) + ": unexpected junk in function call! (2)");
    }

    if (emit_output)
        output << function->call(args);
}


void ProcessEndOfSyntax(const std::string &name_of_syntactic_construct, TemplateScanner * const scanner) {
    const TemplateScanner::TokenType token(scanner->getToken(/* emit_output = */ false));
    if (unlikely(token != TemplateScanner::END_OF_SYNTAX))
        throw std::runtime_error("in Template::ProcessEndOfSyntax: error on line " + std::to_string(scanner->getLineNo())
                                 + " expected '}' after " + name_of_syntactic_construct + " but found "
                                 + TemplateScanner::TokenTypeToString(token) + "!");
}


void SkipToToken(TemplateScanner * const scanner, TemplateScanner::TokenType target_token) {
    TemplateScanner::TokenType token;
    while ((token = scanner->getToken(false)) != target_token and token != TemplateScanner::END_OF_INPUT)
        /* do nothing */;
    if (token == TemplateScanner::END_OF_INPUT)
        throw std::runtime_error("in Template::SkipToToken: error on line " + std::to_string(scanner->getLineNo()) + " expected '"
                                 + TemplateScanner::TokenTypeToString(token) + "' but none was found.");
}


class LengthFunc : public Function {
public:
    explicit LengthFunc(): Function("Length", { Function::ArgDesc("vector-valued variable name") }) { }
    virtual std::string call(const std::vector<const Value *> &arguments) const final;
};


std::string LengthFunc::call(const std::vector<const Value *> &arguments) const {
    if (arguments.size() != 1)
        throw std::invalid_argument(name_ + " must be called w/ precisely one argument!");

    return std::to_string(arguments[0]->size());
}


class UrlEncodeFunc : public Function {
public:
    explicit UrlEncodeFunc(): Function("UrlEncode", { Function::ArgDesc("scalar-valued variable name") }) { }
    virtual std::string call(const std::vector<const Value *> &arguments) const final;
};


std::string UrlEncodeFunc::call(const std::vector<const Value *> &arguments) const {
    if (arguments.size() != 1)
        throw std::invalid_argument(name_ + " must be called w/ precisely one argument!");

    const ScalarValue *scalar_value(dynamic_cast<const ScalarValue *>(arguments[0]));
    if (scalar_value == nullptr)
        throw std::invalid_argument("argument to " + name_ + " must be a scalar!");

    return UrlUtil::UrlEncode(scalar_value->getValue());
}


class HostnameFunc : public Function {
public:
    explicit HostnameFunc(): Function("Hostname", { Function::ArgDesc("scalar-valued variable name") }) { }
    virtual std::string call(const std::vector<const Value *> &arguments) const final;
};


std::string HostnameFunc::call(const std::vector<const Value *> &arguments) const {
    if (arguments.size() != 0)
        throw std::invalid_argument(name_ + " must be called w/o argument!");

    return DnsUtil::GetHostname();
}


class RegexMatchFunc : public Function {
public:
    explicit RegexMatchFunc()
        : Function("RegexMatch", { Function::ArgDesc("scalar-valued PCRE"), Function::ArgDesc("String to be matched") }) { }
    virtual std::string call(const std::vector<const Value *> &arguments) const final;
};


std::string RegexMatchFunc::call(const std::vector<const Value *> &arguments) const {
    if (arguments.size() != 2)
        throw std::invalid_argument(name_ + " must be called w/ precisely two arguments!");

    const ScalarValue *regex(dynamic_cast<const ScalarValue *>(arguments[0]));
    if (regex == nullptr)
        throw std::invalid_argument("first argument to " + name_ + " must be a scalar!");

    const ScalarValue *subject(dynamic_cast<const ScalarValue *>(arguments[1]));
    if (subject == nullptr)
        throw std::invalid_argument("second argument to " + name_ + " must be a scalar!");

    std::string err_msg;
    size_t start_pos, end_pos;
    if (not RegexMatcher::Matched(regex->getValue(), subject->getValue(), /* options */ 0, &err_msg, &start_pos, &end_pos)) {
        if (not err_msg.empty())
            throw std::invalid_argument("possible bad regex \"" + regex->getValue() + "\": " + err_msg);
        return "";
    }

    return subject->getValue().substr(start_pos, end_pos - start_pos - 1);
}


// Returns true if the innermost of one of the upper scopes are set to skipping.
bool Skipping(const std::vector<Scope> &scopes) {
    for (auto scope(scopes.rbegin()); scope != scopes.rend(); ++scope) {
        if (scope->skipping())
            return true;
    }

    return false;
}


} // unnamed namespace


void ExpandTemplate(std::istream &input, std::ostream &output, const Map &names_to_values_map, const std::vector<Function *> &functions) {
    if (unlikely(not input))
        LOG_ERROR("input is bad!");
    if (unlikely(not output))
        LOG_ERROR("input is bad!");
    std::vector<Function *> all_functions(functions);
    all_functions.emplace_back(new LengthFunc());
    all_functions.emplace_back(new UrlEncodeFunc());
    all_functions.emplace_back(new HostnameFunc());
    all_functions.emplace_back(new RegexMatchFunc());

    TemplateScanner scanner(input, output, all_functions);
    std::vector<Scope> scopes;
    scopes.push_back(Scope::MakeTopLevelScope());

    TemplateScanner::TokenType token;
    while ((token = scanner.getToken(not Skipping(scopes))) != TemplateScanner::END_OF_INPUT) {
        if (unlikely(token == TemplateScanner::ERROR))
            throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo()) + ": "
                                     + scanner.getLastErrorMessage());
        if (token == TemplateScanner::IF) {
            const unsigned start_line_no(scanner.getLineNo());
            const bool skipping(not ParseIf(&scanner, names_to_values_map, scopes));
            scopes.push_back(Scope::MakeIfScope(start_line_no, skipping));
        } else if (token == TemplateScanner::ELSE) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                         + ": ELSE found w/o corresponding earlier IF!");
            scopes.back().toggleSkipping();
            ProcessEndOfSyntax("ELSE", &scanner);
        } else if (token == TemplateScanner::ENDIF) {
            if (unlikely(scopes.back().getType() != Scope::IF))
                throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                         + ": ENDIF found w/o corresponding earlier IF!");
            scopes.pop_back();
            ProcessEndOfSyntax("ENDIF", &scanner);
        } else if (token == TemplateScanner::LOOP) {
            std::set<std::string> loop_vars;
            size_t loop_count;
            try {
                ParseLoop(&scanner, &loop_vars, &loop_count, names_to_values_map, scopes);
            } catch (const std::exception &x) {
                throw std::runtime_error("in Template::ExpandTemplate: " + std::string(x.what()));
            }
            const unsigned start_line_no(scanner.getLineNo());
            if (likely(loop_count > 0))
                scopes.push_back(Scope::MakeLoopScope(start_line_no, scanner.getInputStreamPos(), loop_vars, loop_count));
            else
                SkipToToken(&scanner, TemplateScanner::ENDLOOP);
        } else if (token == TemplateScanner::ENDLOOP) {
            Scope &current_scope(scopes.back());
            if (unlikely(current_scope.getType() != Scope::LOOP))
                throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                         + ": ENDLOOP found w/o corresponding earlier LOOP!");
            ProcessEndOfSyntax("ENDLOOP", &scanner);

            current_scope.incIterationCount();
            if (current_scope.getCurrentIterationCount() >= current_scope.getLoopCount())
                scopes.pop_back();
            else
                scanner.seek(current_scope.getStartStreamPos(), current_scope.getStartLineNumber());
        } else if (token == TemplateScanner::VARIABLE_NAME) {
            const std::string &last_variable_name(scanner.getLastVariableName());
            if (not Skipping(scopes)) {
                std::string variable_value;
                if (not GetScalarValue(last_variable_name, names_to_values_map, scopes, &variable_value))
                    throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                             + ": found unexpected variable \"" + last_variable_name + "\"!");
                output << variable_value;
            }
            ProcessEndOfSyntax("variable expansion", &scanner);
        } else if (token == TemplateScanner::FUNCTION_NAME) {
            ParseFunctionCall(&scanner, names_to_values_map, scopes, scanner.getLastFunction(), output,
                              /* emit_output = */ Skipping(scopes));
            ProcessEndOfSyntax("function call", &scanner);
        }
    }

    const Scope &scope(scopes.back());
    switch (scope.getType()) {
    case Scope::TOP_LEVEL:
        return;
    case Scope::IF:
        throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                 + ": IF started on line " + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    case Scope::LOOP:
        throw std::runtime_error("in Template::ExpandTemplate: error on line " + std::to_string(scanner.getLineNo())
                                 + ": LOOP started on line " + std::to_string(scope.getStartLineNumber()) + " was never closed!");
    }
}


std::string ExpandTemplate(const std::string &template_string, const Map &names_to_values_map, const std::vector<Function *> &functions) {
    std::istringstream input(template_string);
    std::ostringstream expanded_template;
    ExpandTemplate(input, expanded_template, names_to_values_map, functions);
    return expanded_template.str();
}


} // namespace Template
