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
#include <vector>
#include <cstdlib>
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Compiler.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "Solr.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "VuFind.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] [solr_host_and_port]\n";
    std::cerr << "  Sends out notification emails for journal subscribers.\n";
    std::cerr << "  Should \"solr_host_and_port\" be missing \"localhost:8080\" will be used.\n";
    std::exit(EXIT_FAILURE);
}


struct SerialControlNumberAndLastIssueDate {
    std::string serial_control_number_;
    std::string last_issue_date_;
public:
    SerialControlNumberAndLastIssueDate(const std::string &serial_control_number, const std::string &last_issue_date)
        : serial_control_number_(serial_control_number), last_issue_date_(last_issue_date) { }
};


struct NewIssueInfo {
    std::string control_number_;
    std::string title_;
public:
    NewIssueInfo(const std::string &control_number, const std::string &title)
        : control_number_(control_number), title_(title) { }
};


void ExtractNewIssueInfos(const std::string &json_document, std::vector<NewIssueInfo> * const new_issue_infos,
                          std::string * const max_last_issue_date)
{
    std::stringstream input(json_document, std::ios_base::in);
    boost::property_tree::ptree property_tree;
    boost::property_tree::json_parser::read_json(input, property_tree);

    for (const auto &document : property_tree.get_child("response.docs.")) {
        const auto &id(document.second.get<std::string>("id"));
        const auto &title(document.second.get<std::string>("title"));
        new_issue_infos->emplace_back(id, title);

        const auto &recording_date(document.second.get<std::string>("recording_date"));
        if (recording_date > *max_last_issue_date)
            *max_last_issue_date = recording_date;
    }
}


void GetNewIssues(const std::string &solr_host_and_port, const std::string &serial_control_number,
                  std::string last_issue_date, std::vector<NewIssueInfo> * const new_issue_infos,
                  std::string * const max_last_issue_date)
{
    if (not StringUtil::EndsWith(last_issue_date, "Z"))
        last_issue_date += "T00:00:00Z"; // Solr does not support the short form of the ISO 8601 date formats.
    const std::string QUERY("superior_ppn: " + serial_control_number + " AND recording_date:{" + last_issue_date
                            + " TO *}");
    std::string json_result;
    if (unlikely(not Solr::Query(QUERY, "title,author,recording_date", &json_result, solr_host_and_port,
                                 /* timeout = */ 5, Solr::XML)))
    {
        std::cerr << "Solr query failed or timed-out: \"" << QUERY << "\".\n";
        return;
    }

    ExtractNewIssueInfos(json_result, new_issue_infos, max_last_issue_date);
}


void SendNotificationEmail(const std::string &firstname, const std::string &lastname, const std::string &email,
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
    std::vector<std::string> urls, titles;
    for (const auto &new_issue_info : new_issue_infos) {
        urls.emplace_back(new_issue_info.control_number_);
        titles.emplace_back(new_issue_info.title_);
    }
    names_to_values_map["urls"] = urls;
    names_to_values_map["titles"] = titles;
    const std::string email_contents(MiscUtil::ExpandTemplate(email_template, names_to_values_map));

    if (unlikely(EmailSender::SendEmail("notifications@ixtheo.de", email, "Ixtheo Subscriptions", email_contents,
                                        EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        Error("failed to send a notification email to \"" + email + "\"!");
}


void ProcessSingleUser(const bool verbose, DbConnection * const db_connection, const std::string &user_id,
                       const std::string &solr_host_and_port,
                       const std::vector<SerialControlNumberAndLastIssueDate> &control_numbers_and_last_issue_dates)
{
    const std::string SELECT_USER_ATTRIBUTES("SELECT * FROM user WHERE id=" + user_id);
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

    std::vector<NewIssueInfo> new_issue_infos;
    for (const auto &control_number_and_last_issue_date : control_numbers_and_last_issue_dates) {
        std::string max_last_issue_date(control_number_and_last_issue_date.last_issue_date_);
        GetNewIssues(solr_host_and_port, control_number_and_last_issue_date.serial_control_number_,
                     control_number_and_last_issue_date.last_issue_date_, &new_issue_infos, &max_last_issue_date);

        const std::string REPLACE_STMT("REPLACE INTO ixtheo_journal_subscriptions SET last_issue_date='"
                                       + max_last_issue_date + "'");
        if (unlikely(not db_connection->query(REPLACE_STMT)))
            Error("Replace failed: " + REPLACE_STMT + " (" + db_connection->getLastErrorMessage() + ")");
    }
    if (verbose)
        std::cerr << "Found " << new_issue_infos.size() << " new issues for " << " \"" << username << "\".\n";
    if (not new_issue_infos.empty())
        SendNotificationEmail(firstname, lastname, email, new_issue_infos);
}


void ProcessSubscriptions(const bool verbose, DbConnection * const db_connection,
                          const std::string &solr_host_and_port)
{
    const std::string SELECT_STMT("SELECT * FROM ixtheo_journal_subscriptions ORDER BY id");
    if (unlikely(not db_connection->query(SELECT_STMT)))
	Error("Select failed: " + SELECT_STMT + " (" + db_connection->getLastErrorMessage() + ")");

    std::string current_id;
    std::vector<SerialControlNumberAndLastIssueDate> control_numbers_and_last_issue_dates;
    DbResultSet id_result_set(db_connection->getLastResultSet());
    unsigned user_count(0), subscription_count(0);
    while (const DbRow id_row = id_result_set.getNextRow()) {
        const std::string user_id(id_row["id"]);

        if (user_id != current_id) {
            ++user_count;
            if (not control_numbers_and_last_issue_dates.empty()) {
                ProcessSingleUser(verbose, db_connection, current_id, solr_host_and_port,
                                  control_numbers_and_last_issue_dates);
                control_numbers_and_last_issue_dates.clear();
            }
            current_id = user_id;
        }

        control_numbers_and_last_issue_dates.emplace_back(SerialControlNumberAndLastIssueDate(
            id_row["journal_control_number"], id_row["last_issue_date"]));
        ++subscription_count;
    }

    if (not control_numbers_and_last_issue_dates.empty())
        ProcessSingleUser(verbose, db_connection, current_id, solr_host_and_port, control_numbers_and_last_issue_dates);

    if (verbose)
        std::cout << "Processed " << user_count << " users and " << subscription_count << " subscriptions.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc > 3)
        Usage();

    bool verbose(false);
    std::string solr_host_and_port("localhost:8080");
    if (argc == 3) {
        if (std::strcmp("--verbose", argv[1]) != 0)
            Usage();
        verbose = true;
        solr_host_and_port = argv[2];
    } else if (argc == 2) {
        if (std::strcmp("--verbose", argv[1]) == 0)
            verbose = true;
        else
            solr_host_and_port = argv[1];
    }

    try {
	std::string mysql_url;
	VuFind::GetMysqlURL(&mysql_url);
	DbConnection db_connection(mysql_url);

        ProcessSubscriptions(verbose, &db_connection, solr_host_and_port);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
