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
public:
    explicit FieldOrSubfieldDescriptor(const std::string &field_or_subfield)
        : field_or_subfield_(field_or_subfield) { }
    bool isStar() const { return field_or_subfield_ == "*"; }
    std::string getTag() const { return isStar() ? "*" : field_or_subfield_.substr(0, 3); }
    std::string getSubfieldCodes() const;
    std::string getRaw() const { return field_or_subfield_; }
    std::string toString() const { return "field_or_subfield: " + field_or_subfield_; }
};


class ConditionDescriptor {
public:
    enum CompType { NO_COMPARISION, EQUAL_EQUAL, NOT_EQUAL, SINGLE_FIELD_EQUAL, SINGLE_FIELD_NOT_EQUAL, EXISTS,
                    IS_MISSING };
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
    ConditionDescriptor(const std::string &field_or_subfield_reference, const CompType comp_type,
                        RegexMatcher * const data_matcher);

    CompType getCompType() const { return comp_type_; }
    const std::string &getFieldOrSubfieldReference() const { return field_or_subfield_reference_; }

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
                                                 const FieldOrSubfieldDescriptor &field_or_subfield_desc)
    {
        conds_and_field_or_subfield_descs_.push_back(std::make_pair(cond_desc, field_or_subfield_desc));
    }

    const std::vector<std::pair<ConditionDescriptor, FieldOrSubfieldDescriptor>> &getCondsAndFieldOrSubfieldDescs()
        const { return conds_and_field_or_subfield_descs_; }

    std::string toString() const;
};


bool ParseQuery(const std::string &input, QueryDescriptor * const query_desc, std::string * const err_msg);
