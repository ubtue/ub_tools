/** \file   RegexMatcher.cc
 *  \brief  Implementation of the query parser for the marc_grep2 tool.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MarcQueryParser.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcGrepTokenizer.h"
#include "StringUtil.h"
#include "util.h"


std::string LeaderCondition::toString() const {
    return "LeaderCondition: start_offset: " + std::to_string(start_offset_) + ", end_offset: "
        + std::to_string(end_offset_) + ", match: \"" + Tokenizer::EscapeString(match_) + '"';
}


std::string FieldOrSubfieldDescriptor::getSubfieldCodes() const {
    if (isStar())
        throw std::runtime_error("FieldOrSubfieldDescriptor::getSubfieldCodes() called for \"*\" descriptor!");
    return field_or_subfield_.substr(3);
}


std::string ConditionDescriptor::toString() const {
    std::string as_string("(");

    switch (comp_type_) {
    case NO_COMPARISION:
        as_string += "NO_COMPARISION";
        break;
    case EQUAL_EQUAL:
        as_string += "EQUAL_EQUAL";
        break;
    case NOT_EQUAL:
        as_string += "NOT_EQUAL";
        break;
    case SINGLE_FIELD_EQUAL:
        as_string += "SINGLE_FIELD_EQUAL";
        break;
    case SINGLE_FIELD_NOT_EQUAL:
        as_string += "SINGLE_FIELD_NOT_EQUAL";
        break;
    case EXISTS:
        as_string += "EXISTS";
        break;
    case IS_MISSING:
        as_string += "IS_MISSING";
        break;
    }

    if (not field_or_subfield_reference_.empty())
        as_string += ", \"" + Tokenizer::EscapeString(field_or_subfield_reference_) + "\"";
    if (data_matcher_ != nullptr)
        as_string += ", \"" + Tokenizer::EscapeString(data_matcher_->getPattern()) + "\"";
    as_string += ")";

    return as_string;
}


ConditionDescriptor::ConditionDescriptor(const std::string &field_or_subfield_reference, const CompType comp_type)
    : comp_type_(comp_type), field_or_subfield_reference_(field_or_subfield_reference)
{
    if (comp_type != EXISTS and comp_type != IS_MISSING)
        logger->error("Invalid CompType in ConditionDescriptor constructor! (1)");
}


ConditionDescriptor::ConditionDescriptor(const std::string &field_or_subfield_reference, const CompType comp_type,
                                         const RegexMatcher * const data_matcher)
    : comp_type_(comp_type), field_or_subfield_reference_(field_or_subfield_reference),
      data_matcher_(data_matcher)
{
    if (comp_type != EQUAL_EQUAL and comp_type != NOT_EQUAL and comp_type != SINGLE_FIELD_EQUAL
        and comp_type != SINGLE_FIELD_NOT_EQUAL)
        logger->error("Invalid CompType in ConditionDescriptor constructor! (2)");
}


std::string QueryDescriptor::toString() const {
    std::string as_string;

    if (hasLeaderCondition())
        as_string += leader_cond_->toString() + "\n";

    for (const auto &cond_and_field_or_subfield_desc : conds_and_field_or_subfield_descs_)
        as_string += cond_and_field_or_subfield_desc.first.toString() + ", "
                     + cond_and_field_or_subfield_desc.second.toString() + "\n";

    return as_string;
}


// Parses the following part of the syntax of a query:
//   "leader[" offset_range "]=" value
void ParseLeaderCondition(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    TokenType token(tokenizer->getToken());
    if (token != LEADER_KW)
        throw std::runtime_error("Expected \"leader\" at beginning of a leader condition!");

    token = tokenizer->getToken();
    if (token != OPEN_BRACKET)
        throw std::runtime_error("Expected an opening bracket after the \"leader\" keyword!");

    token = tokenizer->getToken();
    if (token != UNSIGNED_CONSTANT)
        throw std::runtime_error("Expected a numeric offset after \"leader[\"! (1)");
    const unsigned start_offset(tokenizer->getLastUnsignedConstant());
    if (start_offset >= Leader::LEADER_LENGTH)
        throw std::runtime_error("Leader start offset >= leader length (" + std::to_string(Leader::LEADER_LENGTH)
                                 + ")!");

    unsigned end_offset(start_offset);

    token = tokenizer->getToken();
    if (token == HYPHEN) {
        token = tokenizer->getToken();
        if (token != UNSIGNED_CONSTANT)
            throw std::runtime_error("Expected a 2nd numeric offset as part of the leader offset range!");
        end_offset = tokenizer->getLastUnsignedConstant();
        if (end_offset < start_offset)
            throw std::runtime_error("2nd numeric offset of the leader offset range must be >= first offset!");
        if (end_offset >= Leader::LEADER_LENGTH)
            throw std::runtime_error("Leader end offset >= leader length (" + std::to_string(Leader::LEADER_LENGTH)
                                     + ")!");
        token = tokenizer->getToken();
    }

    if (token != CLOSE_BRACKET)
        throw std::runtime_error("Expected a closing bracket after a leader offset range!");

    token = tokenizer->getToken();
    if (token != EQUAL)
        throw std::runtime_error("Expected an equal sign after the closing bracket of a leader offset range!");

    token = tokenizer->getToken();
    if (token != STRING_CONSTANT)
        throw std::runtime_error("Expected a string constant as the last part of a leader condition!");
    const std::string string_const(tokenizer->getLastStringConstant());
    if (string_const.length() != end_offset - start_offset + 1)
        throw std::runtime_error("Final string constant of leader condition does not match offset range in length!");

    query_desc->setLeaderCondition(new LeaderCondition(start_offset, end_offset, string_const));
}


void ParseSimpleFieldList(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    std::vector<std::string> field_or_subfield_candidates;
    StringUtil::Split(tokenizer->getLastStringConstant(), ':', &field_or_subfield_candidates,
                      /* suppress_empty_components = */ false);

    for (const auto &field_or_subfield_candidate : field_or_subfield_candidates) {
        if (field_or_subfield_candidate.length() < DirectoryEntry::TAG_LENGTH)
            throw std::runtime_error("\"" + field_or_subfield_candidate
                                     +"\" is not a valid field or subfield reference!");
        query_desc->addFieldOrSubfieldDescriptor(FieldOrSubfieldDescriptor(field_or_subfield_candidate));
    }
}


void ParseFieldOrSubfieldReference(Tokenizer * const tokenizer, std::string * const field_or_subfield_reference) {
    const TokenType token(tokenizer->getToken());
    if (token != STRING_CONSTANT)
        throw std::runtime_error("Expected a field or subfield reference but found \""
                                 + Tokenizer::TokenTypeToString(token) + "\" instead!");
    const std::string string_const(tokenizer->getLastStringConstant());
    if (string_const.length() < DirectoryEntry::TAG_LENGTH)
        throw std::runtime_error("\"" + Tokenizer::EscapeString(string_const)
                                 + "\" is not a valid field or subfield reference!");
    *field_or_subfield_reference = string_const;
}


ConditionDescriptor::CompType TokenToConditionDescriptorCompType(const TokenType token) {
    switch (token) {
    case EQUAL_EQUAL:
        return ConditionDescriptor::EQUAL_EQUAL;
    case NOT_EQUAL:
        return ConditionDescriptor::NOT_EQUAL;
    case SINGLE_FIELD_EQUAL:
        return ConditionDescriptor::SINGLE_FIELD_EQUAL;
    case SINGLE_FIELD_NOT_EQUAL:
        return ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL;
    default:
        logger->error("in TokenToConditionDescriptorCompType: can't convert \"" + Tokenizer::TokenTypeToString(token)
                      + "\" to a ConditionDescriptor::CompType!");
    }
}


// Parses the "condition" part of a conditional field or subfield reference:
//  field_or_subfield_reference comp_op string_constant | field_or_subfield_reference "exists"
//  | field_or_subfield_reference "is_missing"
ConditionDescriptor ParseCondition(Tokenizer * const tokenizer) {
    std::string field_or_subfield_reference;
    ParseFieldOrSubfieldReference(tokenizer, &field_or_subfield_reference);
    if (field_or_subfield_reference.length() > DirectoryEntry::TAG_LENGTH + 1)
        throw std::runtime_error("Can't use \"" + field_or_subfield_reference + "\" in a comparison because of "
                                 "multiple subfield codes!");

    TokenType token(tokenizer->getToken());
    if ((token == SINGLE_FIELD_EQUAL or token == SINGLE_FIELD_NOT_EQUAL) 
        and field_or_subfield_reference.length() == DirectoryEntry::TAG_LENGTH)
        throw std::runtime_error("Field reference \"" + field_or_subfield_reference + "\"before \""
                                 + std::string(token == SINGLE_FIELD_EQUAL ? "===" : "!==")
                                 + " but a subfield reference is required!");

    if (token == EQUAL_EQUAL or token == NOT_EQUAL or token == SINGLE_FIELD_EQUAL
        or token == SINGLE_FIELD_NOT_EQUAL)
    {
        const ConditionDescriptor::CompType comp_type(TokenToConditionDescriptorCompType(token));
        token = tokenizer->getToken();
        if (token != STRING_CONSTANT)
            throw std::runtime_error("Expected regex string constant after \"==\" or \"!=\"!");
        const std::string string_const(tokenizer->getLastStringConstant());
        std::string err_msg;
        const RegexMatcher * const regex_matcher(RegexMatcher::RegexMatcherFactory(string_const, &err_msg));
        if (not err_msg.empty())
            throw std::runtime_error("Bad regex in condition: \"" + Tokenizer::EscapeString(string_const) + "\"!");
        return ConditionDescriptor(field_or_subfield_reference, comp_type, regex_matcher);
    } else if (token == EXISTS_KW)
        return ConditionDescriptor(field_or_subfield_reference, ConditionDescriptor::EXISTS);
    else if (token == IS_MISSING_KW)
        return ConditionDescriptor(field_or_subfield_reference, ConditionDescriptor::IS_MISSING);
    else
        throw std::runtime_error("Bad or missing condition in a conditional field or subfield reference!"
                                 "Found " + Tokenizer::TokenTypeToString(token) + "\" instead!");
}


// Parses a conditional field or subfield reference:
//   "if" condition "extract" field_or_subfield_reference
void ParseConditionalFieldOrSubfieldReference(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    //
    // Parse condition.
    //

    TokenType token(tokenizer->getToken());
    if (token != IF_KW)
        throw std::runtime_error("Expected \"if\" at start of a conditional field or subfield reference, "
                                 "found " + Tokenizer::TokenTypeToString(token) + " instead!");

    const ConditionDescriptor condition_desc(ParseCondition(tokenizer));

    token = tokenizer->getToken();
    if (token != EXTRACT_KW)
        throw std::runtime_error("Expected \"extract\" after the condition of a conditional field or subfield "
                                 "reference!");

    //
    // Parse field or subfield reference:
    //

    token = tokenizer->getToken();
    if (token == STAR) {
        query_desc->addConditionalFieldOrSubfieldDescriptor(
            condition_desc, FieldOrSubfieldDescriptor("*"));
    } else {
        if (token != STRING_CONSTANT)
            throw std::runtime_error("Expected field or subfield reference after \"extract\"!");
        tokenizer->ungetToken();
        std::string field_or_subfield_candidate;
        ParseFieldOrSubfieldReference(tokenizer, &field_or_subfield_candidate);

        if (condition_desc.getCompType() == ConditionDescriptor::SINGLE_FIELD_EQUAL
            or condition_desc.getCompType() == ConditionDescriptor::SINGLE_FIELD_NOT_EQUAL)
        {
            if (field_or_subfield_candidate.length() == DirectoryEntry::TAG_LENGTH)
                throw std::runtime_error("Expected subfield reference but found field reference \""
                                         + field_or_subfield_candidate + "\" instead!");
            const std::string condition_tag(
                condition_desc.getFieldOrSubfieldReference().substr(0, DirectoryEntry::TAG_LENGTH));
            const std::string extract_tag(field_or_subfield_candidate.substr(0, DirectoryEntry::TAG_LENGTH));
            if (condition_tag != extract_tag)
                throw std::runtime_error("Extracted tag \"" + extract_tag + "\" not equal to condition tag \""
                                         + condition_tag);
        }

        query_desc->addConditionalFieldOrSubfieldDescriptor(
            condition_desc, FieldOrSubfieldDescriptor(field_or_subfield_candidate));
    }
}


// Parses the following part of the syntax of conditional field or subfield references:
//   conditional_field_or_subfield_reference { "," conditional_field_or_subfield_reference }
void ParseConditionalFieldOrSubfieldReferences(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    ParseConditionalFieldOrSubfieldReference(tokenizer, query_desc);
    while ((tokenizer->getToken()) == COMMA)
        ParseConditionalFieldOrSubfieldReference(tokenizer, query_desc);
    tokenizer->ungetToken();
}


// Parses the following part of the syntax of a simple query:
//   simple_field_list | conditional_field_or_subfield_references
void ParseSimpleQuery(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    TokenType token(tokenizer->getToken());
    if (token == STRING_CONSTANT)
        ParseSimpleFieldList(tokenizer, query_desc);
    else {
        tokenizer->ungetToken();
        ParseConditionalFieldOrSubfieldReferences(tokenizer, query_desc);
    }
}


// Parses the following part of the syntax of a query:
//   [ leader_condition ] simple_query
void ParseQuery(Tokenizer * const tokenizer, QueryDescriptor * const query_desc) {
    TokenType token(tokenizer->getToken());

    tokenizer->ungetToken();
    if (token == LEADER_KW)
        ParseLeaderCondition(tokenizer, query_desc);

    ParseSimpleQuery(tokenizer, query_desc);
}


bool ParseQuery(const std::string &input, QueryDescriptor * const query_desc, std::string * const err_msg) {
    Tokenizer tokenizer(input);

    try {
        ParseQuery(&tokenizer, query_desc);
        return true;
    } catch (const std::exception &e) {
        *err_msg = e.what();
        return false;
    }
}
