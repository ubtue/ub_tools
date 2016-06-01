/** \file    new_journal_alert.cc
 *  \brief   Detects new journal issues for subscribed users.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include <cstring>
#include "DbConnection.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input notification_log_filename\n";
    std::cerr << "  Sends out notification emails for journal subscribers.\n";
    std::exit(EXIT_FAILURE);
}


inline bool AllDigits(const std::string &s) {
    for (char ch : s) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


bool IsValidDate(const std::string &date_string) {
    if (unlikely(date_string.length() != 6))
        return false;

    if (unlikely(not AllDigits(date_string)))
        return false;

    const unsigned day_candidate(date_string[0] - '0' + (date_string[1] - '0') * 10);
    if (unlikely(day_candidate < 1 or day_candidate > 31))
        return false;

    const unsigned month_candidate(date_string[3] - '0' + (date_string[2] - '0') * 10);
    if (unlikely(month_candidate < 1 or month_candidate > 12))
        return false;

    return true;
}


void CreateNotificationLog(const bool verbose, File * const marc_input, File * const /*notification_log_output*/) {
    if (verbose)
        std::cout << "Starting paersing of MARC input.\n";

    unsigned count(0), serial_record_count(0), matched_serial_count(0);
    std::string err_msg;
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(marc_input)) {
        ++count;

	const Leader &leader(record.getLeader());
        if (not leader.isSerial())
            continue;

	const std::vector<std::string> &fields(record.getFields());

        const ssize_t _008_index(record.getFieldIndex("008"));
        if (_008_index == -1) {
            std::cerr << "008 field is missing for the record with the control number " << fields[0] << ".\n";
            continue;
        }

        // Extract the date:
        const std::string date_string(fields[_008_index].substr(0, 5));
        if (unlikely(not IsValidDate(date_string))) {
            std::cerr << "Record with the control number " << fields[0] << " has a bad date \""
                      << date_string << "\".\n";
            continue;
        }
    }

    if (not err_msg.empty())
        Error(err_msg);

    if (verbose) {
        std::cerr << "Read " << count << " records.\n";
        std::cerr << "Found " << serial_record_count << " serial recordss.\n";
        std::cerr << "Logged " << matched_serial_count << " serial records.\n";
    }
}


std::unique_ptr<File> OpenInputFile(const std::string &filename) {
    std::string mode("r");
    mode += MediaTypeUtil::GetFileMediaType(filename) == "application/lz4" ? "u" : "";
    std::unique_ptr<File> file(new File(filename, mode));
    if (file == nullptr)
        Error("can't open \"" + filename + "\" for reading!");

    return file;
}


struct JournalTitleAndLastIssueDate {
    std::string journal_title_;
    std::string last_issue_date_; // YYYYMMDD
public:
    JournalTitleAndLastIssueDate(const std::string &journal_title, const std::string &last_issue_date)
        : journal_title_(journal_title), last_issue_date_(last_issue_date) { }

    inline bool operator==(const JournalTitleAndLastIssueDate &rhs) const {
        return journal_title_ == rhs.journal_title_;
    }
};


template <> struct std::hash<JournalTitleAndLastIssueDate> {
    std::size_t operator()(const JournalTitleAndLastIssueDate &journal_title_and_last_issue_date) const {
        return std::hash<std::string>()(journal_title_and_last_issue_date.journal_title_);
    }
};


struct JournalSubscriberInfo {
    std::string email_address_;
    std::string first_name_;
    std::string last_name_;
    std::unordered_set<JournalTitleAndLastIssueDate> journal_titles_and_last_issue_dates_;
public:
    JournalSubscriberInfo(const std::string &email_address, const std::string &first_name,
                          const std::string &last_name)
        : email_address_(email_address), first_name_(first_name), last_name_(last_name) { }
    void addJournalTitleAndLastIssueDate(const JournalTitleAndLastIssueDate &new_journal_title_and_last_issue_date) {
        journal_titles_and_last_issue_dates_.insert(new_journal_title_and_last_issue_date);
    }
};


// Populates the user-specific parts of a JournalSubscriberInfo struct with data found in the vufind.user table.
JournalSubscriberInfo GetJournalSubscriberInfo(DbConnection * const db_connection, const std::string &user_id) {
    const std::string SELECT_STMT("SELECT email,firstname,lastname FROM user WHERE id=" + user_id);
    if (unlikely(not db_connection->query(SELECT_STMT)))
        Error("Select failed: " + SELECT_STMT + " (" + db_connection->getLastErrorMessage() + ")");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (unlikely(result_set.empty())) // This should be impossible!
        throw std::runtime_error("result set was empty for query: " + SELECT_STMT);
    const DbRow row(result_set.getNextRow());
    return JournalSubscriberInfo(row["email"], row["firstname"], row["lastname"]);
}


void GetUsersAndJournals(
    DbConnection * const db_connection,
    std::vector<std::pair<JournalSubscriberInfo, std::unordered_set<JournalTitleAndLastIssueDate>>>
        * const users_and_journals)
{
    users_and_journals->clear();

    const std::string SELECT_IDS_STMT("SELECT DISTINCT id FROM ixtheo_journal_subscriptions");
    if (unlikely(not db_connection->query(SELECT_IDS_STMT)))
	Error("Select failed: " + SELECT_IDS_STMT + " (" + db_connection->getLastErrorMessage() + ")");

    DbResultSet id_result_set(db_connection->getLastResultSet());
    while (const DbRow id_row = id_result_set.getNextRow()) {
        const std::string user_id(id_row["id"]);
        const JournalSubscriberInfo new_journal_subscriber_info(GetJournalSubscriberInfo(db_connection, user_id));

        const std::string SELECT_TITLES_AND_DATES_STMT("SELECT journal_title,last_issue_date FROM "
                                                       "ixtheo_journal_subscriptions WHERE id=" + user_id);
        if (unlikely(not db_connection->query(SELECT_TITLES_AND_DATES_STMT)))
            Error("Select failed: " + SELECT_TITLES_AND_DATES_STMT + " (" + db_connection->getLastErrorMessage()
                  + ")");
        DbResultSet result_set(db_connection->getLastResultSet());
        std::unordered_set<JournalTitleAndLastIssueDate> titles_and_dates;
        while (const DbRow row = result_set.getNextRow())
            titles_and_dates.emplace(row["journal_title"], row["last_issue_date"]);
        users_and_journals->emplace_back(std::make_pair(new_journal_subscriber_info, titles_and_dates));
    }
}


void PopulateJournalTitleToUserInfoMap(
    const std::vector<std::pair<JournalSubscriberInfo, std::unordered_set<JournalTitleAndLastIssueDate>>>
        &users_and_journals,
    std::unordered_map<std::string, std::vector<JournalSubscriberInfo const *>> * const
        journal_title_to_user_info_map)
{
    journal_title_to_user_info_map->clear();

    for (const auto &user_and_journals : users_and_journals) {
        for (const auto &title_and_date : user_and_journals.second) {
            auto journal_title_and_user_infos(journal_title_to_user_info_map->find(title_and_date.journal_title_));
            if (journal_title_and_user_infos == journal_title_to_user_info_map->end()) {
                journal_title_to_user_info_map->emplace(title_and_date.journal_title_,
                                                        std::vector<JournalSubscriberInfo const *>());
                journal_title_and_user_infos = journal_title_to_user_info_map->find(title_and_date.journal_title_);
            }
            journal_title_and_user_infos->second.emplace_back(&user_and_journals.first);
        }
    }
}


int main(int argc, char **argv) {
    progname = argv[0];

    if ((argc != 3 and argc != 4)
        or (argc == 4 and std::strcmp(argv[1], "-v") != 0 and std::strcmp(argv[1], "--verbose") != 0))
        Usage();
    const bool verbose(argc == 3);

    const std::string marc_input_filename(argv[argc == 4 ? 2 : 1]);
    std::unique_ptr<File> marc_input(OpenInputFile(marc_input_filename));

    try {
	std::string mysql_url;
	VuFind::GetMysqlURL(&mysql_url);
	DbConnection db_connection(mysql_url);
        std::vector<std::pair<JournalSubscriberInfo, std::unordered_set<JournalTitleAndLastIssueDate>>>
            users_and_journals;
        GetUsersAndJournals(&db_connection, &users_and_journals);
        if (verbose)
            std::cout << "Found entries for " << users_and_journals.size()
                      << " users that are interested in notifications.\n";

        std::unordered_map<std::string, std::vector<JournalSubscriberInfo const *>> journal_title_to_user_info_map;
        PopulateJournalTitleToUserInfoMap(users_and_journals, &journal_title_to_user_info_map);
        if (verbose)
            std::cout << "Found " << journal_title_to_user_info_map.size()
                      << " serials for which there is at least one subscriber each.\n";

        const std::string notification_log_filename(argv[verbose ? 3 : 2]);
        File notification_log_output(notification_log_filename, "w");
        if (not notification_log_output)
            Error("can't open \"" + notification_log_filename + "\" for writing!");

        CreateNotificationLog(verbose, marc_input.get(), &notification_log_output);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
