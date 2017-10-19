/** \file    new_journal_alert.cc
 *  \brief   Detects new journal issues for subscribed users.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017 Library of the University of Tübingen

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
#include <memory>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <kchashdb.h>
#include "Compiler.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "JSON.h"
#include "MiscUtil.h"
#include "Solr.h"
#include "StringUtil.h"
#include "TimeUtil.h"
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


struct SerialControlNumberAndMaxLastModificationTime {
    std::string serial_control_number_;
    std::string last_modification_time_;
    bool changed_;
public:
    SerialControlNumberAndMaxLastModificationTime(const std::string &serial_control_number,
                                                  const std::string &last_modification_time)
        : serial_control_number_(serial_control_number), last_modification_time_(last_modification_time),
          changed_(false) { }
    inline void setMaxLastModificationTime(const std::string &new_last_modification_time)
        { last_modification_time_ = new_last_modification_time; changed_ = true; }
    inline bool changed() const { return changed_; }
};


struct NewIssueInfo {
    std::string control_number_;
    std::string series_title_;
    std::string issue_title_;
    std::string last_modification_time_;
public:
    NewIssueInfo(const std::string &control_number, const std::string &series_title, const std::string &issue_title)
        : control_number_(control_number), series_title_(series_title), issue_title_(issue_title) { }
};


// Makes "date" look like an ISO-8601 date ("2017-01-01 00:00:00" => "2017-01-01T00:00:00Z")
std::string ConvertDateToZuluDate(std::string date) {
    if (unlikely(date.length() != 19 or date[10] != ' '))
        logger->error("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = 'T';
    return date + 'Z';
}


// Converts ISO-8601 date back to mysql-like date format ("2017-01-01T00:00:00Z" => "2017-01-01 00:00:00")
std::string ConvertDateFromZuluDate(std::string date) {
    if (unlikely(date.length() != 20 or date[10] != 'T' or date[19] != 'Z'))
        logger->error("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = ' ';
    return date.substr(0, 19);
}


static std::string EMPTY_STRING;


std::string GetIssueId(const JSON::ObjectNode * const doc_obj) {
    const std::string id(JSON::LookupString("/id", doc_obj, &EMPTY_STRING));
    if (unlikely(id.empty()))
        logger->error("Did not find 'id' node in JSON tree!");
    
    return id;
}


std::string GetIssueTitle(const std::string &id, const JSON::ObjectNode * const doc_obj) {
    const std::string NO_AVAILABLE_TITLE("*No available title*");
    const auto issue_title(JSON::LookupString("/title", doc_obj, &NO_AVAILABLE_TITLE));
    if (unlikely(issue_title == NO_AVAILABLE_TITLE))
        logger->warning("No title found for ID " + id + "!");
    
    return issue_title;
}


std::string GetLastModificationTime(const JSON::ObjectNode * const doc_obj) {
    const std::string last_modification_time(JSON::LookupString("/last_modification_time", doc_obj, &EMPTY_STRING));
    if (unlikely(last_modification_time.empty()))
        logger->error("Did not find 'last_modification_time' node in JSON tree!");
    
    return last_modification_time;
}


std::string GetSeriesTitle(const JSON::ObjectNode * const doc_obj) {
    const std::string NO_SERIES_TITLE("*No Series Title*");
    const JSON::JSONNode * const container_ids_and_titles(doc_obj->getValue("container_ids_and_titles"));
    if (container_ids_and_titles == nullptr) {
        logger->warning("\"container_ids_and_titles\" is null");
        return NO_SERIES_TITLE;
    }
        
    const JSON::ArrayNode * const container_ids_and_titles_array(dynamic_cast<const JSON::ArrayNode *>(container_ids_and_titles));
    
    if (unlikely(container_ids_and_titles_array == nullptr))
        logger->error("\"container_ids_and_titles\" is not a JSON array!");
    if (container_ids_and_titles_array->empty()) {
        logger->warning("\"container_ids_and_titles\" is empty");
        return NO_SERIES_TITLE;
    }
    const JSON::JSONNode *first_id_and_title(container_ids_and_titles_array->getValue(0));
    const JSON::StringNode *first_id_and_title_string(dynamic_cast<const JSON::StringNode *>(first_id_and_title));
    if (first_id_and_title_string == nullptr)
        logger->error("first entry in container_ids_and_titles is not a JSON string!");
    std::string first_id_and_title_string_value(first_id_and_title_string->getValue());
    
    StringUtil::ReplaceString("#31;", "\x1F", &first_id_and_title_string_value);
    std::vector<std::string> parts;
    StringUtil::Split(first_id_and_title_string_value, '\x1F', &parts);
    if (unlikely(parts.size() < 2))
        logger->error("strange id and title value \"" + first_id_and_title_string_value + "\"!");
    
    return parts[1];
}


/** \return True if new issues were found, false o/w. */
bool ExtractNewIssueInfos(const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                          std::unordered_set<std::string> * const new_notification_ids,
                          const std::string &json_document, std::vector<NewIssueInfo> * const new_issue_infos,
                          std::string * const max_last_modification_time)
{
    bool found_at_least_one_new_issue(false);
    
    JSON::Parser parser(json_document);
    JSON::JSONNode *tree;
    if (not parser.parse(&tree))
        logger->error("in " + std::string(__FUNCTION__) + " JSON parser failed: " + parser.getErrorMessage());

    const JSON::ObjectNode * const tree_obj(dynamic_cast<const JSON::ObjectNode *>(tree));
    if (unlikely(tree_obj == nullptr))
        logger->error("in " + std::string(__FUNCTION__) + ": top level JSON entity is not an object type!");
    
    const JSON::ObjectNode * const response(dynamic_cast<const JSON::ObjectNode *>(tree_obj->getValue("response")));
    if (unlikely(response == nullptr))
        logger->error("in " + std::string(__FUNCTION__) + ": top level node \"response\" in JSON tree is missing!");
    
    const JSON::ArrayNode * const docs(dynamic_cast<const JSON::ArrayNode *>(response->getValue("docs")));
    if (unlikely(docs == nullptr))
        logger->error("in " + std::string(__FUNCTION__) + ": array node \"docs\" in JSON tree is missing!");
    
    for (auto doc(docs->cbegin()); doc != docs->cend(); ++doc) {
        const JSON::ObjectNode * const doc_obj(dynamic_cast<const JSON::ObjectNode *>(*doc));
        if (unlikely(doc_obj == nullptr))
            logger->error("in " + std::string(__FUNCTION__) + ": document object is missing!");
        
        const std::string id(GetIssueId(doc_obj));
        if (notified_db->check(id) > 0)
            continue; // We sent a notification for this issue.
        new_notification_ids->insert(id);
        
        const std::string issue_title(GetIssueTitle(id, doc_obj));
        const std::string series_title(GetSeriesTitle(doc_obj));
        
        new_issue_infos->emplace_back(id, series_title, issue_title);
        
        const std::string last_modification_time(GetLastModificationTime(doc_obj));
        if (last_modification_time > *max_last_modification_time) {
            *max_last_modification_time = last_modification_time;
            found_at_least_one_new_issue = true;
        }
    }

    return found_at_least_one_new_issue;
}


std::string GetEmailTemplate(const std::string user_type) {
    std::string result;
    const std::string EMAIL_TEMPLATE_PATH("/usr/local/var/lib/tuelib/subscriptions_email." + user_type + ".template");
    
    if (unlikely(!FileUtil::ReadString(EMAIL_TEMPLATE_PATH, &result)))
        logger->error("can't load email template \"" + EMAIL_TEMPLATE_PATH + "\"!");
    
    return result;
}


bool GetNewIssues(const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                  std::unordered_set<std::string> * const new_notification_ids, const std::string &solr_host_and_port,
                  const std::string &serial_control_number, std::string last_modification_time,
                  std::vector<NewIssueInfo> * const new_issue_infos, std::string * const max_last_modification_time)
{
    const unsigned year_current(StringUtil::ToUnsigned(TimeUtil::GetCurrentYear()));
    const unsigned year_min(year_current - 2);
    const std::string QUERY("superior_ppn:" + serial_control_number
                            + " AND last_modification_time:{" + last_modification_time + " TO *}"
                            + " AND year:[" + std::to_string(year_min) + " TO " + std::to_string(year_current) + "]"
    );
    
    std::string json_result;
    if (unlikely(not Solr::Query(QUERY, "id,title,last_modification_time,container_ids_and_titles", &json_result,
                                 solr_host_and_port, /* timeout = */ 5, Solr::JSON)))
        logger->error("Solr query failed or timed-out: \"" + QUERY + "\".");

    return ExtractNewIssueInfos(notified_db, new_notification_ids, json_result, new_issue_infos,
                                max_last_modification_time);
}


void SendNotificationEmail(const std::string &firstname, const std::string &lastname,
                           const std::string &recipient_email, const std::string &vufind_host,
                           const std::string &sender_email, const std::string &email_subject,
                           const std::vector<NewIssueInfo> &new_issue_infos,
                           const std::string &user_type)
{
    std::string email_template = GetEmailTemplate(user_type);

    // Process the email template:
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    names_to_values_map["firstname"] = std::vector<std::string>{ firstname };
    names_to_values_map["lastname"] = std::vector<std::string>{ lastname };
    std::vector<std::string> urls, series_titles, issue_titles;
    for (const auto &new_issue_info : new_issue_infos) {
        urls.emplace_back("https://" + vufind_host + "/Record/" + new_issue_info.control_number_);
        series_titles.emplace_back(new_issue_info.series_title_);
        issue_titles.emplace_back(HtmlUtil::HtmlEscape(new_issue_info.issue_title_));
    }
    names_to_values_map["url"]           = urls;
    names_to_values_map["series_title"]  = series_titles;
    names_to_values_map["issue_title"]   = issue_titles;
    std::istringstream input(email_template);
    std::ostringstream email_contents;
    MiscUtil::ExpandTemplate(input, email_contents, names_to_values_map);

    if (unlikely(not EmailSender::SendEmail(sender_email, recipient_email, email_subject, email_contents.str(),
                                            EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        logger->error("failed to send a notification email to \"" + recipient_email + "\"!");
}


void ProcessSingleUser(const bool verbose, DbConnection * const db_connection,
                       const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                       std::unordered_set<std::string> * const new_notification_ids, const std::string &user_id,
                       const std::string &solr_host_and_port, const std::string &hostname,
                       const std::string &sender_email, const std::string &email_subject,
                       std::vector<SerialControlNumberAndMaxLastModificationTime>
                           &control_numbers_and_last_modification_times)
{
    db_connection->queryOrDie("SELECT * FROM user LEFT JOIN ixtheo_user ON user.id = ixtheo_user.id WHERE user.id='"
                              + user_id + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    
    if (result_set.empty())
        logger->error("found no user attributes in table \"user\" for ID \"" + user_id + "\"!");
    if (result_set.size() > 1)
        logger->error("found multiple user attribute sets in table \"user\" for ID \"" + user_id + "\"!");

    const DbRow row(result_set.getNextRow());

    const std::string username(row["username"]);
    if (verbose)
        std::cerr << "Found " << control_numbers_and_last_modification_times.size() << " subscriptions for \""
                  << username << "\".\n";

    const std::string firstname(row["firstname"]);
    const std::string lastname(row["lastname"]);
    const std::string email(row["email"]);
    const std::string user_type(row["user_type"]);

    // Collect the dates for new issues.
    std::vector<NewIssueInfo> new_issue_infos;
    for (auto &control_number_and_last_modification_time : control_numbers_and_last_modification_times) {
        std::string max_last_modification_time(control_number_and_last_modification_time.last_modification_time_);
        if (GetNewIssues(notified_db, new_notification_ids, solr_host_and_port,
                         control_number_and_last_modification_time.serial_control_number_,
                         control_number_and_last_modification_time.last_modification_time_, &new_issue_infos,
                         &max_last_modification_time))
            control_number_and_last_modification_time.setMaxLastModificationTime(max_last_modification_time);
    }
    if (verbose)
        std::cerr << "Found " << new_issue_infos.size() << " new issues for " << "\"" << username << "\".\n";

    if (not new_issue_infos.empty())
        SendNotificationEmail(firstname, lastname, email, hostname, sender_email, email_subject, new_issue_infos,
                              user_type);

    // Update the database with the new last issue dates.
    for (const auto &control_number_and_last_modification_time : control_numbers_and_last_modification_times) {
        if (not control_number_and_last_modification_time.changed())
            continue;
        
        db_connection->queryOrDie(
            "UPDATE ixtheo_journal_subscriptions SET max_last_modification_time='"
            + ConvertDateFromZuluDate(control_number_and_last_modification_time.last_modification_time_)
            + "' WHERE id=" + user_id
            + " AND journal_control_number='" + control_number_and_last_modification_time.serial_control_number_
            + "'");
    }
}


void ProcessSubscriptions(const bool verbose, DbConnection * const db_connection,
                          const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                          std::unordered_set<std::string> * const new_notification_ids,
                          const std::string &solr_host_and_port, const std::string &user_type,
                          const std::string &hostname, const std::string &sender_email,
                          const std::string &email_subject)
{
    db_connection->queryOrDie("SELECT DISTINCT id FROM ixtheo_journal_subscriptions WHERE id IN (SELECT id FROM "
                              "ixtheo_user WHERE ixtheo_user.user_type = '" + user_type  + "')");

    unsigned subscription_count(0);
    DbResultSet id_result_set(db_connection->getLastResultSet());
    const unsigned user_count(id_result_set.size());
    while (const DbRow id_row = id_result_set.getNextRow()) {
        const std::string user_id(id_row["id"]);

        db_connection->queryOrDie("SELECT journal_control_number,max_last_modification_time FROM "
                                  "ixtheo_journal_subscriptions WHERE id=" + user_id);
        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<SerialControlNumberAndMaxLastModificationTime> control_numbers_and_last_modification_times;
        while (const DbRow row = result_set.getNextRow()) {
            control_numbers_and_last_modification_times.emplace_back(SerialControlNumberAndMaxLastModificationTime(
                row["journal_control_number"], ConvertDateToZuluDate(row["max_last_modification_time"])));
            ++subscription_count;
        }
        ProcessSingleUser(verbose, db_connection, notified_db, new_notification_ids, user_id, solr_host_and_port,
                          hostname, sender_email, email_subject, control_numbers_and_last_modification_times);
    }

    if (verbose)
        std::cout << "Processed " << user_count << " users and " << subscription_count << " subscriptions.\n";
}


void RecordNewlyNotifiedIds(const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                            const std::unordered_set<std::string> &new_notification_ids)
{
    const std::string now(TimeUtil::GetCurrentDateAndTime());
    for (const auto &id : new_notification_ids) {
        if (not notified_db->add(id, now))
            logger->error("Failed to add key/value pair to database \"" + notified_db->path() + "\" ("
                          + std::string(notified_db->error().message()) + ")!");
    }
}


std::unique_ptr<kyotocabinet::HashDB> CreateOrOpenKeyValueDB(const std::string &user_type) {
    const std::string DB_FILENAME("/usr/local/var/lib/tuelib/" + user_type + "_notified.db");
    std::unique_ptr<kyotocabinet::HashDB> db(new kyotocabinet::HashDB());
    if (not (db->open(DB_FILENAME,
                      kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::OCREATE)))
        logger->error("failed to open or create \"" + DB_FILENAME + "\"!");
    return db;
}


// gets user subscriptions for superior works from mysql
// uses kyotocabinet HashDB (file) to prevent entries from being sent multiple times to same user
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
        logger->error("user_type parameter must be either \"ixtheo\" or \"relbib\"!");
    
    const std::string hostname(argv[2]);
    const std::string sender_email(argv[3]);
    const std::string email_subject(argv[4]);

    std::unique_ptr<kyotocabinet::HashDB> notified_db(CreateOrOpenKeyValueDB(user_type));

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        std::unordered_set<std::string> new_notification_ids;
        ProcessSubscriptions(verbose, &db_connection, notified_db, &new_notification_ids, solr_host_and_port,
                             user_type, hostname, sender_email, email_subject);
        RecordNewlyNotifiedIds(notified_db, new_notification_ids);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
