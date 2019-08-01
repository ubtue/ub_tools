/** \brief MARC grep the Next Generation.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Compiler.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("--query=query [--output-format=output_format] marc_file1 [marc_file2 .. marc_fileN]\n"
            "Queries have the following syntax:\n"
            "expression → term {OR term}\n"
            "term       → factor {AND factor}\n"
            "factor     → field_or_subfield_reference (== | !=) string_constant_or_regex\n"
            "factor     → NOT factor\n"
            "factor     → '(' expression ')'\n"
            );
    std::exit(EXIT_FAILURE);
}


enum TokenType { AND, OR, NOT, STRING_CONST, FUNC_CALL, OPEN_PAREN, CLOSE_PAREN, REGEX, EQUALS, NOT_EQUALS,
                 COMMA, ERROR, END_OF_QUERY };


class Tokenizer {
    struct FunctionDesc {
        std::string name_;
        unsigned argument_count_;
    };

    std::string::const_iterator next_ch_;
    const std::string::const_iterator end_;
    bool pushed_back_;
    TokenType last_token_;
    std::string last_error_message_;
    std::string last_string_;
    std::string last_function_name_;
    const static std::vector<FunctionDesc> function_descriptions_;
public:
    explicit Tokenizer(const std:: string &query): next_ch_(query.cbegin()), end_(query.cend()), pushed_back_(false) { }
    TokenType getNextToken();
    void ungetLastToken();
    inline const std::string &getLastErrorMessage() const { return last_error_message_; }
    inline const std::string &getLastString() const { return last_string_; }
    inline const std::string &getLastFunctionName() const { return last_function_name_; }
    static std::string TokenTypeToString(const TokenType token);
private:
    TokenType parseStringConstantOrRegex();
    bool isKnownFunctionName(const std::string &name_candidate) const;
};


const std::vector<Tokenizer::FunctionDesc> Tokenizer::function_descriptions_{ { "HasField", 1 }, { "HasSubfield", 2} };


TokenType Tokenizer::getNextToken() {
    if (pushed_back_) {
        pushed_back_ = false;
        return last_token_;
    }

    // Skip over spaces:
    while (next_ch_ != end_ and *next_ch_ == ' ')
        ++next_ch_;

    if (next_ch_ == end_)
        return last_token_ = END_OF_QUERY;

    if (*next_ch_ == '"' or *next_ch_ == '/')
        return last_token_ = parseStringConstantOrRegex();

    if (*next_ch_ == ',') {
        ++next_ch_;
        return last_token_ = COMMA;
    }

    if (*next_ch_ == '(') {
        ++next_ch_;
        return last_token_ = OPEN_PAREN;
    }

    if (*next_ch_ == ')') {
        ++next_ch_;
        return last_token_ = CLOSE_PAREN;
    }

    if (*next_ch_ == '=') {
        ++next_ch_;
        if (next_ch_ == end_ or *next_ch_ != '=') {
            last_error_message_ = "unexpected single equal sign found!";
            return ERROR;
        }
        return last_token_ = EQUALS;
    }

    if (*next_ch_ == '!') {
        ++next_ch_;
        if (next_ch_ == end_ or *next_ch_ != '=') {
            last_error_message_ = "unexpected single exclamation point found!";
            return ERROR;
        }
        return last_token_ = NOT_EQUALS;
    }

    if (unlikely(not StringUtil::IsAsciiLetter(*next_ch_))) {
        last_error_message_ = "expected ASCII letter!";
        return ERROR;
    }

    std::string id;
    while (next_ch_ != end_ and StringUtil::IsAsciiLetter(*next_ch_))
        id += *next_ch_;

    if (id == "AND")
        return last_token_ = AND;
    if (id == "OR")
        return last_token_ = OR;
    if (id == "NOT")
        return last_token_ = NOT;

    if (isKnownFunctionName(id)) {
        last_function_name_ = id;
        return last_token_ = FUNC_CALL;
    }

    last_error_message_ = "unknown function name \"" + id + "\"!";

    return ERROR;
}


void Tokenizer::ungetLastToken() {
    if (unlikely(pushed_back_))
        LOG_ERROR("can't push back two tokens in a row!");
    pushed_back_ = true;
}


TokenType Tokenizer::parseStringConstantOrRegex() {
    const char terminator(*next_ch_++);

    last_string_.clear();
    bool escaped(false);
    for (/* Intentionally empty! */; next_ch_ != end_; ++next_ch_) {
        if (escaped) {
            escaped = false;
            last_string_ += *next_ch_;
        } else if (*next_ch_ == terminator) {
            ++next_ch_;
            return (terminator == '"') ? STRING_CONST : REGEX;
        } else if (*next_ch_ == '\\')
            escaped = true;
        else
            last_string_ += *next_ch_;
    }

    last_error_message_ = "unterminated string constant or regex!";
    return ERROR;
}


bool Tokenizer::isKnownFunctionName(const std::string &name_candidate) const {
    for (const auto &function_description : function_descriptions_) {
        if (function_description.name_ == name_candidate)
            return true;
    }

    return false;
}


std::string Tokenizer::TokenTypeToString(const TokenType token) {
    switch (token) {
    case AND:
        return "AND";
    case OR:
        return "OR";
    case NOT:
        return "NOT";
    case STRING_CONST:
        return "string constant";
    case FUNC_CALL:
        return "function call";
    case OPEN_PAREN:
        return "(";
    case CLOSE_PAREN:
        return ")";
    case REGEX:
        return "regular expression";
    case EQUALS:
        return "==";
    case NOT_EQUALS:
        return "!=";
    case COMMA:
        return ",";
    case ERROR:
        return "unexpected input";
    case END_OF_QUERY:
        return "end-of-query";
    }
}


class Query {
    enum NodeType { AND_NODE, OR_NODE, STRING_COMPARISON_NODE, REGEX_COMPARISON_NODE };
    class Node {
    protected:
        Node() {}
    public:
        virtual ~Node() = 0;
        virtual NodeType getNodeType() const = 0;
        virtual bool eval(const MARC::Record &record) const = 0;
    };

    class AndNode final : public Node {
        std::vector<Node *> children_;
    public:
        AndNode() { }
        ~AndNode() {
            for (const auto child_node : children_)
                delete child_node;
        }
        virtual NodeType getNodeType() const override { return AND_NODE; }
        virtual bool eval(const MARC::Record &record) const override;
    };

    class OrNode final : public Node {
        std::vector<Node *> children_;
    public:
        OrNode() { }
        ~OrNode() {
            for (const auto child_node : children_)
                delete child_node;
        }
        virtual NodeType getNodeType() const override { return OR_NODE; }
        virtual bool eval(const MARC::Record &record) const override;
    };

    class StringComparisonNode final : public Node {
        MARC::Tag field_tag_;
        char subfield_code_;
        std::string string_const_;
        bool invert_;
    public:
        StringComparisonNode(const std::string &field_or_subfield_reference, const std::string &string_const, const bool invert)
            : field_tag_(field_or_subfield_reference.substr(0, MARC::Record::TAG_LENGTH)),
              subfield_code_(field_or_subfield_reference.length() > MARC::Record::TAG_LENGTH
                             ? field_or_subfield_reference[MARC::Record::TAG_LENGTH] : '\0'),
              string_const_(string_const), invert_(invert) { }
        ~StringComparisonNode() = default;
        virtual NodeType getNodeType() const override { return STRING_COMPARISON_NODE; }
        virtual bool eval(const MARC::Record &record) const override;
    };

    class RegexComparisonNode final : public Node {
        MARC::Tag field_tag_;
        char subfield_code_;
        RegexMatcher *regex_;
        bool invert_;
    public:
        RegexComparisonNode(const std::string &field_or_subfield_reference, RegexMatcher * const regex, const bool invert)
            : field_tag_(field_or_subfield_reference.substr(0, MARC::Record::TAG_LENGTH)),
              subfield_code_(field_or_subfield_reference.length() > MARC::Record::TAG_LENGTH
                             ? field_or_subfield_reference[MARC::Record::TAG_LENGTH] : '\0'),
              regex_(regex), invert_(invert) { }
        ~RegexComparisonNode() { delete regex_; }
        virtual NodeType getNodeType() const override { return STRING_COMPARISON_NODE; }
        virtual bool eval(const MARC::Record &record) const override;
    };

    Tokenizer tokenizer_;
public:
    explicit Query(const std:: string &query);
    bool matched(const MARC::Record &record) const;
private:
    void ParseExpression();
    void ParseTerm();
    void ParseFactor();
};


Query::Node::~Node() {
    /* Intentionally empty! */
}


bool Query::AndNode::eval(const MARC::Record &record) const {
    for (const auto child_node : children_) {
        if (not child_node->eval(record))
            return false;
    }

    return true;
}


bool Query::OrNode::eval(const MARC::Record &record) const {
    for (const auto child_node : children_) {
        if (child_node->eval(record))
            return true;
    }

    return false;
}


bool Query::StringComparisonNode::eval(const MARC::Record &record) const {
    for (const auto &field : record.getTagRange(field_tag_)) {
        if (subfield_code_ == '\0') {
            if (field .getContents() == string_const_) {
                if (not invert_)
                    return true;
            } else if (invert_)
                return true;
        } else {
            const MARC::Subfields subfields(field.getSubfields());
            for (const auto &value_and_code : subfields) {
                if (value_and_code.code_ == subfield_code_) {
                    if (value_and_code.value_ == string_const_) {
                        if (not invert_)
                            return true;
                    } else if (invert_)
                        return true;
                }
            }
        }
    }

    return false;
}


bool Query::RegexComparisonNode::eval(const MARC::Record &record) const {
    for (const auto &field : record.getTagRange(field_tag_)) {
        if (subfield_code_ == '\0') {
            if (regex_->matched(field .getContents())) {
                if (not invert_)
                    return true;
            } else if (invert_)
                return true;
        } else {
            const MARC::Subfields subfields(field.getSubfields());
            for (const auto &value_and_code : subfields) {
                if (value_and_code.code_ == subfield_code_) {
                    if (regex_->matched(value_and_code.value_)) {
                        if (not invert_)
                            return true;
                    } else if (invert_)
                        return true;
                }
            }
        }
    }

    return false;
}


Query::Query(const std:: string &query): tokenizer_(query) {
    ParseExpression();
}


bool Query::matched(const MARC::Record &/*record*/) const {
    return false;
}


void Query::ParseExpression() {
    ParseTerm();

    TokenType token(tokenizer_.getNextToken());
    while (token == OR) {
        ParseTerm();
        token = tokenizer_.getNextToken();
    }

    tokenizer_.ungetLastToken();
}


void Query::ParseTerm() {
    ParseFactor();

    TokenType token(tokenizer_.getNextToken());
    while (token == AND) {
        ParseFactor();
        token = tokenizer_.getNextToken();
    }

    tokenizer_.ungetLastToken();
}


void Query::ParseFactor() {
    TokenType token(tokenizer_.getNextToken());
    if (token == STRING_CONST) {
        if (tokenizer_.getLastString().length() != MARC::Record::TAG_LENGTH
            and tokenizer_.getLastString().length() != MARC::Record::TAG_LENGTH + 1)
            throw std::runtime_error("invalid field or subfield reference \"" + tokenizer_.getLastString() + "\"!");

        token = tokenizer_.getNextToken();
        if (token != EQUALS and token != NOT_EQUALS)
            throw std::runtime_error("expected == or != after field or subfield reference, found "
                                     + Tokenizer::TokenTypeToString(token) + " instead!");

        const TokenType last_token(token);
        token = tokenizer_.getNextToken();
        if (token != STRING_CONST and token != REGEX)
            throw std::runtime_error("expected a string constant or a regex after " + Tokenizer::TokenTypeToString(last_token) + ", found "
                                     + Tokenizer::TokenTypeToString(token) + " instead!");

        return;
    }

    if (token == NOT) {
        ParseFactor();
        return;
    }

    if (token != OPEN_PAREN)
        throw std::runtime_error("opening parenthesis, NOT or string constant expected found " + Tokenizer::TokenTypeToString(token)
                                 + "instead!");

    ParseExpression();

    token = tokenizer_.getNextToken();
    if (token != CLOSE_PAREN)
        throw std::runtime_error("closing parenthesis after expression expected, found " + Tokenizer::TokenTypeToString(token) + "instead!");
}


void ProcessRecords(const Query &query, MARC::Reader * const marc_reader) {
    unsigned record_count(0), matched_count(0);

    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not query.matched(record))
            continue;

        ++matched_count;
    }

    std::cerr << "Matched " << matched_count << " of " << record_count << " records.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    if (not StringUtil::StartsWith(argv[1], "--query="))
        LOG_ERROR("missing --query=...!");
    const std::string query_str(argv[1] + __builtin_strlen("--query="));
    Query query(query_str);

    if (argc < 3)
        Usage();

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        auto marc_reader(MARC::Reader::Factory(argv[1]));
        ProcessRecords(query, marc_reader.get());
    }

    return EXIT_SUCCESS;
}
