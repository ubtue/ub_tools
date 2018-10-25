/** \file    new_journal_alert.cc
 *  \brief   Detects new journal issues for subscribed users.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2018 Library of the University of Tübingen

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
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "JSON.h"
#include "Solr.h"
#include "StringUtil.h"
#include "Template.h"
#include "TimeUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--debug] [solr_host_and_port] user_type hostname sender_email "
              << "email_subject\n"
              << "  Sends out notification emails for journal subscribers.\n"
              << "  Should \"solr_host_and_port\" be missing \"localhost:8080\" will be used.\n"
              << "  \"user_type\" must be \"ixtheo\", \"relbib\" or some other realm."
              << "  \"hostname\" should be the symbolic hostname which will be used in constructing\n"
              << "  URL's that a user might see.\n"
              << "  If \"--debug\" is given, emails will not be sent and database will not be updated.\n\n";
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
    std::vector<std::string> authors_;
public:
    NewIssueInfo(const std::string &control_number, const std::string &series_title, const std::string &issue_title,
                 const std::vector<std::string> &authors)
        : control_number_(control_number), series_title_(series_title), issue_title_(issue_title), authors_(authors) { }
};


// Makes "date" look like an ISO-8601 date ("2017-01-01 00:00:00" => "2017-01-01T00:00:00Z")
std::string ConvertDateToZuluDate(std::string date) {
    if (unlikely(date.length() != 19 or date[10] != ' '))
        LOG_ERROR("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = 'T';
    return date + 'Z';
}


// Converts ISO-8601 date back to mysql-like date format ("2017-01-01T00:00:00Z" => "2017-01-01 00:00:00")
std::string ConvertDateFromZuluDate(std::string date) {
    if (unlikely(date.length() != 20 or date[10] != 'T' or date[19] != 'Z'))
        LOG_ERROR("unexpected datetime in " + std::string(__FUNCTION__) + ": \"" + date + "\"!");
    date[10] = ' ';
    return date.substr(0, 19);
}


std::string GetIssueId(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string id(JSON::LookupString("/id", doc_obj, /* default_value = */ ""));
    if (unlikely(id.empty()))
        LOG_ERROR("Did not find 'id' node in JSON tree!");

    return id;
}


std::string GetIssueTitle(const std::string &id, const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string NO_AVAILABLE_TITLE("*No available title*");
    const auto issue_title(JSON::LookupString("/title", doc_obj, /* default_value = */ NO_AVAILABLE_TITLE));
    if (unlikely(issue_title == NO_AVAILABLE_TITLE))
        LOG_WARNING("No title found for ID " + id + "!");

    return issue_title;
}


std::string GetLastModificationTime(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string last_modification_time(JSON::LookupString("/last_modification_time", doc_obj, /* default_value = */ ""));
    if (unlikely(last_modification_time.empty()))
        LOG_ERROR("Did not find 'last_modification_time' node in JSON tree!");

    return last_modification_time;
}


std::string GetSeriesTitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string NO_SERIES_TITLE("*No Series Title*");
    const std::shared_ptr<const JSON::JSONNode> container_ids_and_titles(doc_obj->getNode("container_ids_and_titles"));
    if (container_ids_and_titles == nullptr) {
        LOG_WARNING("\"container_ids_and_titles\" is null");
        return NO_SERIES_TITLE;
    }

    const std::shared_ptr<const JSON::ArrayNode> container_ids_and_titles_array(
        JSON::JSONNode::CastToArrayNodeOrDie("container_ids_and_titles", container_ids_and_titles));
    if (container_ids_and_titles_array->empty()) {
        LOG_WARNING("\"container_ids_and_titles\" is empty");
        return NO_SERIES_TITLE;
    }

    std::string first_id_and_title_string_value(container_ids_and_titles_array->getStringNode(0)->getValue());
    StringUtil::ReplaceString("#31;", "\x1F", &first_id_and_title_string_value);
    std::vector<std::string> parts;
    StringUtil::Split(first_id_and_title_string_value, '\x1F', &parts);
    if (unlikely(parts.size() < 2))
        LOG_ERROR("strange id and title value \"" + first_id_and_title_string_value + "\"!");

    return parts[1];
}


std::vector<std::string> GetAuthors(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::shared_ptr<const JSON::JSONNode> author(doc_obj->getNode("author"));
    if (author == nullptr) {
        LOG_WARNING("\"author\" is null");
        return std::vector<std::string>();
    }

    const std::shared_ptr<const JSON::ArrayNode> author_array(
        JSON::JSONNode::CastToArrayNodeOrDie("author", author));
    if (author_array->empty()) {
        LOG_WARNING("\"author\" is empty");
        return std::vector<std::string>();
    }

    std::vector<std::string> authors;
    for (const auto &array_entry : *author_array) {
        const std::shared_ptr<const JSON::StringNode> author_string(JSON::JSONNode::CastToStringNodeOrDie("author string",
                                                                                                          array_entry));
        authors.emplace_back(author_string->getValue());
    }

    return authors;
}


/** \return True if new issues were found, false o/w. */
bool ExtractNewIssueInfos(const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                          std::unordered_set<std::string> * const new_notification_ids,
                          const std::string &json_document, std::vector<NewIssueInfo> * const new_issue_infos,
                          std::string * const max_last_modification_time)
{
    bool found_at_least_one_new_issue(false);

    JSON::Parser parser(json_document);
    std::shared_ptr<JSON::JSONNode> tree;
    if (not parser.parse(&tree))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ObjectNode> tree_obj(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", tree));
    const std::shared_ptr<const JSON::ObjectNode> response(tree_obj->getObjectNode("response"));
    const std::shared_ptr<const JSON::ArrayNode> docs(response->getArrayNode("docs"));

    for (const auto &doc : *docs) {
        const std::shared_ptr<const JSON::ObjectNode> doc_obj(JSON::JSONNode::CastToObjectNodeOrDie("document object", doc));

        const std::string id(GetIssueId(doc_obj));
        if (notified_db->check(id) > 0)
            continue; // We already sent a notification for this issue.
        new_notification_ids->insert(id);

        const std::string issue_title(GetIssueTitle(id, doc_obj));
        const std::string series_title(GetSeriesTitle(doc_obj));
        const std::vector<std::string> authors(GetAuthors(doc_obj));

        new_issue_infos->emplace_back(id, series_title, issue_title, authors);

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

    if (unlikely(not FileUtil::ReadString(EMAIL_TEMPLATE_PATH, &result)))
        LOG_ERROR("can't load email template \"" + EMAIL_TEMPLATE_PATH + "\"!");

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

    std::string json_result, err_msg;
    if (unlikely(not Solr::Query(QUERY, "id,title,author,last_modification_time,container_ids_and_titles", &json_result, &err_msg,
                                 solr_host_and_port, /* timeout = */ 5, Solr::JSON)))
        LOG_ERROR("Solr query failed or timed-out: \"" + QUERY + "\". (" + err_msg + ")");

    return ExtractNewIssueInfos(notified_db, new_notification_ids, json_result, new_issue_infos,
                                max_last_modification_time);
}


void SendNotificationEmail(const bool debug, const std::string &firstname, const std::string &lastname,
                           const std::string &recipient_email, const std::string &vufind_host,
                           const std::string &sender_email, const std::string &email_subject,
                           const std::vector<NewIssueInfo> &new_issue_infos,
                           const std::string &user_type)
{
    std::string email_template = GetEmailTemplate(user_type);

    // Process the email template:
    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("firstname", firstname);
    names_to_values_map.insertScalar("lastname", lastname);
    std::vector<std::string> urls, series_titles, issue_titles;
    std::vector<std::shared_ptr<Template::Value>> authors;
    for (const auto &new_issue_info : new_issue_infos) {
        urls.emplace_back("https://" + vufind_host + "/Record/" + new_issue_info.control_number_);
        series_titles.emplace_back(new_issue_info.series_title_);
        issue_titles.emplace_back(HtmlUtil::HtmlEscape(new_issue_info.issue_title_));
        std::shared_ptr<Template::ArrayValue> issue_authors(new Template::ArrayValue("authors"));
        for (const auto &author : new_issue_info.authors_)
            issue_authors->appendValue(author);
        authors.emplace_back(issue_authors);
    }
    names_to_values_map.insertArray("url", urls);
    names_to_values_map.insertArray("series_title", series_titles);
    names_to_values_map.insertArray("issue_title", issue_titles);
    names_to_values_map.insertArray("authors", authors);
    std::istringstream input(email_template);
    std::ostringstream email_contents;
    Template::ExpandTemplate(input, email_contents, names_to_values_map);

    if (debug)
        std::cerr << "Debug mode, email address is " << sender_email << ", template expanded to:\n" << email_contents.str()
                  << '\n';
    else if (unlikely(not EmailSender::SendEmail(sender_email, recipient_email, email_subject, email_contents.str(),
                                                 EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        LOG_ERROR("failed to send a notification email to \"" + recipient_email + "\"!");
}


void ProcessSingleUser(const bool debug, DbConnection * const db_connection,
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
        LOG_ERROR("found no user attributes in table \"user\" for ID \"" + user_id + "\"!");
    if (result_set.size() > 1)
        LOG_ERROR("found multiple user attribute sets in table \"user\" for ID \"" + user_id + "\"!");

    const DbRow row(result_set.getNextRow());
    const std::string username(row["username"]);

    LOG_INFO("Found " + std::to_string(control_numbers_and_last_modification_times.size()) + " subscriptions for \""
         + username + "\".");

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

    LOG_INFO("Found " + std::to_string(new_issue_infos.size()) + " new issues for " + "\"" + username + "\".");

    if (not new_issue_infos.empty())
        SendNotificationEmail(debug, firstname, lastname, email, hostname, sender_email, email_subject, new_issue_infos,
                              user_type);

    // Update the database with the new last issue dates
    // skip in DEBUG mode
    if (debug)
        return;

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


void ProcessSubscriptions(const bool debug, DbConnection * const db_connection,
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
        ProcessSingleUser(debug, db_connection, notified_db, new_notification_ids, user_id, solr_host_and_port,
                          hostname, sender_email, email_subject, control_numbers_and_last_modification_times);
    }

    LOG_INFO("Processed " + std::to_string(user_count) + " users and " + std::to_string(subscription_count) + " subscriptions.\n");
}


void RecordNewlyNotifiedIds(const std::unique_ptr<kyotocabinet::HashDB> &notified_db,
                            const std::unordered_set<std::string> &new_notification_ids)
{
    const std::string now(TimeUtil::GetCurrentDateAndTime());
    for (const auto &id : new_notification_ids) {
        if (not notified_db->add(id, now))
            LOG_ERROR("Failed to add key/value pair to database \"" + notified_db->path() + "\" ("
                  + std::string(notified_db->error().message()) + ")!");
    }
}


std::unique_ptr<kyotocabinet::HashDB> CreateOrOpenKeyValueDB(const std::string &user_type) {
    const std::string DB_FILENAME("/usr/local/var/lib/tuelib/" + user_type + "_notified.db");
    std::unique_ptr<kyotocabinet::HashDB> db(new kyotocabinet::HashDB());
    if (not (db->open(DB_FILENAME,
                      kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OREADER | kyotocabinet::HashDB::OCREATE)))
        LOG_ERROR("failed to open or create \"" + DB_FILENAME + "\"!");
    return db;
}


} // unnamed namespace


// gets user subscriptions for superior works from mysql
// uses kyotocabinet HashDB (file) to prevent entries from being sent multiple times to same user
int Main(int argc, char **argv) {
    if (argc < 5)
        Usage();

    bool debug(false);
    if (std::strcmp("--debug", argv[1]) == 0) {
        if (argc < 6)
            Usage();
        debug = true;
        --argc, ++argv;
    }

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
        LOG_ERROR("user_type parameter must be either \"ixtheo\" or \"relbib\"!");

    const std::string hostname(argv[2]);
    const std::string sender_email(argv[3]);
    const std::string email_subject(argv[4]);

    std::unique_ptr<kyotocabinet::HashDB> notified_db(CreateOrOpenKeyValueDB(user_type));

    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    std::unordered_set<std::string> new_notification_ids;
    ProcessSubscriptions(debug, &db_connection, notified_db, &new_notification_ids, solr_host_and_port, user_type, hostname,
                         sender_email, email_subject);
    if (not debug)
        RecordNewlyNotifiedIds(notified_db, new_notification_ids);

    return EXIT_SUCCESS;
}
