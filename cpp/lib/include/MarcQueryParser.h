/** \file   MarcQueryParser.h
 *  \brief  Interface of the query parser for the marc_grep2 tool.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <memory>
#include <string>
#include <vector>
#include "RegexMatcher.h"


class LeaderCondition {
    unsigned start_offset_, end_offset_;
    std::string match_;

public:
    LeaderCondition(const unsigned start_offset, const unsigned end_offset, const std::string &match)
        : start_offset_(start_offset), end_offset_(end_offset), match_(match) { }
    unsigned getStartOffset() const { return start_offset_; }
    unsigned getEndOffset() const { return end_offset_; }
    const std::string &getMatch() const { return match_; }
    std::string toString() const;
};


class FieldOrSubfieldDescriptor {
    const std::string field_or_subfield_;
    const char indicator1_, indicator2_;

public:
    FieldOrSubfieldDescriptor(): indicator1_('\0'), indicator2_('\0') { }
    FieldOrSubfieldDescriptor(const FieldOrSubfieldDescriptor &other) = default;
    explicit FieldOrSubfieldDescriptor(const std::string &field_or_subfield, const char indicator1, const char indicator2)
        : field_or_subfield_(field_or_subfield), indicator1_(indicator1), indicator2_(indicator2) { }

    inline bool empty() const { return field_or_subfield_.empty(); }
    inline bool isStar() const { return field_or_subfield_ == "*"; }
    inline std::string getTag() const { return isStar() ? "*" : field_or_subfield_.substr(0, 3); }
    std::string getSubfieldCodes() const;
    inline std::string getRaw() const { return field_or_subfield_; }
    inline char getIndicator1() const { return indicator1_; }
    inline char getIndicator2() const { return indicator2_; }
    inline std::string toString() const { return "field_or_subfield: " + field_or_subfield_; }
};


class ConditionDescriptor {
public:
    enum CompType { NO_COMPARISION, EQUAL_EQUAL, NOT_EQUAL, SINGLE_FIELD_EQUAL, SINGLE_FIELD_NOT_EQUAL, EXISTS, IS_MISSING };

private:
    CompType comp_type_;
    const std::string field_or_subfield_reference_;
    const std::shared_ptr<RegexMatcher> data_matcher_;

public:
    explicit ConditionDescriptor(): comp_type_(NO_COMPARISION) { }

    // "comp_type" must be EXISTS or IS_MISSING.
    ConditionDescriptor(const std::string &field_or_subfield_reference, const CompType comp_type);

    // "comp_type" must be EQUAL_EQUAL, NOT_EQUAL, SINGLE_FIELD_EQUAL or SINGLE_FIELD_NOT_EQUAL.
    // Warning: "data_matcher" must be the address of a heap object!  ConditionDescriptor then takes the ownership.
    ConditionDescriptor(const std::string &field_or_subfield_reference, const CompType comp_type, RegexMatcher * const data_matcher);

    CompType getCompType() const { return comp_type_; }
    inline const std::string &getFieldOrSubfieldReference() const { return field_or_subfield_reference_; }

    // Warning: Only call this member function if the ConditionDescriptor is of a type that logically requires
    //          a regex to compare against.  If that is not the case you will get a nasty error!
    RegexMatcher &getDataMatcher() const { return *data_matcher_; }

    std::string toString() const;
};


class QueryDescriptor {
    std::shared_ptr<LeaderCondition> leader_cond_;
    std::vector<std::pair<ConditionDescriptor, FieldOrSubfieldDescriptor>> conds_and_field_or_subfield_descs_;

public:
    // Warning: You must pass a heap object into this function.  Do not pass the address of a LeaderCondition!
    void setLeaderCondition(LeaderCondition * const new_leader_cond_) { leader_cond_.reset(new_leader_cond_); }

    bool hasLeaderCondition() const { return leader_cond_ != nullptr; }
    const LeaderCondition &getLeaderCondition() const { return *leader_cond_; }

    void addFieldOrSubfieldDescriptor(const FieldOrSubfieldDescriptor &field_or_subfield_desc) {
        conds_and_field_or_subfield_descs_.push_back(std::make_pair(ConditionDescriptor(), field_or_subfield_desc));
    }

    void addConditionalFieldOrSubfieldDescriptor(const ConditionDescriptor cond_desc,
                                                 const FieldOrSubfieldDescriptor &field_or_subfield_desc) {
        conds_and_field_or_subfield_descs_.push_back(std::make_pair(cond_desc, field_or_subfield_desc));
    }

    const std::vector<std::pair<ConditionDescriptor, FieldOrSubfieldDescriptor>> &getCondsAndFieldOrSubfieldDescs() const {
        return conds_and_field_or_subfield_descs_;
    }

    std::string toString() const;
};


bool ParseQuery(const std::string &input, QueryDescriptor * const query_desc, std::string * const err_msg);
