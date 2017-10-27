/** \file    full_text_cache_monitor.cc
 *  \brief   A CGI-tool for displaying the fulltext cache contents
 *  \author  Mario Trojan
 */
/*
    Copyright (C) 2016,2017, Library of the University of Tübingen

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
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "FullTextCache.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "VuFind.h"
#include "WebUtil.h"

std::multimap<std::string, std::string> cgi_args;


std::string GetCGIParameterOrDefault(const std::string &parameter_name,
                                     const std::string &default_value = "")
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


void GetExampleData(DbConnection * const db_connection,
                    const std::string &error_message, const std::string &domain,
                    std::string &example_id, std::string &example_url)
{
    db_connection->queryOrDie("SELECT id, url "
                              "FROM full_text_cache "
                              "WHERE error_message='" + db_connection->escapeString(error_message) + "' "
                              "AND SUBSTRING(LEFT(url, LOCATE('/', url, 8) - 1), 8)='" + db_connection->escapeString(domain) + "' "
                              "LIMIT 1 ");

    DbResultSet result_set(db_connection->getLastResultSet());
    const DbRow row(result_set.getNextRow());
    example_id = row["id"];
    example_url = row["url"];
}


void ShowError(const std::string &error_message, const std::string &description = "") {
    std::cout << "  <h1>" + error_message + "</h1>"
              << "  <h3>" + description + "</h3>";

    throw std::runtime_error("error_message");
}


void ShowPageHeader() {
    std::cout << "<h1>Fulltext Cache Monitor</h1>";
    std::cout << "<p>What do you want to do?</p>";
    std::cout << "<ul>";
    std::cout << "<li><a href=\"?page=errors\">Show errors</a></li>";
    std::cout << "<li><form method=\"get\">Search for ID</a><input name=\"id\"></input><input type=\"hidden\" name=\"page\" value=\"details\"></input><input type=\"submit\"></input></form></li>";
    std::cout << "</ul>";
}


void ShowPageDetails(DbConnection * const db_connection) {
    std::string id(GetCGIParameterOrDefault("id"));
    if (id.empty())
        ShowError("error: parameter missing", "no ID given!");

    FullTextCache::Entry entry;
    if (not FullTextCache::GetEntry(db_connection, id, entry))
        ShowError("error: ID not found", id);

    std::cout << "<h1>Details for ID " + id + ":</h1>";
    std::cout << "<table>";
    std::cout << "<tr><td>Status:</td><td>" << entry.status_ << "</td></tr>";
    std::cout << "<tr><td>Expiration:</td><td>" << entry.expiration_ << "</td></tr>";
    std::cout << "<tr><td>URL:</td><td>" << entry.url_ << "</td></tr>";
    std::cout << "<tr><td>Error Message:</td><td>" << entry.error_message_ << "</td></tr>";
    std::cout << "</table>";
}


void ShowPageErrors(DbConnection * const db_connection) {
    std::vector<std::string> error_messages, counts, domains, urls, ids;
    {
        db_connection->queryOrDie("SELECT error_message, count(*) AS count, "
                                  "SUBSTRING(LEFT(url, LOCATE('/', url, 8) - 1), 8) AS domain "
                                  "FROM full_text_cache "
                                  "WHERE error_message IS NOT NULL "
                                  "GROUP BY error_message, domain "
                                  "ORDER BY count DESC ");

        DbResultSet result_set(db_connection->getLastResultSet());
        while (const DbRow row = result_set.getNextRow()) {
            error_messages.emplace_back(row["error_message"]);
            counts.emplace_back(row["count"]);
            domains.emplace_back(row["domain"]);
        }
    }

    for (unsigned i(0); i < error_messages.size(); ++i) {
        std::string domain(domains[i]);

        std::string example_id;
        std::string example_url;
        GetExampleData(db_connection, error_messages[i], domains[i],
                       example_id, example_url);

        ids.emplace_back(example_id);
        urls.emplace_back(example_url);
    }


    std::cout << "<h1>Error overview:</h1>";
    std::cout << "<table id=\"errors\">";

    std::cout << "<tr>";
    std::cout << "<th align=\"left\">Count:</th>";
    std::cout << "<th align=\"left\">Error Message:</th>";
    std::cout << "<th align=\"left\">Domain:</th>";
    std::cout << "<th align=\"left\">Example URL:</th>";
    std::cout << "<th align=\"left\">Example ID:</th>";
    std::cout << "</tr>";
    for (unsigned i(0); i < error_messages.size(); ++i) {
        std::cout << "<tr>";

        std::cout << "<td>" << counts[i] << "</td>";
        std::cout << "<td>" << error_messages[i] << "</td>";
        std::cout << "<td>" << domains[i] << "</td>";
        std::cout << "<td>" << urls[i] << "</td>";
        std::cout << "<td>" << "<a href=\"?page=details&id=" + ids[i] + "\">" + ids[i] + "</a>" << "</td>";

        std::cout << "</tr>";
    }

    std::cout << "</table>";


}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

        std::cout << "<!DOCTYPE html><html><head>"
                  << "<style type=\"text/css\">"
                  << "table#errors td { border: 1px solid #CCCCCC;}"
                  << "</style>"
                  << "<title>Fulltext Monitor</title></head>"
                  << "<body>";

        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
        const std::string subpage(GetCGIParameterOrDefault("page"));

        ShowPageHeader();

        if (subpage == "details") {
            ShowPageDetails(&db_connection);
        } else if (subpage == "errors") {
            ShowPageErrors(&db_connection);
        }

        std::cout << "</body>"
                  << "</html>";
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
