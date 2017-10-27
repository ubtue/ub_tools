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
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "VuFind.h"
#include "WebUtil.h"


class PageException: public std::exception
{
public:
    PageException(std::string message) : message_(message) { }
    const char * what () const throw ()
    {
        return message_.c_str();
    }
private:
    std::string message_;
};


const std::string template_directory("/usr/local/var/lib/tuelib/full_text_cache_monitor/");
std::multimap<std::string, std::string> cgi_args;


void ExpandTemplate(const std::string &template_name,
                    std::string &body,
                    const std::map<std::string, std::vector<std::string>> &template_variables = {})
{
    std::ifstream template_html(template_directory + template_name + ".html", std::ios::binary);
    std::ostringstream template_out;
    MiscUtil::ExpandTemplate(template_html, template_out, template_variables);
    body += template_out.str();
}


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


void ShowError(const std::string &error_message, std::string &body) {
    body += "<h1 style=\"color: red;\">Error</h1>";
    body += "<h4 style=\"color: red;\">" + error_message + "</h4>";
}


void ShowPageHeader(std::string &body) {
    std::map<std::string, std::vector<std::string>> template_variables;
    std::string id(GetCGIParameterOrDefault("id"));
    template_variables.emplace("id", std::vector<std::string> {id});
    ExpandTemplate("header", body, template_variables);
}


void ShowPageIdDetails(DbConnection * const db_connection, std::string &body) {
    std::string id(GetCGIParameterOrDefault("id"));
    if (id.empty())
        throw PageException("parameter missing: no ID given");


    FullTextCache::Entry entry;
    if (not FullTextCache::GetEntry(db_connection, id, entry))
        throw PageException("ID not found: " + id);

    std::map<std::string, std::vector<std::string>> template_variables;
    template_variables.emplace("id", std::vector<std::string> {HtmlUtil::HtmlEscape(id)});
    template_variables.emplace("status", std::vector<std::string> {HtmlUtil::HtmlEscape(entry.status_)});
    template_variables.emplace("expiration", std::vector<std::string> {HtmlUtil::HtmlEscape(entry.expiration_)});
    template_variables.emplace("url", std::vector<std::string> {HtmlUtil::HtmlEscape(entry.url_)});
    template_variables.emplace("error_message", std::vector<std::string> {HtmlUtil::HtmlEscape(entry.error_message_)});
    template_variables.emplace("link_sobek", std::vector<std::string> {"<a href=\"https://sobek.ub.uni-tuebingen.de/Record/" + UrlUtil::UrlEncode(id) + "\">test (sobek)</a>"});
    template_variables.emplace("link_ub15", std::vector<std::string> {"<a href=\"https://krimdok.uni-tuebingen.de/Record/" + UrlUtil::UrlEncode(id) + "\">live (ub15)</a>"});
    ExpandTemplate("id_details", body, template_variables);
}


void ShowPageErrorSummary(DbConnection * const db_connection, std::string &body) {
    std::vector<std::string> error_messages, counts, domains, urls, ids, links_details, links_error_details;
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
        links_details.emplace_back("<a href=\"?page=id_details&id=" + UrlUtil::UrlEncode(ids[i]) + "\">" + HtmlUtil::HtmlEscape(ids[i]) + "</a>");
        links_error_details.emplace_back("<a href=\"?page=error_list&domain=" + UrlUtil::UrlEncode(domains[i]) + "&error_message=" + UrlUtil::UrlEncode(error_messages[i]) + "\">Show error list</a>");
    }

    std::map<std::string, std::vector<std::string>> template_variables;
    template_variables.emplace("id", ids);
    template_variables.emplace("url", urls);
    template_variables.emplace("error_message", error_messages);
    template_variables.emplace("domain", domains);
    template_variables.emplace("count", counts);
    template_variables.emplace("url", urls);
    template_variables.emplace("link_details", links_details);
    template_variables.emplace("link_error_details", links_error_details);
    ExpandTemplate("error_summary", body, template_variables);
}


void ShowPageErrorList(DbConnection * const db_connection, std::string &body) {

    std::string error_message(GetCGIParameterOrDefault("error_message"));
    std::string domain(GetCGIParameterOrDefault("domain"));

    db_connection->queryOrDie("SELECT id, url "
                              "FROM full_text_cache "
                              "WHERE error_message='" + db_connection->escapeString(error_message) + "'"
                              "AND SUBSTRING(LEFT(url, LOCATE('/', url, 8) - 1), 8) = '" + db_connection->escapeString(domain) + "'"
                              );

    std::vector<std::string> ids, urls, error_messages;
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const DbRow row = result_set.getNextRow()) {
        ids.emplace_back("<a href=\"?page=id_details&id=" + UrlUtil::UrlEncode(row["id"]) + "\">" + HtmlUtil::HtmlEscape(row["id"]) + "</a>");
        urls.emplace_back(HtmlUtil::HtmlEscape(row["url"]));
        error_messages.emplace_back(HtmlUtil::HtmlEscape(error_message));
    }

    std::map<std::string, std::vector<std::string>> template_variables;
    template_variables.emplace("id", ids);
    template_variables.emplace("url", urls);
    template_variables.emplace("error_message", error_messages);
    ExpandTemplate("error_list", body, template_variables);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    try {
        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
        const std::string subpage(GetCGIParameterOrDefault("page"));

        std::string body;
        ShowPageHeader(body);

        try {
            if (subpage == "id_details") {
                ShowPageIdDetails(&db_connection, body);
            } else if (subpage == "error_summary") {
                ShowPageErrorSummary(&db_connection, body);
            } else if (subpage == "error_list") {
                ShowPageErrorList(&db_connection, body);
            } else if (subpage != "") {
                throw PageException("Page does not exist: " + subpage);
            }
        } catch (PageException &e) {
            ShowError(e.what(), body);
        }

        std::map<std::string, std::vector<std::string>> names_to_values_map;

        std::string css;
        FileUtil::ReadString(template_directory + "style.css", &css);
        names_to_values_map.emplace("css", std::vector<std::string> {css});

        std::ifstream template_html(template_directory + "index.html", std::ios::binary);
        names_to_values_map.emplace("body", std::vector<std::string> {body});

        MiscUtil::ExpandTemplate(template_html, std::cout, names_to_values_map);
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
