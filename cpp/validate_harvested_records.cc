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
#include "EmailSender.h"
#include "IniFile.h"
#include "MARC.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
   ::Usage("marc_input marc_output missed_expectations_file email_address");
}


enum FieldPresence { ALWAYS, SOMETIMES, IGNORE };


struct FieldInfo {
    std::string name_;
    FieldPresence presence_;
public:
    FieldInfo(const std::string &name, const FieldPresence presence): name_(name), presence_(presence) { }
};


class JournalInfo {
    bool not_in_database_yet_;
    std::vector<FieldInfo> field_infos_;
public:
    using const_iterator = std::vector<FieldInfo>::const_iterator;
    using iterator = std::vector<FieldInfo>::iterator;
public:
    explicit JournalInfo(const bool not_in_database_yet): not_in_database_yet_(not_in_database_yet) { }
    JournalInfo() = default;
    JournalInfo(const JournalInfo &rhs) = default;

    size_t size() const { return field_infos_.size(); }
    bool isInDatabase() const { return not not_in_database_yet_; }
    void addField(const std::string &field_name, const FieldPresence field_presence)
        { field_infos_.emplace_back(field_name, field_presence); }
    const_iterator begin() const { return field_infos_.cbegin(); }
    const_iterator end() const { return field_infos_.cend(); }
    iterator begin() { return field_infos_.begin(); }
    iterator end() { return field_infos_.end(); }
    iterator find(const std::string &field_name) {
        return std::find_if(field_infos_.begin(), field_infos_.end(),
                            [&field_name](const FieldInfo &field_info){ return field_name == field_info.name_; });
    }
};


std::string GetJournalNameOrDie(const MARC::Record &record) {
    const auto journal_name(record.getSuperiorTitle());
    if (unlikely(journal_name.empty()))
        LOG_ERROR("the record w/ control number \"" + record.getControlNumber() + "\" is missing a superior title!");

    return journal_name;
}


FieldPresence StringToFieldPresence(const std::string &s) {
    if (s == "always")
        return ALWAYS;
    if (s == "sometimes")
        return SOMETIMES;
    if (s == "ignore")
        return IGNORE;
    LOG_ERROR("unknown enumerated value \"" + s + "\"!");
    __builtin_unreachable();
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
        __builtin_unreachable();
    }
}


void LoadFromDatabaseOrCreateFromScratch(DbConnection * const db_connection, const std::string &journal_name,
                                         JournalInfo * const journal_info)
{
    db_connection->queryOrDie("SELECT metadata_field_name,field_presence FROM metadata_presence_tracer WHERE journal_name='"
                              + db_connection->escapeString(journal_name) + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        LOG_INFO("\"" + journal_name + "\" was not yet in the database.");
        *journal_info = JournalInfo(/* not_in_database_yet = */true);
        return;
    }

    *journal_info = JournalInfo(/* not_in_database_yet = */false);
    while (auto row = result_set.getNextRow())
        journal_info->addField(row["metadata_field_name"], StringToFieldPresence(row["field_presence"]));
    LOG_INFO("Loadad " + std::to_string(journal_info->size()) + " entries for \"" + journal_name + "\" from the database.");
}


// Two-way mapping required as the map is uni-directional
const std::map<std::string, std::string> EQUIVALENT_TAGS_MAP{
    { "700", "100" }, { "100", "700" }
};


void AnalyseNewJournalRecord(const MARC::Record &record, const bool first_record, JournalInfo * const journal_info) {
    std::unordered_set<std::string> seen_tags;
    MARC::Tag last_tag;
    for (const auto &field : record) {
        auto current_tag(field.getTag());
        if (current_tag == last_tag)
            continue;

        seen_tags.emplace(current_tag.toString());

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


bool RecordMeetsExpectations(const MARC::Record &record, const std::string &journal_name, const JournalInfo &journal_info) {
    std::unordered_set<std::string> seen_tags;
    MARC::Tag last_tag;
    for (const auto &field : record) {
        const auto current_tag(field.getTag());
        if (current_tag == last_tag)
            continue;
        seen_tags.emplace(current_tag.toString());
        last_tag = current_tag;
    }

    bool missed_at_least_one_expectation(false);
    for (const auto &field_info : journal_info) {
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
            missed_at_least_one_expectation = true;
        }
    }

    return not missed_at_least_one_expectation;
}


void WriteToDatabase(DbConnection * const db_connection, const std::string &journal_name, const JournalInfo &journal_info) {
    for (const auto &field_info : journal_info)
        db_connection->queryOrDie("INSERT INTO metadata_presence_tracer SET journal_name='" + journal_name
                                  + "', metadata_field_name='" + db_connection->escapeString(field_info.name_)
                                  + "', field_presence='" + FieldPresenceToString(field_info.presence_) + "'");
}


void SendEmail(const std::string &email_address, const std::string &message_subject, const std::string &message_body) {
    const auto reply_code(EmailSender::SendEmail("zts_harvester_delivery_pipeline@uni-tuebingen.de",
                          email_address, message_subject, message_body,
                          EmailSender::MEDIUM, EmailSender::PLAIN_TEXT, /* reply_to = */ "",
                          /* use_ssl = */ true, /* use_authentication = */ true));

    if (reply_code >= 300)
        LOG_ERROR("failed to send email, the response code was: " + std::to_string(reply_code));
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 5)
        Usage();

    DbConnection db_connection;
    auto reader(MARC::Reader::Factory(argv[1]));
    auto valid_records_writer(MARC::Writer::Factory(argv[2]));
    auto delinquent_records_writer(MARC::Writer::Factory(argv[3]));
    std::map<std::string, JournalInfo> journal_name_to_info_map;
    const std::string email_address(argv[4]);

    unsigned total_record_count(0), new_record_count(0), missed_expectation_count(0);
    while (const auto record = reader->read()) {
        ++total_record_count;
        const auto journal_name(GetJournalNameOrDie(record));

        auto journal_name_and_info(journal_name_to_info_map.find(journal_name));
        bool first_record(false); // True if the current record is the first encounter of a journal
        if (journal_name_and_info == journal_name_to_info_map.end()) {
            first_record = true;
            JournalInfo new_journal_info;
            LoadFromDatabaseOrCreateFromScratch(&db_connection, journal_name, &new_journal_info);
            journal_name_to_info_map[journal_name] = new_journal_info;
            journal_name_and_info = journal_name_to_info_map.find(journal_name);
        }

        bool missed_expectation(false);
        if (journal_name_and_info->second.isInDatabase()) {
            if (not RecordMeetsExpectations(record, journal_name_and_info->first, journal_name_and_info->second)) {
                missed_expectation = true;
                ++missed_expectation_count;
            }
        } else {
            AnalyseNewJournalRecord(record, first_record, &journal_name_and_info->second);
            ++new_record_count;
        }

        if (missed_expectation)
            delinquent_records_writer->write(record);
        else
            valid_records_writer->write(record);
    }

    for (const auto &journal_name_and_info : journal_name_to_info_map) {
        if (not journal_name_and_info.second.isInDatabase())
            WriteToDatabase(&db_connection, journal_name_and_info.first, journal_name_and_info.second);
    }

    if (missed_expectation_count > 0) {
        // send notification to the email address
        SendEmail(email_address, "validate_harvested_records encountered warnings",
                  "Some records missed expectations with respect to MARC fields. "
                  "Check the log at '/usr/local/var/log/tuefind/zts_harvester_delivery_pipeline.log' for details.");
    }

    LOG_INFO("Processed " + std::to_string(total_record_count) + " record(s) of which " + std::to_string(new_record_count)
             + " was/were (a) record(s) of new journals and " + std::to_string(missed_expectation_count)
             + " record(s) missed expectations.");

    return EXIT_SUCCESS;
}
