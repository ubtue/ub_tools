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

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Compiler.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "MiscUtil.h"
#include "Solr.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] [solr_host_and_port] user_type hostname sender_email "
              << "email_subject\n"
              << "  Sends out notification emails for journal subscribers.\n"
              << "  Should \"solr_host_and_port\" be missing \"localhost:8080\" will be used.\n"
              << "  \"user_type\" must be \"ixtheo\", \"relbib\" or some other realm."
              << "  \"hostname\" should be the symbolic hostname which will be used in constructing\n"
              << "  URL's that a user might see.\n\n";
    std::exit(EXIT_FAILURE);
}


struct SerialControlNumberAndLastIssueDate {
    std::string serial_control_number_;
    std::string last_issue_date_;
    bool changed_;
public:
    SerialControlNumberAndLastIssueDate(const std::string &serial_control_number, const std::string &last_issue_date)
        : serial_control_number_(serial_control_number), last_issue_date_(last_issue_date), changed_(false) { }
    inline void setLastIssueDate(const std::string &new_last_issue_date)
        { last_issue_date_ = new_last_issue_date; changed_ = true; }
    inline bool changed() const { return changed_; }
};


struct NewIssueInfo {
    std::string control_number_;
    std::string journal_title_;
    std::string issue_title_;
    std::string last_issue_date_;
public:
    NewIssueInfo(const std::string &control_number, const std::string &journal_title, const std::string &issue_title)
        : control_number_(control_number), journal_title_(journal_title), issue_title_(issue_title) { }
};


/** \return True if new issues were found, false o/w. */
bool ExtractNewIssueInfos(const std::string &json_document, std::vector<NewIssueInfo> * const new_issue_infos,
                          std::string * const max_last_issue_date)
{
    std::stringstream input(json_document, std::ios_base::in);
    boost::property_tree::ptree property_tree;
    boost::property_tree::json_parser::read_json(input, property_tree);

    bool found_at_least_one_new_issue(false);
    for (const auto &document : property_tree.get_child("response.docs.")) {
        const auto &id(document.second.get<std::string>("id"));
        const auto &issue_title(document.second.get<std::string>("title"));
        const auto &journal_issue_node(document.second.get_child_optional("journal_issue"));
        const std::string journal_title_received(
            not journal_issue_node ? "" : document.second.get_child("journal_issue").equal_range("").first->second.get_value<std::string>());
        const std::string journal_title(
            not journal_title_received.empty() ? journal_title_received : "*No Journal Title*");
        new_issue_infos->emplace_back(id, journal_title, issue_title);

        const auto &recording_date(document.second.get<std::string>("recording_date"));
        if (recording_date > *max_last_issue_date) {
            *max_last_issue_date = recording_date;
            found_at_least_one_new_issue = true;
        }
    }

    return found_at_least_one_new_issue;
}


bool GetNewIssues(const std::string &solr_host_and_port, const std::string &serial_control_number,
                  std::string last_issue_date, std::vector<NewIssueInfo> * const new_issue_infos,
                  std::string * const max_last_issue_date)
{
    if (not StringUtil::EndsWith(last_issue_date, "Z"))
        last_issue_date += "T00:00:00Z"; // Solr does not support the short form of the ISO 8601 date formats.
    const std::string QUERY("superior_ppn:" + serial_control_number + " AND recording_date:{" + last_issue_date
                            + " TO *}");
    std::string json_result;
    if (unlikely(not Solr::Query(QUERY, "id,title,recording_date,journal_issue", &json_result,
                                 solr_host_and_port, /* timeout = */ 5, Solr::JSON)))
        Error("Solr query failed or timed-out: \"" + QUERY + "\".");

    return ExtractNewIssueInfos(json_result, new_issue_infos, max_last_issue_date);
}


void SendNotificationEmail(const std::string &firstname, const std::string &lastname,
                           const std::string &recipient_email, const std::string &vufind_host,
                           const std::string &sender_email, const std::string &email_subject,
                           const std::vector<NewIssueInfo> &new_issue_infos)
{
    const std::string EMAIL_TEMPLATE_PATH("/var/lib/tuelib/subscriptions_email.template");
    std::string email_template;
    if (unlikely(not FileUtil::ReadString(EMAIL_TEMPLATE_PATH, &email_template)))
        Error("can't load email template \"" + EMAIL_TEMPLATE_PATH + "\"!");

    // Process the email template:
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    names_to_values_map["firstname"] = std::vector<std::string>{ firstname };
    names_to_values_map["lastname"] = std::vector<std::string>{ lastname };
    std::vector<std::string> urls, journal_titles, issue_titles;
    for (const auto &new_issue_info : new_issue_infos) {
        urls.emplace_back("https://" + vufind_host + "/Record/" + new_issue_info.control_number_);
        journal_titles.emplace_back(new_issue_info.journal_title_);
        issue_titles.emplace_back(HtmlUtil::HtmlEscape(new_issue_info.issue_title_));
    }
    names_to_values_map["url"]           = urls;
    names_to_values_map["journal_title"] = journal_titles;
    names_to_values_map["issue_title"]   = issue_titles;
    std::istringstream input(email_template);
    std::ostringstream email_contents;
    MiscUtil::ExpandTemplate(input, email_contents, names_to_values_map);

    if (unlikely(not EmailSender::SendEmail(sender_email, recipient_email, email_subject, email_contents.str(),
                                            EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        Error("failed to send a notification email to \"" + recipient_email + "\"!");
}


void ProcessSingleUser(const bool verbose, DbConnection * const db_connection, const std::string &user_id,
                       const std::string &solr_host_and_port, const std::string &hostname,
                       const std::string &sender_email, const std::string &email_subject,
                       std::vector<SerialControlNumberAndLastIssueDate> &control_numbers_and_last_issue_dates)
{
    const std::string SELECT_USER_ATTRIBUTES("SELECT * FROM user WHERE id='" + user_id + "'");
    if (unlikely(not db_connection->query(SELECT_USER_ATTRIBUTES)))
        Error("Select failed: " + SELECT_USER_ATTRIBUTES + " (" + db_connection->getLastErrorMessage() + ")");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        Error("found no user attributes in table \"user\" for ID \"" + user_id + "\"!");
    if (result_set.size() > 1)
        Error("found multiple user attribute sets in table \"user\" for ID \"" + user_id + "\"!");

    const DbRow row(result_set.getNextRow());
    const std::string username(row["username"]);
    if (verbose)
        std::cerr << "Found " << control_numbers_and_last_issue_dates.size() << " subscriptions for \"" << username
                  << ".\n";

    const std::string firstname(row["firstname"]);
    const std::string lastname(row["lastname"]);
    const std::string email(row["email"]);

    // Collect the dates for new issues.
    std::vector<NewIssueInfo> new_issue_infos;
    for (auto &control_number_and_last_issue_date : control_numbers_and_last_issue_dates) {
        std::string max_last_issue_date(control_number_and_last_issue_date.last_issue_date_);
        if (GetNewIssues(solr_host_and_port, control_number_and_last_issue_date.serial_control_number_,
                         control_number_and_last_issue_date.last_issue_date_, &new_issue_infos, &max_last_issue_date))
            control_number_and_last_issue_date.setLastIssueDate(max_last_issue_date);
    }
    if (verbose)
        std::cerr << "Found " << new_issue_infos.size() << " new issues for " << " \"" << username << "\".\n";

    if (not new_issue_infos.empty())
        SendNotificationEmail(firstname, lastname, email, hostname, sender_email, email_subject, new_issue_infos);

    // Update the database with the new last issue dates.
    for (const auto &control_number_and_last_issue_date : control_numbers_and_last_issue_dates) {
        if (not control_number_and_last_issue_date.changed())
            continue;

        const std::string REPLACE_STMT("REPLACE INTO ixtheo_journal_subscriptions SET id=" + user_id
                                       + ",last_issue_date='" + control_number_and_last_issue_date.last_issue_date_
                                       + "',journal_control_number='"
                                       + control_number_and_last_issue_date.serial_control_number_ + "'");
        if (unlikely(not db_connection->query(REPLACE_STMT)))
            Error("Replace failed: " + REPLACE_STMT + " (" + db_connection->getLastErrorMessage() + ")");
    }
}


void ProcessSubscriptions(const bool verbose, DbConnection * const db_connection,
                          const std::string &solr_host_and_port, const std::string &user_type,
                          const std::string &hostname, const std::string &sender_email,
                          const std::string &email_subject)
{
    const std::string SELECT_IDS_STMT("SELECT DISTINCT id FROM ixtheo_journal_subscriptions "
                                      "WHERE id IN (SELECT id FROM ixtheo_user WHERE ixtheo_user.user_type = '"
                                      + user_type  + "')");
    if (unlikely(not db_connection->query(SELECT_IDS_STMT)))
        Error("Select failed: " + SELECT_IDS_STMT + " (" + db_connection->getLastErrorMessage() + ")");

    unsigned subscription_count(0);
    DbResultSet id_result_set(db_connection->getLastResultSet());
    const unsigned user_count(id_result_set.size());
    while (const DbRow id_row = id_result_set.getNextRow()) {
        const std::string user_id(id_row["id"]);

        const std::string SELECT_SUBSCRIPTION_INFO("SELECT journal_control_number,last_issue_date FROM "
                                                   "ixtheo_journal_subscriptions WHERE id=" + user_id);
        if (unlikely(not db_connection->query(SELECT_SUBSCRIPTION_INFO)))
            Error("Select failed: " + SELECT_SUBSCRIPTION_INFO + " (" + db_connection->getLastErrorMessage() + ")");

        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<SerialControlNumberAndLastIssueDate> control_numbers_and_last_issue_dates;
        while (const DbRow row = result_set.getNextRow()) {
            control_numbers_and_last_issue_dates.emplace_back(SerialControlNumberAndLastIssueDate(
                row["journal_control_number"], row["last_issue_date"]));
            ++subscription_count;
        }
        ProcessSingleUser(verbose, db_connection, user_id, solr_host_and_port, hostname, sender_email,
                          email_subject, control_numbers_and_last_issue_dates);
    }

    if (verbose)
        std::cout << "Processed " << user_count << " users and " << subscription_count << " subscriptions.\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();

    bool verbose;
    if (std::strcmp("--verbose", argv[1]) == 0) {
        if (argc < 6)
            Usage();
        verbose = true;
        --argc, ++argv;
    } else
        verbose = false;
    
    std::string solr_host_and_port;
    if (argc == 5)
        solr_host_and_port = "localhost:8080";
    else if (argc == 6) {
        solr_host_and_port = argv[1];
        --argc, ++argv;
    } else
        Usage();
    
    const std::string user_type(argv[1]);
    if (user_type != "ixtheo" and user_type != "relbib")
        Error("user_type parameter must be either \"ixtheo\" or \"relbib\"!");
    
    const std::string hostname(argv[2]);
    const std::string sender_email(argv[3]);
    const std::string email_subject(argv[4]);

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        ProcessSubscriptions(verbose, &db_connection, solr_host_and_port, user_type, hostname,
                             sender_email, email_subject);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
