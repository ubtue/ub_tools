/** \file    new_journal_tool.cc
 *  \brief   Command-line utility to display information about journal subscriptions and to reset entries
 *           in the notified_db for testing purposes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020-2021 Library of the University of Tübingen

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
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "BSZUtil.h"
#include "DbConnection.h"
#include "FileUtil.h"
#include "JSON.h"
#include "KeyValueDB.h"
#include "Solr.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[solr_host_and_port] user_type command command_args\n"
            "possible commands are \"list_users\", \"list_subs\" and \"clear\"\n\n"
            "\"list_users\" takes no arguments\n"
            "\"list_subs\" takes a single command argument which can be either a username or the special taken \"all\"\n"
            "    when \"all\" has been specified the subscription status for all users will be displayed\n"
            "\"clear\" takes one or two command args\n"
            "    when the single argument following \"clear\" is \"all\" the entire database is purged\n"
            "    when a username follows after \"clear\" there is an optional subscription name after the username\n"
            "    if a subscription name has been specified, only that subscription will be purged o/w all the user's\n"
            "    subscriptions will be purged.\n");
}


void ListUsers(DbConnection * const db_connection, const std::string &user_type) {
    const std::string QUERY("SELECT username,firstname,lastname FROM user LEFT JOIN ixtheo_user ON user.id = ixtheo_user.id "
                            "WHERE ixtheo_user.user_type='" + user_type + "'");
    db_connection->queryOrDie(QUERY);
    DbResultSet result_set(db_connection->getLastResultSet());
    size_t username_max_size(0), firstname_max_size(0);
    while (const DbRow row = result_set.getNextRow()) {
        const auto username_size(row["username"].size());
        if (username_size > username_max_size)
            username_max_size = username_size;

        const auto firstname_size(row["firstname"].size());
        if (firstname_size > firstname_max_size)
            firstname_max_size = firstname_size;
    }

    db_connection->queryOrDie(QUERY);
    DbResultSet result_set2(db_connection->getLastResultSet());
    while (const DbRow row = result_set2.getNextRow()) {
        std::string username(row["username"]), firstname(row["firstname"]);
        std::cout << TextUtil::PadTrailing(&username, username_max_size) << " -> " << TextUtil::PadTrailing(&firstname, firstname_max_size)
                  << ' ' << row["lastname"] << '\n';
    }
}


std::string GetSeriesTitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string NO_SERIES_TITLE("*No Series Title*");
    const std::shared_ptr<const JSON::JSONNode> title(doc_obj->getNode("title"));
    if (title == nullptr) {
        LOG_WARNING("\"title\" is null");
        return NO_SERIES_TITLE;
    }

    const std::shared_ptr<const JSON::StringNode> title_node(
        JSON::JSONNode::CastToStringNodeOrDie("title", title));
    if (unlikely(title_node == nullptr))
        LOG_ERROR("title_node is not a JSON string!");

    return title_node->getValue().empty() ? NO_SERIES_TITLE : title_node->getValue();
}


std::string GetTitle(const std::string &ppn, const std::string &solr_host, const unsigned solr_port) {
    const std::string SOLR_QUERY("superior_ppn:" + ppn);
    std::string json_document, err_msg;
    if (unlikely(not Solr::Query(SOLR_QUERY, "title", &json_document, &err_msg, solr_host, solr_port, /* timeout = */ 5,
                                 Solr::JSON, /* max_no_of_rows = */1)))
        LOG_ERROR("Solr query failed or timed-out: \"" + SOLR_QUERY + "\". (" + err_msg + ")");

    JSON::Parser parser(json_document);
    std::shared_ptr<JSON::JSONNode> tree;
    if (not parser.parse(&tree))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ObjectNode> tree_obj(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", tree));
    const std::shared_ptr<const JSON::ObjectNode> response(tree_obj->getObjectNode("response"));
    const std::shared_ptr<const JSON::ArrayNode> docs(response->getArrayNode("docs"));

    if (unlikely(docs->empty()))
        return "*UNKNOWN TITLE*";

    const std::shared_ptr<JSON::JSONNode> doc(*docs->begin());
    const std::shared_ptr<const JSON::ObjectNode> doc_obj(JSON::JSONNode::CastToObjectNodeOrDie("document object", doc));
    return GetSeriesTitle(doc_obj);
}


void ListSubs(DbConnection * const db_connection, const std::string &user_type, const std::string &username,
              const std::string &host, const unsigned port)
{
    std::string query("SELECT username,ixtheo_user.id AS id FROM user LEFT JOIN ixtheo_user ON user.id = ixtheo_user.id "
                      "WHERE ixtheo_user.user_type='" + user_type + "'");
    if (username != "all")
        query += " AND username=" + db_connection->escapeString(username, /* add_quotes = */true);

    db_connection->queryOrDie(query);
    DbResultSet result_set(db_connection->getLastResultSet());

    while (const DbRow row = result_set.getNextRow()) {
        db_connection->queryOrDie("SELECT journal_control_number_or_bundle_name,max_last_modification_time FROM "
                                  "ixtheo_journal_subscriptions WHERE user_id=" + row["id"]);
        DbResultSet result_set2(db_connection->getLastResultSet());
        if (result_set2.empty())
            continue;

        std::cout << row["username"] << ":\n";
        while (const DbRow row2 = result_set2.getNextRow()) {
            const std::string journal_control_number_or_bundle_name(row2["journal_control_number_or_bundle_name"]);
            std::cout << '\t' << StringUtil::PadTrailing(journal_control_number_or_bundle_name, BSZUtil::PPN_LENGTH_NEW)
                      << " -> " << row2["max_last_modification_time"] << ' '
                      << GetTitle(journal_control_number_or_bundle_name, host, port) << '\n';
        }
    }
}


void Clear(DbConnection * const db_connection, KeyValueDB * const notified_db, const std::string &username,
           const std::string &subscription_name)
{
    db_connection->queryOrDie("SELECT ixtheo_user.id AS id FROM user LEFT JOIN ixtheo_user ON user.id = ixtheo_user.id WHERE username="
                              + db_connection->escapeAndQuoteString(username));
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        std::cout << "Username \"" << username << "\" was not found!\n";
        return;
    }
    const std::string user_id(result_set.getNextRow()["id"]);

    if (subscription_name == "all") {
        db_connection->queryOrDie("SELECT journal_control_number_or_bundle_name FROM ixtheo_journal_subscriptions WHERE user_id="
                                  + user_id);
        DbResultSet result_set2(db_connection->getLastResultSet());
        while (const DbRow row = result_set2.getNextRow())
            notified_db->remove(row["journal_control_number_or_bundle_name"]);
        db_connection->queryOrDie("DELETE FROM ixtheo_journal_subscriptions WHERE user_id=" + user_id);
        std::cout << "Deleted " << db_connection->getNoOfAffectedRows() << " subscriptions.\n";
    } else {
        db_connection->queryOrDie("DELETE FROM ixtheo_journal_subscriptions WHERE user_id=" + user_id
                                  + " AND journal_control_number_or_bundle_name=" + db_connection->escapeAndQuoteString(subscription_name));
        const unsigned affected_rows(db_connection->getNoOfAffectedRows());
        if (affected_rows == 0)
            std::cout << "Subscription " << subscription_name << " for user \"" << username << "\" not found!\n";
        else {
            notified_db->remove(subscription_name);
            std::cout << "Subscription " << subscription_name << " has ben successfully deleted!\n";
        }
    }
}


} // unnamed namespace


// gets user subscriptions for superior works from MySQL
// uses a KeyValueDB instance to prevent entries from being sent multiple times to same user
int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    std::string solr_host;
    unsigned solr_port;
    if (std::strchr(argv[1], ':') == nullptr) {
        solr_host = Solr::DEFAULT_HOST;
        solr_port = Solr::DEFAULT_PORT;
    } else {
        std::string solr_port_as_string;
        StringUtil::SplitOnString(std::string(argv[1]), ":", &solr_host, &solr_port_as_string);
        solr_port = StringUtil::ToUnsigned(solr_port_as_string);
        --argc, ++argv;
    }
    if (argc < 3)
        Usage();

    const std::string user_type(argv[1]);
    if (user_type != "ixtheo" and user_type != "relbib")
        LOG_ERROR("user_type parameter must be either \"ixtheo\" or \"relbib\"!");

    const std::string DB_FILENAME(UBTools::GetTuelibPath() + user_type + "_notified.db");
    std::unique_ptr<KeyValueDB> notified_db(OpenKeyValueDBOrDie(DB_FILENAME));

    auto db_connection(DbConnection::VuFindMySQLFactory());

    const std::string command(argv[2]);

    if (command == "list_users") {
        if (argc != 3)
            Usage();
        ListUsers(&db_connection, user_type);
    } else if (command == "list_subs") {
        if (argc != 4)
            Usage();
        const std::string username(argv[3]);
        ListSubs(&db_connection, user_type, username, solr_host, solr_port);
    } else if (command == "clear") {
        if (argc < 4 or argc > 5)
            Usage();
        const std::string username(argv[3]);
        if (username == "all" and argc > 4)
            Usage();
        const std::string subscription_name(argc == 5 ? argv[4] : "all");
        Clear(&db_connection, notified_db.get(), username, subscription_name);
    } else
        Usage();


    return EXIT_SUCCESS;
}
