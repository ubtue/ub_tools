/** \brief Utility for validating and fixing up records harvested by zts_harvester
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DnsUtil.h"
#include "EmailSender.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
   ::Usage("(marc_input marc_output missed_expectations_file email_address)|(update_db zeder_id zeder_instance field_name field_presence)\n"
           "\tThis tool has two operating modes 1) checking MARC data for missed expectations and 2) altering these expectations.\n"
           "\tin the \"update_db\" mode, \"field_name\" must be a 3-character MARC tag and \"field_presence\" must be one of\n"
           "\tALWAYS, SOMETIMES, IGNORE.  Please note that only existing entries can be changed!");
}


enum FieldPresence { ALWAYS, SOMETIMES, IGNORE };


bool StringToFieldPresence(const std::string &field_presence_str, FieldPresence * const field_presence) {
    if (field_presence_str == "ALWAYS") {
        *field_presence = ALWAYS;
        return true;
    }

    if (field_presence_str == "SOMETIMES") {
        *field_presence = SOMETIMES;
        return true;
    }

    if (field_presence_str == "IGNORE") {
        *field_presence = IGNORE;
        return true;
    }

    return false;
}


struct FieldInfo {
    std::string name_;
    FieldPresence presence_;
public:
    FieldInfo(const std::string &name, const FieldPresence presence): name_(name), presence_(presence) { }
};


/**
 *  This struct contains non-journal-related field infos.
 *  The journal-specific struct will inherit from it.
 */
struct GeneralInfo {
    std::vector<FieldInfo> field_infos_;
public:
    using const_iterator = std::vector<FieldInfo>::const_iterator;
    using iterator = std::vector<FieldInfo>::iterator;

    GeneralInfo() = default;
    GeneralInfo(const GeneralInfo &rhs) = default;

    size_t size() const { return field_infos_.size(); }

    void addField(const std::string &field_name, const FieldPresence field_presence)
        { field_infos_.emplace_back(field_name, field_presence); }

    void addField(const FieldInfo &field_info) { field_infos_.emplace_back(field_info); }

    const_iterator begin() const { return field_infos_.cbegin(); }
    const_iterator end() const { return field_infos_.cend(); }
    const_iterator find(const std::string &field_name) const {
        return std::find_if(field_infos_.begin(), field_infos_.end(),
                            [&field_name](const FieldInfo &field_info){ return field_name == field_info.name_; });
    }
    iterator begin() { return field_infos_.begin(); }
    iterator end() { return field_infos_.end(); }
    iterator find(const std::string &field_name) {
        return std::find_if(field_infos_.begin(), field_infos_.end(),
                            [&field_name](const FieldInfo &field_info){ return field_name == field_info.name_; });
    }

    // Combine GeneralInfo with other General Info (e.g. JournalInfo).
    // rhs will have priority to simulate data inheritance.
    static GeneralInfo Combine(const GeneralInfo &lhs, const GeneralInfo &rhs);
};


GeneralInfo GeneralInfo::Combine(const GeneralInfo &lhs, const GeneralInfo &rhs) {
    auto lhs_iter(lhs.begin());
    auto rhs_iter(rhs.begin());

    GeneralInfo combined_info;
    while (lhs_iter != lhs.end() or rhs_iter != rhs.end()) {
        if (lhs_iter == lhs.end()) {
            combined_info.addField(*rhs_iter);
            ++rhs_iter;
        } else if (rhs_iter == rhs.end()) {
            combined_info.addField(*lhs_iter);
            ++lhs_iter;
        } else {
            const int compare_result(StringUtil::AlphaWordCompare(lhs_iter->name_, rhs_iter->name_));
            if (compare_result == 0) {
                // if present on both sides, rhs wins!
                combined_info.addField(*rhs_iter);
                ++lhs_iter;
                ++rhs_iter;
            } else if (compare_result < 0) {
                combined_info.addField(*lhs_iter);
                ++lhs_iter;
            } else if (compare_result > 0) {
                combined_info.addField(*rhs_iter);
                ++rhs_iter;
            }
        }
    }

    return combined_info;
}


class JournalInfo : public GeneralInfo {
    std::string zeder_id_;
    std::string zeder_instance_;
    bool not_in_database_yet_;
public:
    JournalInfo(const std::string &zeder_id, const std::string &zeder_instance,
                const bool not_in_database_yet): zeder_id_(zeder_id), zeder_instance_(zeder_instance),
                not_in_database_yet_(not_in_database_yet) { }
    JournalInfo() = default;
    JournalInfo(const JournalInfo &rhs) = default;

    const std::string &getZederId() const { return zeder_id_; }
    const std::string &getZederInstance() const { return zeder_instance_; }
    bool isInDatabase() const { return not not_in_database_yet_; }
};


FieldPresence StringToFieldPresence(const std::string &s) {
    if (s == "always")
        return ALWAYS;
    if (s == "sometimes")
        return SOMETIMES;
    if (s == "ignore")
        return IGNORE;
    LOG_ERROR("unknown enumerated value \"" + s + "\"!");
}


std::string FieldPresenceToString(const FieldPresence field_presence) {
    switch (field_presence) {
    case ALWAYS:
        return "always";
    case SOMETIMES:
        return "sometimes";
    case IGNORE:
        return "ignore";
    default:
        LOG_ERROR("we should *never get here!");
    }
}


GeneralInfo LoadGeneralInfo(DbConnection * const db_connection) {
    db_connection->queryOrDie("SELECT metadata_field_name,field_presence FROM metadata_presence_tracer "
                              "WHERE zeder_journals.zeder_id IS NULL ORDER BY metadata_field_name ASC");

    GeneralInfo general_info;
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        general_info.addField(row["metadata_field_name"], StringToFieldPresence(row["field_presence"]));

    return general_info;
}


void LoadFromDatabaseOrCreateFromScratch(DbConnection * const db_connection, const std::string &zeder_id,
                                         const std::string &zeder_instance, JournalInfo * const journal_info)
{
    db_connection->queryOrDie("SELECT metadata_field_name,field_presence FROM metadata_presence_tracer "
                              "LEFT JOIN zeder_journals ON zeder_journals.id = metadata_presence_tracer.zeder_journal_id "
                              "WHERE zeder_journals.zeder_id=" + db_connection->escapeAndQuoteString(zeder_id) +
                              " AND zeder_journals.zeder_instance=" + db_connection->escapeAndQuoteString(zeder_instance) +
                              " ORDER BY metadata_presence_tracer.metadata_field_name ASC");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        LOG_INFO(zeder_id + "(" + zeder_instance + ")" + " was not yet in the database.");
        *journal_info = JournalInfo(zeder_id, zeder_instance, /* not_in_database_yet = */true);
        return;
    }

    *journal_info = JournalInfo(zeder_id, zeder_instance, /* not_in_database_yet = */false);
    while (const auto row = result_set.getNextRow())
        journal_info->addField(row["metadata_field_name"], StringToFieldPresence(row["field_presence"]));
}


// Two-way mapping required as the map is uni-directional
const std::map<std::string, std::string> EQUIVALENT_TAGS_MAP{
    { "700", "100" }, { "100", "700" }
};


void AnalyseNewJournalRecord(const MARC::Record &record, const bool first_record,
                             const GeneralInfo &general_info, JournalInfo * const journal_info)
{
    std::unordered_set<std::string> seen_tags;
    MARC::Tag last_tag;
    for (const auto &field : record) {
        auto current_tag(field.getTag());
        if (current_tag == last_tag)
            continue;

        seen_tags.emplace(current_tag.toString());

        if (general_info.find(current_tag.toString()) != general_info.end())
            continue;

        if (first_record)
            journal_info->addField(current_tag.toString(), ALWAYS);
        else if (journal_info->find(current_tag.toString()) == journal_info->end())
            journal_info->addField(current_tag.toString(), SOMETIMES);

        last_tag = current_tag;
    }

    for (auto &field_info : *journal_info) {
        if (seen_tags.find(field_info.name_) == seen_tags.end())
            field_info.presence_ = SOMETIMES;
    }
}


bool RecordMeetsExpectations(const MARC::Record &record, const std::string &journal_name,
                             const GeneralInfo &general_info, const JournalInfo &journal_info)
{
    std::unordered_set<std::string> seen_tags;
    MARC::Tag last_tag;
    for (const auto &field : record) {
        const auto current_tag(field.getTag());
        if (current_tag == last_tag)
            continue;
        seen_tags.emplace(current_tag.toString());
        last_tag = current_tag;
    }

    bool meets_expectations(true);
    const GeneralInfo combined_info(GeneralInfo::Combine(general_info, journal_info));
    for (const auto &field_info : combined_info) {
        if (field_info.presence_ != ALWAYS)
            continue;   // we only care about required fields that are missing

        const auto equivalent_tag(EQUIVALENT_TAGS_MAP.find(field_info.name_));
        if (seen_tags.find(field_info.name_) != seen_tags.end())
            ;// required tag found
        else if (equivalent_tag != EQUIVALENT_TAGS_MAP.end() and seen_tags.find(equivalent_tag->second) != seen_tags.end())
            ;// equivalent tag found
        else {
            LOG_WARNING("Record w/ control number " + record.getControlNumber() + " in \"" + journal_name
                     + "\" is missing the always expected " + field_info.name_ + " field.");
            meets_expectations = false;
        }
    }

    return meets_expectations;
}


void WriteToDatabase(DbConnection * const db_connection, const GeneralInfo &general_info, const JournalInfo &journal_info) {
    for (const auto &field_info : journal_info) {
        if (general_info.find(field_info.name_) == general_info.end())
            db_connection->queryOrDie("INSERT INTO metadata_presence_tracer SET zeder_journal_id=(SELECT id FROM zeder_journals "
                                      "WHERE zeder_id=" + db_connection->escapeAndQuoteString(journal_info.getZederId()) + " "
                                      "AND zeder_instance=" + db_connection->escapeAndQuoteString(journal_info.getZederInstance()) + ")"
                                      ", metadata_field_name=" + db_connection->escapeAndQuoteString(field_info.name_) +
                                      ", field_presence='" + FieldPresenceToString(field_info.presence_) + "'");
    }
}


void SendEmail(const std::string &email_address, const std::string &message_subject, const std::string &message_body) {
    const auto reply_code(EmailSender::SendEmail("zts_harvester_delivery_pipeline@uni-tuebingen.de",
                          email_address, message_subject, message_body,
                          EmailSender::MEDIUM, EmailSender::PLAIN_TEXT, /* reply_to = */ "",
                          /* use_ssl = */ true, /* use_authentication = */ true));

    if (reply_code >= 300)
        LOG_WARNING("failed to send email, the response code was: " + std::to_string(reply_code));
}


void UpdateDB(DbConnection * const db_connection, const std::string &zeder_id, const std::string &zeder_instance,
              const std::string &field_name, const std::string &field_presence_str)
{
    FieldPresence field_presence;
    if (not StringToFieldPresence(field_presence_str, &field_presence))
        LOG_ERROR("\"" + field_presence_str + "\" is not a valid field_presence!");
    if (field_name.length() != MARC::Record::TAG_LENGTH)
        LOG_ERROR("\"" + field_name + "\" is not a valid field name!");

    db_connection->queryOrDie("UPDATE metadata_presence_tracer SET field_presence='" + field_presence_str + "' WHERE zeder_journal_id="
                              + "(SELECT id FROM zeder_journals WHERE zeder_id=" + db_connection->escapeAndQuoteString(zeder_id) + " "
                              + "AND zeder_instance=" + db_connection->escapeAndQuoteString(zeder_instance) + ") "
                              + "AND field_name='" + field_name + "'");
    if (db_connection->getNoOfAffectedRows() == 0)
        LOG_ERROR("can't update non-existent database entry: " + zeder_id + "(" + zeder_instance + ")"
                  + ", field_name: \"" + field_name + "\"");
}


bool IsRecordValid(DbConnection * const db_connection, const MARC::Record &record, const GeneralInfo &general_info,
                   std::map<std::string, JournalInfo> * const journal_name_to_info_map,
                   unsigned * const new_record_count, unsigned * const missed_expectation_count)
{
    const std::string zeder_id(record.getFirstSubfieldValue("ZID", 'a'));
    const std::string zeder_instance(record.getFirstSubfieldValue("ZID", 'b'));
    if (zeder_id.empty() or zeder_instance.empty())
        LOG_ERROR("Record w/ control number \"" + record.getControlNumber() + "\" has either no zeder_id or no zeder_instance!");

    const auto journal_name(record.getSuperiorTitle());
    if (journal_name.empty()) {
        LOG_WARNING("Record w/ control number \"" + record.getControlNumber() + "\" is missing a superior title!");
        ++(*missed_expectation_count);
        return false;
    }

    auto journal_name_and_info(journal_name_to_info_map->find(journal_name));
    bool first_record(false); // True if the current record is the first encounter of a journal
    if (journal_name_and_info == journal_name_to_info_map->end()) {
        first_record = true;
        JournalInfo new_journal_info;
        LoadFromDatabaseOrCreateFromScratch(db_connection, zeder_id, zeder_instance, &new_journal_info);
        (*journal_name_to_info_map)[journal_name] = new_journal_info;
        journal_name_and_info = journal_name_to_info_map->find(journal_name);
    }

    if (not RecordMeetsExpectations(record, journal_name_and_info->first, general_info, journal_name_and_info->second)) {
        ++(*missed_expectation_count);
        return false;
    } else if (not journal_name_and_info->second.isInDatabase()) {
        AnalyseNewJournalRecord(record, first_record, general_info, &journal_name_and_info->second);
        ++(*new_record_count);
    }

    return true;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5 and argc != 6)
        Usage();

    DbConnection db_connection;

    if (std::strcmp(argv[1], "update_db") == 0) {
        if (argc != 6)
            Usage();
        UpdateDB(&db_connection, argv[2], argv[3], argv[4], argv[5]);
        return EXIT_SUCCESS;
    }

    if (argc != 5)
        Usage();

    auto reader(MARC::Reader::Factory(argv[1]));
    auto valid_records_writer(MARC::Writer::Factory(argv[2]));
    auto delinquent_records_writer(MARC::Writer::Factory(argv[3]));
    std::map<std::string, JournalInfo> journal_name_to_info_map;
    const std::string email_address(argv[4]);
    const auto general_info(LoadGeneralInfo(&db_connection));

    unsigned total_record_count(0), new_record_count(0), missed_expectation_count(0);
    while (const auto record = reader->read()) {
        ++total_record_count;
        if (IsRecordValid(&db_connection, record, general_info, &journal_name_to_info_map,
                          &new_record_count, &missed_expectation_count))
            valid_records_writer->write(record);
        else
            delinquent_records_writer->write(record);
    }

    for (const auto &journal_name_and_info : journal_name_to_info_map) {
        if (not journal_name_and_info.second.isInDatabase())
            WriteToDatabase(&db_connection, general_info, journal_name_and_info.second);
    }

    if (missed_expectation_count > 0) {
        // send notification to the email address
        SendEmail(email_address, "validate_harvested_records encountered warnings (from: " + DnsUtil::GetHostname() + ")",
                  "Some records missed expectations with respect to MARC fields. "
                  "Check the log at '" + UBTools::GetTueFindLogPath() + "zts_harvester_delivery_pipeline.log' for details.");
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " record(s) of which " + std::to_string(new_record_count)
             + " was/were (a) record(s) of new journals and " + std::to_string(missed_expectation_count)
             + " record(s) missed expectations.");

    return EXIT_SUCCESS;
}
