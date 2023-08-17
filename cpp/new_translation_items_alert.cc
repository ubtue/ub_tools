/** \file     new_translation_items_alert
 *  \brief    A tool for informing translators about new imported terms
 *
 *
    Copyright (C) 2023, Library of the University of Tübingen

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

#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "EmailSender.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "Template.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"


namespace {


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");
const std::string NEW_ITEM_NOTIFICATION_SECTION("NewItemNotifications");
const std::string TRANSLATION_LANGUAGES_SECTION("TranslationLanguages");
const std::string EMAIL_SECTION("Email");

[[noreturn]] void Usage() {
    ::Usage(
        "[--debug]\n"
        "Debug suppresses sending of Emails and updating of the last_notified_timestamp");
}

DbResultSet ExecSqlAndReturnResultsOrDie(const std::string &select_statement, DbConnection * const db_connection) {
    db_connection->queryOrDie(select_statement);
    return db_connection->getLastResultSet();
}


bool GetUsers(const IniFile &ini_file, std::set<std::string> * const users) {
    // Users to be notified are administrators and ordinary users
    std::string administrators;
    if (ini_file.lookup("Users", "administrators", &administrators)) {
        StringUtil::SplitThenTrimWhite(administrators, ',', users);
    }

    // Ordinary users
    auto ordinary_users(ini_file.getSectionEntryNames(TRANSLATION_LANGUAGES_SECTION));
    std::copy_if(ordinary_users.begin(), ordinary_users.end(), std::inserter(*users, users->end()),
                 [](std::string &user) { return not user.empty(); });
    return not users->empty();
}


std::string GetAbsoluteTimeForInterval(DbConnection &db_connection, const std::string interval) {
    if (interval.empty())
        LOG_ERROR("Empty interval");

    const char unit(interval[interval.length() - 1]);
    std::string unit_full;
    switch (unit) {
    case 'd':
        unit_full = "DAY";
        break;
    case 'w':
        unit_full = "WEEK";
        break;
    case 'm':
        unit_full = "MONTH";
        break;
    default:
        LOG_ERROR("Invalid unit " + std::to_string(unit) + " interval " + interval);
    }
    const std::string num_value(interval.substr(0, interval.length() - 1));
    const std::string query("SELECT DATE_SUB(NOW(), INTERVAL " + num_value + " " + unit_full + ") AS time");
    DbResultSet result(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    return result.getNextRow()["time"];
}


std::string GetLastNotified(DbConnection &db_connection, const std::string &user) {
    const std::string last_notified_query("SELECT last_notified FROM translators WHERE translator='" + user + "'");
    DbResultSet last_notified_result(ExecSqlAndReturnResultsOrDie(last_notified_query, &db_connection));
    return last_notified_result.getNextRow()["last_notified"];
}


std::string GetCurrentDBTimestamp(DbConnection &db_connection) {
    std::string now_query("SELECT NOW() AS NOW");
    DbResultSet now_result(ExecSqlAndReturnResultsOrDie(now_query, &db_connection));
    return now_result.getNextRow()["NOW"];
}


void GetNewItems(DbConnection &db_connection, const std::string last_notified, Template::Map * const names_to_values_map) {
    names_to_values_map->clear();

    std::string vufind_new_items_query("SELECT token FROM vufind_translations WHERE create_timestamp>='" + last_notified + "'"
                                       + " AND language_code='eng'");
    DbResultSet vufind_new_result_set(ExecSqlAndReturnResultsOrDie(vufind_new_items_query, &db_connection));

    std::vector<std::string> vufind_new_items;
    while (const auto &db_row = vufind_new_result_set.getNextRow())
        vufind_new_items.emplace_back(db_row["token"]);
    names_to_values_map->insertArray("vufind_new_items", vufind_new_items);


    std::string keywords_new_items_query("SELECT translation FROM keyword_translations WHERE create_timestamp>='" + last_notified
                                         + +"' AND language_code='ger' AND prev_version_id IS NULL");
    DbResultSet keywords_new_result_set(ExecSqlAndReturnResultsOrDie(keywords_new_items_query, &db_connection));

    std::vector<std::string> keywords_new_items;
    while (const auto db_row = keywords_new_result_set.getNextRow())
        keywords_new_items.emplace_back(db_row["translation"]);
    names_to_values_map->insertArray("keywords_new_items", keywords_new_items);
    names_to_values_map->insertScalar("last_notified", last_notified.substr(0, __builtin_strlen("0000-00-00")));
}


void MailNewItems(const std::string &user, const IniFile &ini_file, const Template::Map &names_to_values_map, const bool debug = false) {
    std::stringstream mail_content;
    std::ifstream new_translation_items_template(UBTools::GetTuelibPath() + "translate_chainer/new_translation_items_alert.msg");
    Template::ExpandTemplate(new_translation_items_template, mail_content, names_to_values_map);
    const std::string recipient(ini_file.getString(EMAIL_SECTION, user, ""));
    if (recipient.empty())
        LOG_ERROR("Could not determine Email address for user \"" + user + "\" section \"" + EMAIL_SECTION + "\" in Ini file \""
                  + ini_file.getFilename());
    if (debug) {
        std::cerr << "CONTENT:" << mail_content.str();
        return;
    }

    if (unlikely(not EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", { recipient }, "New Translation Items",
                                                   mail_content.str(), EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        LOG_ERROR("Could not send mail");
}

std::string GetNotifyInterval(const IniFile &ini_file, const std::string &user) {
    std::string notify_interval;
    ini_file.lookup(NEW_ITEM_NOTIFICATION_SECTION, user, &notify_interval);
    return notify_interval;
}


std::string GetNotifyThreshold(const IniFile &ini_file, DbConnection &db_connection, const std::string &user) {
    const std::string notify_interval(GetNotifyInterval(ini_file, user));
    if (notify_interval.empty())
        return "";
    return GetAbsoluteTimeForInterval(db_connection, notify_interval);
}


bool NotifyTimeExceeded(DbConnection &db_connection, const std::string &last_notified, const std::string notify_threshold) {
    const std::string query("SELECT DATEDIFF('" + notify_threshold + "','" + last_notified + "') <= 0 AS notify_time_exceeded");
    DbResultSet result(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    return (bool)std::atoi(result.getNextRow()["notify_time_exceeded"].c_str());
}


void UpdateLastNotifiedTo(DbConnection * const db_connection, const std::string &user, const std::string new_last_notified,
                          const bool debug = false) {
    if (debug)
        return;

    const std::string update_statement("UPDATE translators SET last_notified ='" + new_last_notified + "' WHERE translator='" + user + "'");
    db_connection->queryOrDie(update_statement);
}


void NotifyTranslators(const IniFile &ini_file, DbConnection &db_connection, const bool debug = false) {
    if (ini_file.getSection(NEW_ITEM_NOTIFICATION_SECTION) == ini_file.end())
        LOG_ERROR("No section \"" + NEW_ITEM_NOTIFICATION_SECTION + "\" present in " + CONF_FILE_PATH);

    std::set<std::string> users;
    GetUsers(ini_file, &users);
    for (const std::string &user : users) {
        const std::string notify_interval(GetNotifyInterval(ini_file, user));
        if (notify_interval.empty())
            continue;

        std::string last_notified(GetLastNotified(db_connection, user));
        // If there is no value for last notified yet, use our interval as initial starting point for the range
        if (last_notified.empty())
            last_notified = GetAbsoluteTimeForInterval(db_connection, notify_interval);

        if (not NotifyTimeExceeded(db_connection, last_notified, GetNotifyThreshold(ini_file, db_connection, user)))
            continue;
        Template::Map names_to_values_map;
        // Hold a time slightly before the actual queries were sent
        const std::string query_time_lower_bound(GetCurrentDBTimestamp(db_connection));
        GetNewItems(db_connection, last_notified, &names_to_values_map);
        MailNewItems(user, ini_file, names_to_values_map, debug);
        UpdateLastNotifiedTo(&db_connection, user, query_time_lower_bound, debug);
    }
}


} // end unnamed namespace


int Main(int argc, char **argv) {
    bool debug(false);
    if (argc > 2)
        Usage();
    if (argc == 2 and std::strcmp("--debug", argv[1]) == 0)
        debug = true;

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));
    NotifyTranslators(ini_file, db_connection, debug);
    return EXIT_SUCCESS;
}
