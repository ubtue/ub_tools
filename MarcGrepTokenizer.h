#ifndef MARC_GREP_TOKENIZER_H
#define MARC_GREP_TOKENIZER_H


#include <string>


enum TokenType {
    END_OF_INPUT, HYPHEN, OPEN_BRACKET, CLOSE_BRACKET, EQUAL, COLON, COMMA, EQUAL_EQUAL, NOT_EQUAL,
    SINGLE_FIELD_EQUAL, SINGLE_FIELD_NOT_EQUAL, STRING_CONSTANT, STAR,
    UNSIGNED_CONSTANT, INVALID_INPUT, LEADER_KW, IF_KW, EXTRACT_KW, EXISTS_KW, IS_MISSING_KW
};


class Tokenizer {
    const std::string input_;
    std::string::const_iterator ch_;
    const std::string::const_iterator end_;
    TokenType pushed_back_token_;
    bool token_has_been_pushed_back_;
    TokenType last_token_;
    std::string last_string_constant_;
    unsigned last_unsigned_constant_;
public:
    explicit Tokenizer(const std::string input)
	: input_(input), ch_(input_.begin()), end_(input_.end()), token_has_been_pushed_back_(false) { }
    TokenType getToken();
    void ungetToken();
    std::string getLastStringConstant() const { return last_string_constant_; }
    unsigned getLastUnsignedConstant() const { return last_unsigned_constant_; }
    static std::string EscapeString(const std::string &s);
    static std::string TokenTypeToString(const TokenType token);
};


#endif // ifndef MARC_GREP_TOKENIZER_H
