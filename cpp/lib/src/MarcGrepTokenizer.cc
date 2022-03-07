#include "MarcGrepTokenizer.h"
#include <stdexcept>
#include <cctype>
#include "StringUtil.h"


std::string Tokenizer::TokenTypeToString(const TokenType token) {
    switch (token) {
    case END_OF_INPUT:
        return "END_OF_INPUT";
    case HYPHEN:
        return "HYPHEN";
    case OPEN_BRACKET:
        return "OPEN_BRACKET";
    case CLOSE_BRACKET:
        return "CLOSE_BRACKET";
    case EQUAL:
        return "EQUAL";
    case COLON:
        return "COLON";
    case COMMA:
        return "COMMA";
    case EQUAL_EQUAL:
        return "EQUAL_EQUAL";
    case NOT_EQUAL:
        return "NOT_EQUAL";
    case SINGLE_FIELD_EQUAL:
        return "SINGLE_FIELD_EQUAL";
    case SINGLE_FIELD_NOT_EQUAL:
        return "SINGLE_FIELD_NOT_EQUAL";
    case STRING_CONSTANT:
        return "STRING_CONSTANT";
    case UNSIGNED_CONSTANT:
        return "UNSIGNED_CONSTANT";
    case INVALID_INPUT:
        return "INVALID_INPUT";
    case LEADER_KW:
        return "LEADER_KW";
    case IF_KW:
        return "IF_KW";
    case EXTRACT_KW:
        return "EXTRACT_KW";
    case EXISTS_KW:
        return "EXISTS_KW";
    case IS_MISSING_KW:
        return "IS_MISSING_KW";
    case STAR:
        return "STAR";
    }

    return "INVALID_INPUT"; // We should never get here!
}


std::string Tokenizer::EscapeString(const std::string &s) {
    std::string escaped_s;
    escaped_s.reserve(s.size());

    for (const auto &ch : s) {
        if (ch == '\\' or ch == '"')
            escaped_s += '\\';
        escaped_s += ch;
    }

    return escaped_s;
}


TokenType Tokenizer::getToken() {
    if (token_has_been_pushed_back_) {
        token_has_been_pushed_back_ = false;
        return pushed_back_token_;
    }

    // Skip over whitespace:
    while (ch_ != end_ and std::isblank(*ch_))
        ++ch_;

    if (ch_ == end_)
        return last_token_ = END_OF_INPUT;

    switch (*ch_) {
    case '-':
        ++ch_;
        return last_token_ = HYPHEN;
    case '[':
        ++ch_;
        return last_token_ = OPEN_BRACKET;
    case ']':
        ++ch_;
        return last_token_ = CLOSE_BRACKET;
    case '*':
        ++ch_;
        return last_token_ = STAR;
    case '=':
        ++ch_;
        if (ch_ == end_ or *ch_ != '=')
            return last_token_ = EQUAL;
        ++ch_;
        if (ch_ == end_ or *ch_ != '=')
            return last_token_ = EQUAL_EQUAL;
        ++ch_;
        return last_token_ = SINGLE_FIELD_EQUAL;
    case '!':
        ++ch_;
        if (ch_ == end_ or *ch_ != '=')
            return last_token_ = INVALID_INPUT;
        ++ch_;
        if (ch_ == end_ or *ch_ != '=')
            return last_token_ = NOT_EQUAL;
        ++ch_;
        return last_token_ = SINGLE_FIELD_NOT_EQUAL;
    case ':':
        ++ch_;
        return last_token_ = COLON;
    case ',':
        ++ch_;
        return last_token_ = COMMA;
    }

    // Parse unsigned constant:
    std::string unsigned_constant_candidate;
    while (ch_ != end_ and isdigit(*ch_))
        unsigned_constant_candidate += *ch_++;
    if (not unsigned_constant_candidate.empty()) {
        return StringUtil::ToUnsigned(unsigned_constant_candidate, &last_unsigned_constant_) ? UNSIGNED_CONSTANT : INVALID_INPUT;
    }

    // Parse keywords:
    std::string keyword_candidate;
    while (ch_ != end_ and (StringUtil::IsAsciiLetter(*ch_) or *ch_ == '_'))
        keyword_candidate += *ch_++;
    if (not keyword_candidate.empty()) {
        if (keyword_candidate == "leader")
            return last_token_ = LEADER_KW;
        if (keyword_candidate == "if")
            return last_token_ = IF_KW;
        if (keyword_candidate == "extract")
            return last_token_ = EXTRACT_KW;
        if (keyword_candidate == "exists")
            return last_token_ = EXISTS_KW;
        if (keyword_candidate == "is_missing")
            return last_token_ = IS_MISSING_KW;
        return INVALID_INPUT;
    }

    if (*ch_ != '"')
        return INVALID_INPUT;

    // Parse quoted string constant:
    last_string_constant_.clear();
    for (++ch_; ch_ != end_ and *ch_ != '"'; ++ch_) {
        if (*ch_ == '\\') {
            ++ch_;
            if (ch_ == end_)
                return last_token_ = INVALID_INPUT;
            else if (*ch_ == '\\' or *ch_ == '"')
                last_string_constant_ += *ch_;
            else
                return INVALID_INPUT;
        } else
            last_string_constant_ += *ch_;
    }

    if (ch_ == end_)
        return INVALID_INPUT;
    ++ch_;

    return last_token_ = STRING_CONSTANT;
}


void Tokenizer::ungetToken() {
    if (token_has_been_pushed_back_)
        throw std::runtime_error("In Tokenizer::ungetToken: can't push back twice in a row!");

    pushed_back_token_ = last_token_;
    token_has_been_pushed_back_ = true;
}
