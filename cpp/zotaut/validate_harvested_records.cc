/** \brief Utility for validating and fixing up records harvested by zts_harvester
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "DnsUtil.h"
#include "EmailSender.h"
#include "IniFile.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroHarvesterUtil.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--update-db-errors] marc_input marc_output online_first_file missed_expectations_file email_address");
}


enum FieldPresence { ALWAYS, SOMETIMES, IGNORE };


FieldPresence StringToFieldPresence(const std::string &field_presence_str) {
    if (::strcasecmp(field_presence_str.c_str(), "ALWAYS") == 0)
        return ALWAYS;
    if (::strcasecmp(field_presence_str.c_str(), "SOMETIMES") == 0)
        return SOMETIMES;
    if (::strcasecmp(field_presence_str.c_str(), "IGNORE") == 0)
        return IGNORE;

    LOG_ERROR("unknown field presence \"" + field_presence_str + "\"!");
}


// Note that we are aware of the fact that this class is leaking memory.
// In the context of this program that is not a problem as the constructed instances are long lived
// and can safely exist until the termination of the program.
class FieldPresenceAndRegex {
    FieldPresence field_presence_;
    RegexMatcher * regex_matcher_;
public:
    FieldPresenceAndRegex() = default;
    FieldPresenceAndRegex(const FieldPresenceAndRegex &other) = default;
    FieldPresenceAndRegex(const FieldPresence field_presence, RegexMatcher * const regex_matcher)
        : field_presence_(field_presence), regex_matcher_(regex_matcher) { }

    FieldPresenceAndRegex &operator=(const FieldPresenceAndRegex &rhs) = default;

    bool matched(const std::string &subfield_contents) const {
        if (regex_matcher_ == nullptr)
            return true;
        return regex_matcher_->matched(subfield_contents);
    }

    FieldPresence getFieldPresence() const { return field_presence_; }
    std::string getRegex() const { return regex_matcher_ == nullptr ? "" : regex_matcher_->getPattern(); }
};


class FieldRules {
    std::map<char, FieldPresenceAndRegex> subfield_code_to_field_presence_and_regex_map_;
public:
    FieldRules(const char subfield_code, const FieldPresence field_presence,
               RegexMatcher * const regex_matcher);
    void addRule(const char subfield_code, const FieldPresence field_presence,
                 RegexMatcher * const regex_matcher);
    void findRuleViolations(const MARC::Subfields &subfields, std::string * const reason_for_being_invalid) const;
    bool isMandatoryField() const;
};


FieldRules::FieldRules(const char subfield_code, const FieldPresence field_presence,
                       RegexMatcher * const regex_matcher)
{
    subfield_code_to_field_presence_and_regex_map_[subfield_code] =
        FieldPresenceAndRegex(field_presence, regex_matcher);
}


void FieldRules::addRule(const char subfield_code, const FieldPresence field_presence,
                         RegexMatcher * const regex_matcher)
{
    if (unlikely(subfield_code_to_field_presence_and_regex_map_.find(subfield_code) != subfield_code_to_field_presence_and_regex_map_.end()))
        LOG_ERROR("Attempt to insert a second rule for subfield code '" + std::string(1, subfield_code) + "'!");

    subfield_code_to_field_presence_and_regex_map_[subfield_code] =
        FieldPresenceAndRegex(field_presence, regex_matcher);
}


void FieldRules::findRuleViolations(const MARC::Subfields &subfields, std::string * const reason_for_being_invalid) const {
    std::set<char> found_subfield_codes;
    for (const auto &subfield : subfields) {
        found_subfield_codes.emplace(subfield.code_);
        const auto subfield_code_field_presence_and_regex(subfield_code_to_field_presence_and_regex_map_.find(subfield.code_));
        if (subfield_code_field_presence_and_regex == subfield_code_to_field_presence_and_regex_map_.cend())
            reason_for_being_invalid->append("found unexpected subfield $" + std::string(1, subfield.code_));
        else if (not subfield_code_field_presence_and_regex->second.matched(subfield.value_))
            reason_for_being_invalid->append("contents of subfield $" + std::string(1, subfield.code_) + "("
                                             + subfield.value_ + ") did not match regex \""
                                             + subfield_code_field_presence_and_regex->second.getRegex() + "\"");
    }

    for (const auto &subfield_code_and_field_presence : subfield_code_to_field_presence_and_regex_map_) {
        const auto iter(found_subfield_codes.find(subfield_code_and_field_presence.first));
        if (iter == found_subfield_codes.end() and subfield_code_and_field_presence.second.getFieldPresence() == ALWAYS) {
            if (not reason_for_being_invalid->empty())
                reason_for_being_invalid->append("; ");
            reason_for_being_invalid->append("required subfield " + std::string(1, subfield_code_and_field_presence.first)
                                             + " is missing");
        }
    }
}


bool FieldRules::isMandatoryField() const {
    for (const auto &subfield_code_and_field_presence : subfield_code_to_field_presence_and_regex_map_) {
        if (subfield_code_and_field_presence.second.getFieldPresence() == ALWAYS)
            return true;
    }
    return false;
}


class FieldValidator {
public:
    ~FieldValidator() {}

    /** \return True if we found rules for all subfields in "field" o/w false.
     *  \note   If a rule violation was found, "reason_for_being_invalid" w/ be non-empty after the call and
     *          we will return true.
     */
    virtual bool foundRuleMatch(const unsigned journal_id, const MARC::Record::Field &field,
                                std::string * const reason_for_being_invalid) const = 0;

    virtual void findMissingTags(const unsigned journal_id, const std::set<std::string> &present_tags,
                                 std::set<std::string> * const missing_tags, std::set<std::string> * const checked_tags) const = 0;
};


class GeneralFieldValidator final : public FieldValidator {
    std::unordered_map<std::string, FieldRules> tags_to_rules_map_;
public:
    void addRule(const std::string &tag, const char subfield_code, const FieldPresence field_presence,
                 RegexMatcher * const regex_matcher);
    virtual bool foundRuleMatch(const unsigned journal_id, const MARC::Record::Field &field,
                                std::string * const reason_for_being_invalid) const;
    virtual void findMissingTags(const unsigned journal_id, const std::set<std::string> &present_tags,
                                 std::set<std::string> * const missing_tags, std::set<std::string> * const checked_tags) const;
};


void GeneralFieldValidator::addRule(const std::string &tag, const char subfield_code,
                                    const FieldPresence field_presence, RegexMatcher * const regex_matcher)
{
    auto tag_and_rule(tags_to_rules_map_.find(tag));
    if (tag_and_rule == tags_to_rules_map_.end())
        tags_to_rules_map_.emplace(tag, FieldRules(subfield_code, field_presence, regex_matcher));
    else
        tag_and_rule->second.addRule(subfield_code, field_presence, regex_matcher);
}


bool GeneralFieldValidator::foundRuleMatch(const unsigned /*journal_id*/, const MARC::Record::Field &field,
                                           std::string * const reason_for_being_invalid) const
{
    const std::string tag(field.getTag().toString());
    const auto tags_and_rules(tags_to_rules_map_.find(tag));
    if (tags_and_rules == tags_to_rules_map_.cend())
        return false;

    std::string rule_violations;
    tags_and_rules->second.findRuleViolations(field.getSubfields(), &rule_violations);
    if (not rule_violations.empty())
        *reason_for_being_invalid = tag + ": " + rule_violations;

    return true;
}


void GeneralFieldValidator::findMissingTags(const unsigned /*journal_id*/, const std::set<std::string> &present_tags,
                                            std::set<std::string> * const missing_tags, std::set<std::string> * const checked_tags) const
{
    for (const auto &[required_tag, rule] : tags_to_rules_map_) {
        if (checked_tags->find(required_tag) != checked_tags->end())
            continue;

        if (rule.isMandatoryField() and present_tags.find(required_tag) == present_tags.cend())
            missing_tags->emplace(required_tag);

        checked_tags->emplace(required_tag);
    }
}


class JournalSpecificFieldValidator final : public FieldValidator {
    std::unordered_map<unsigned, GeneralFieldValidator> journal_ids_to_field_validators_map_;
public:
    void addRule(const unsigned journal_id, const std::string &tag, const char subfield_code,
                 const FieldPresence field_presence, RegexMatcher * const regex_matcher);
    virtual bool foundRuleMatch(const unsigned journal_id, const MARC::Record::Field &field,
                                std::string * const reason_for_being_invalid) const;
    virtual void findMissingTags(const unsigned journal_id, const std::set<std::string> &present_tags,
                                 std::set<std::string> * const missing_tags, std::set<std::string> * const checked_tags) const;
};


void JournalSpecificFieldValidator::addRule(const unsigned journal_id, const std::string &tag, const char subfield_code,
                                            const FieldPresence field_presence, RegexMatcher * const regex_matcher)
{
    auto journal_id_and_field_validators(journal_ids_to_field_validators_map_.find(journal_id));
    if (journal_id_and_field_validators == journal_ids_to_field_validators_map_.end()) {
        GeneralFieldValidator new_general_field_validator;
        new_general_field_validator.addRule(tag, subfield_code, field_presence, regex_matcher);
        journal_ids_to_field_validators_map_.emplace(journal_id, new_general_field_validator);
    } else
        journal_id_and_field_validators->second.addRule(tag, subfield_code, field_presence, regex_matcher);
}


bool JournalSpecificFieldValidator::foundRuleMatch(const unsigned journal_id, const MARC::Record::Field &field,
                                                   std::string * const reason_for_being_invalid) const
{
    const auto journal_id_and_field_validators(journal_ids_to_field_validators_map_.find(journal_id));
    if (journal_id_and_field_validators == journal_ids_to_field_validators_map_.cend())
        return false;
    return journal_id_and_field_validators->second.foundRuleMatch(journal_id, field, reason_for_being_invalid);
}


void JournalSpecificFieldValidator::findMissingTags(const unsigned journal_id,
                                                    const std::set<std::string> &present_tags,
                                                    std::set<std::string> * const missing_tags,
                                                    std::set<std::string> * const checked_tags) const
{
    const auto journal_id_and_field_validators(journal_ids_to_field_validators_map_.find(journal_id));
    if (journal_id_and_field_validators == journal_ids_to_field_validators_map_.cend())
        return;
    journal_id_and_field_validators->second.findMissingTags(journal_id, present_tags, missing_tags, checked_tags);
}


void LoadRules(DbConnection * const db_connection, GeneralFieldValidator * const general_regular_article_validator,
               JournalSpecificFieldValidator * const journal_specific_regular_article_validator,
               GeneralFieldValidator * const general_review_article_validator,
               JournalSpecificFieldValidator * const journal_specific_review_article_validator)
{
    db_connection->queryOrDie(
        "SELECT journal_id,marc_field_tag,marc_subfield_code,field_presence,record_type,regex FROM metadata_presence_tracer"
        " ORDER BY marc_field_tag,marc_subfield_code ASC");
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        std::string error_message;
        RegexMatcher *new_regex_matcher(row.isNull("regex") ? nullptr
                                        : RegexMatcher::RegexMatcherFactory(row["regex"], &error_message));
        if (unlikely(not error_message.empty()))
            LOG_ERROR("could not compile \"" + row["regex"] + "\" as a PCRE!");

        if (row["record_type"] == "regular_article") {
            if (row.isNull("journal_id"))
                general_regular_article_validator->addRule(row["marc_field_tag"], row["marc_subfield_code"][0],
                                                           StringToFieldPresence(row["field_presence"]),
                                                           new_regex_matcher);
            else
                journal_specific_regular_article_validator->addRule(StringUtil::ToUnsigned(row["journal_id"]), row["marc_field_tag"],
                                                                    row["marc_subfield_code"][0],
                                                                    StringToFieldPresence(row["field_presence"]),
                                                                    new_regex_matcher);
        } else { // Assume that record_type == review.
            if (row.isNull("journal_id"))
                general_review_article_validator->addRule(row["marc_field_tag"], row["marc_subfield_code"][0],
                                                          StringToFieldPresence(row["field_presence"]),
                                                          new_regex_matcher);
            else
                journal_specific_review_article_validator->addRule(StringUtil::ToUnsigned(row["journal_id"]),
                                                                   row["marc_field_tag"], row["marc_subfield_code"][0],
                                                                   StringToFieldPresence(row["field_presence"]),
                                                                   new_regex_matcher);
        }
    }
}


void SendEmail(const std::string &email_address, const std::string &message_subject, const std::string &message_body) {
    const auto reply_code(EmailSender::SimplerSendEmail("zts_harvester_delivery_pipeline@uni-tuebingen.de",
                                                        { email_address }, message_subject, message_body,
                                                        EmailSender::MEDIUM));
    if (reply_code >= 300)
        LOG_WARNING("failed to send email, the response code was: " + std::to_string(reply_code));
}


static const std::set<std::string> REQUIRED_EXISTING_FIELD_TAGS{ "001", "003", "007" };
static const std::set<std::string> REQUIRED_SPECIAL_CASE_FIELD_TAGS{ "245", "655" };


void CheckGenericRequirements(const MARC::Record &record, std::vector<std::string> * const reasons_for_being_invalid) {

    for (const auto &required_field_tag : REQUIRED_EXISTING_FIELD_TAGS) {
        if (not record.hasTag(required_field_tag))
            reasons_for_being_invalid->emplace_back("required field " + required_field_tag + " is missing");
    }

    const auto _245_field(record.findTag("245"));
    if (_245_field != record.end() and _245_field->getFirstSubfieldWithCode('a').empty())
        reasons_for_being_invalid->emplace_back("subfield 245$a is missing");

    // Check the structure of the 655 field wich is used to flag a record as a review:
    if (record.hasTag("655") and
        ::strcasecmp(record.getFirstSubfieldValue("655", 'a').c_str(), "Rezension") == 0 and
        record.getFirstField("655")->getContents() !=
            " 7""\x1F""aRezension""\x1F""0(DE-588)4049712-4""\x1F""0(DE-627)106186019""\x1F""2gnd-content")
    {
        reasons_for_being_invalid->emplace_back("655 field has unexpected contents");
        return;
    }
}


std::unordered_map<std::string, unsigned> GetZederIdAndInstanceToJournalIdMap(DbConnection * const db_connection) {
    std::unordered_map<std::string, unsigned> zeder_id_and_instance_to_zeder_journal_id_map;

    db_connection->queryOrDie("SELECT id, zeder_id, zeder_instance FROM zeder_journals");
    auto result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        zeder_id_and_instance_to_zeder_journal_id_map[row["zeder_id"] + "#" + row["zeder_instance"]] = StringUtil::ToUnsigned(row["id"]);

    return zeder_id_and_instance_to_zeder_journal_id_map;
}


unsigned GetJournalId(const unsigned zeder_id, const std::string &zeder_instance, DbConnection * const db_connection) {
    static const auto zeder_id_and_instance_to_journal_id_map(GetZederIdAndInstanceToJournalIdMap(db_connection));
    return zeder_id_and_instance_to_journal_id_map.at(std::to_string(zeder_id) + "#" + zeder_instance);
}


bool RecordIsOnlineFirstOrEarlyView(const MARC::Record &record) {
    // Skip if volume and issue are missing or invalid
    const auto volume_and_issue(record.getSubfieldValues("936", "ed"));
    return volume_and_issue.empty() or (std::find(volume_and_issue.begin(), volume_and_issue.end(), "n/a") != volume_and_issue.end());
}

const std::string ONLINE_FIRST_OR_EARLY_VIEW_MESSAGE("Online-first or Early-View");
bool RecordIsValid(DbConnection * const db_connection, const MARC::Record &record, const std::vector<const FieldValidator *> &regular_article_field_validators,
                   const std::vector<const FieldValidator *> &review_article_field_validators, std::vector<std::string> * const reasons_for_being_invalid)
{
    reasons_for_being_invalid->clear();

    // Filter Online-First or Early Views unconditionally
    if (RecordIsOnlineFirstOrEarlyView(record)) {
        reasons_for_being_invalid->emplace_back(ONLINE_FIRST_OR_EARLY_VIEW_MESSAGE);
        return false;
    }

    const auto zid_field(record.findTag("ZID"));
    if (unlikely(zid_field == record.end()))
        LOG_ERROR("record is missing a ZID field!");
    const auto zeder_id(zid_field->getFirstSubfieldWithCode('a'));
    if (unlikely(zeder_id.empty()))
        LOG_ERROR("record is missing an a-subfield in the existing ZID field!");
    const auto zeder_instance(zid_field->getFirstSubfieldWithCode('b'));
    if (unlikely(zeder_instance.empty()))
        LOG_ERROR("record is missing a b-subfield in the existing ZID field!");
    const auto journal_id(GetJournalId(StringUtil::ToUnsigned(zeder_id), zeder_instance, db_connection));


    // 0. Check that requirements for all records, independent of type or journal are met:
    CheckGenericRequirements(record, reasons_for_being_invalid);

    // 1. Check that present fields meet all the requirements:
    MARC::Tag last_tag("   ");
    const auto &field_validators(record.isReviewArticle() ? review_article_field_validators : regular_article_field_validators);
    std::set<std::string> present_tags, tags_for_which_rules_were_found;
    for (const auto &field : record) {
        const auto current_tag(field.getTag());
        if (current_tag == last_tag and not field.isRepeatableField())
            reasons_for_being_invalid->emplace_back(current_tag.toString() + " is not a repeatable field");
        last_tag = current_tag;
        present_tags.emplace(current_tag.toString());

        for (const auto field_validator : field_validators) {
            std::string reason_for_being_invalid;
            if (field_validator->foundRuleMatch(journal_id, field, &reason_for_being_invalid)) {
                tags_for_which_rules_were_found.emplace(current_tag.toString());
                if (not reason_for_being_invalid.empty())
                    reasons_for_being_invalid->emplace_back(reason_for_being_invalid);
                break;
            }
        }
    }

    // 2. Check for missing required fields:
    std::set<std::string> missing_tags, checked_tags;
    for (const auto field_validator : field_validators)
        field_validator->findMissingTags(journal_id, present_tags, &missing_tags, &checked_tags);
    for (const auto &missing_tag : missing_tags)
        reasons_for_being_invalid->emplace_back("required " + missing_tag + "-field is missing");

    // 3. Complain about unknown fields:
    for (const auto &present_tag : present_tags) {
        // skip required fields with hardcoded testing
        if (REQUIRED_EXISTING_FIELD_TAGS.find(present_tag) != REQUIRED_EXISTING_FIELD_TAGS.end() or
            REQUIRED_SPECIAL_CASE_FIELD_TAGS.find(present_tag) != REQUIRED_SPECIAL_CASE_FIELD_TAGS.end())
            continue;

        if (tags_for_which_rules_were_found.find(present_tag) == tags_for_which_rules_were_found.end())
            reasons_for_being_invalid->emplace_back("no rule for present field " + present_tag + " was found");
    }

    return reasons_for_being_invalid->empty();
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 6 and argc != 7)
        Usage();

    bool update_db_errors(false);
    if (argc == 7) {
        if (__builtin_strcmp(argv[1], "--update-db-errors") != 0)
            Usage();
        --argc, ++argv;
        update_db_errors = true;
    }

    DbConnection db_connection;

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto valid_records_writer(MARC::Writer::Factory(argv[2]));
    auto online_first_records_writer(MARC::Writer::Factory(argv[3]));
    auto delinquent_records_writer(MARC::Writer::Factory(argv[4]));
    const std::string email_address(argv[5]);
    ZoteroHarvester::Util::UploadTracker upload_tracker;

    GeneralFieldValidator general_regular_article_validator, general_review_article_validator;
    JournalSpecificFieldValidator journal_specific_regular_article_validator, journal_specific_review_article_validator;
    LoadRules(&db_connection, &general_regular_article_validator, &journal_specific_regular_article_validator,
              &general_review_article_validator, &journal_specific_review_article_validator);
    std::vector<const FieldValidator *> regular_article_field_validators{ &journal_specific_regular_article_validator,
                                                                          &general_regular_article_validator },
                                        review_article_field_validators{ &journal_specific_review_article_validator,
                                                                         &general_review_article_validator,
                                                                         &journal_specific_regular_article_validator,
                                                                         &general_regular_article_validator };

    unsigned total_record_count(0), online_first_record_count(0), missed_expectation_count(0);
    while (const auto record = marc_reader->read()) {
        ++total_record_count;
        LOG_INFO(""); // intentionally empty newline !
        LOG_INFO("Validating record " + record.getControlNumber() + "...");

        std::vector<std::string> reasons_for_being_invalid;
        if (RecordIsValid(&db_connection, record, regular_article_field_validators, review_article_field_validators, &reasons_for_being_invalid)) {
            LOG_INFO("Record " + record.getControlNumber() + " is valid.");
            valid_records_writer->write(record);
        } else {
            if (std::find(reasons_for_being_invalid.begin(), reasons_for_being_invalid.end(), ONLINE_FIRST_OR_EARLY_VIEW_MESSAGE)
                          != reasons_for_being_invalid.end())
            {
                LOG_INFO("Record " + record.getControlNumber() + " is online first");
                online_first_records_writer->write(record);
                ++online_first_record_count;
                upload_tracker.archiveRecord(record, ZoteroHarvester::Util::UploadTracker::DeliveryState::ONLINE_FIRST,
                                             StringUtil::Join(reasons_for_being_invalid, "\n"));
            } else {
                const std::string error_messages(StringUtil::Join(reasons_for_being_invalid, "\n"));
                LOG_WARNING("Record " + record.getControlNumber() + " is invalid:\n" + error_messages);
                ++missed_expectation_count;
                if (update_db_errors)
                    upload_tracker.archiveRecord(record, ZoteroHarvester::Util::UploadTracker::DeliveryState::ERROR,
                                                 error_messages);
                delinquent_records_writer->write(record);
            }
        }
    }

    if (missed_expectation_count > 0) {
        // send notification to the email address
        SendEmail(email_address, "validate_harvested_records encountered warnings (from: " + DnsUtil::GetHostname() + ")",
                  "Some records missed expectations with respect to MARC fields. "
                  "Check the log at '" + UBTools::GetTueFindLogPath() + "zts_harvester_delivery_pipeline.log' for details.");
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " record(s) of which " + std::to_string(missed_expectation_count) +
             " record(s) missed expectations.");

    return EXIT_SUCCESS;
}
