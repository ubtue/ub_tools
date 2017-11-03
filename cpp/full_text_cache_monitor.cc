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
#include "FileUtil.h"
#include "FullTextCache.h"
#include "HtmlUtil.h"
#include "MiscUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "WebUtil.h"


namespace {


class PageException: public std::exception {
    std::string message_;
public:
    PageException(std::string message): message_(message) { }
    inline const char *what() const throw () { return message_.c_str(); }
};


const std::string template_directory("/usr/local/var/lib/tuelib/full_text_cache_monitor/");
std::multimap<std::string, std::string> cgi_args;


void ExpandTemplate(const std::string &template_name,
                    std::string * const body,
                    const std::map<std::string, std::vector<std::string>> &template_variables = {})
{
    std::ifstream template_html(template_directory + template_name + ".html", std::ios::binary);
    std::ostringstream template_out;
    MiscUtil::ExpandTemplate(template_html, template_out, template_variables);
    *body += template_out.str();
}


std::string GetCGIParameterOrDefault(const std::string &parameter_name,
                                     const std::string &default_value = "")
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


void ShowError(const std::string &error_message, std::string * const body) {
    *body += "<h1 class=\"error\">Error</h1>";
    *body += "<h4 class=\"error\">" + error_message + "</h4>";
}


void ShowPageHeader(FullTextCache * const cache, std::string * const body) {
    unsigned cache_size = cache->getSize();
    unsigned error_count = cache->getErrorCount();
    std::string error_rate_string("-");
    if (cache_size > 0) {
        float error_rate(static_cast<float>(error_count) / cache_size);
        error_rate_string = StringUtil::ToString(error_rate);
    }

    std::map<std::string, std::vector<std::string>> template_variables;
    std::string id(GetCGIParameterOrDefault("id"));
    template_variables.emplace("cache_size", std::vector<std::string> { StringUtil::ToString(cache_size) });
    template_variables.emplace("error_count", std::vector<std::string> { StringUtil::ToString(error_count) });
    template_variables.emplace("error_rate", std::vector<std::string> { error_rate_string });
    template_variables.emplace("id", std::vector<std::string> {id});
    ExpandTemplate("header", body, template_variables);
}


void ShowPageIdDetails(FullTextCache * const cache, std::string * const body) {
    std::string id(GetCGIParameterOrDefault("id"));
    if (id.empty())
        throw PageException("parameter missing: no ID given");

    FullTextCache::Entry entry;
    if (not cache->getEntry(id, &entry))
        throw PageException("ID not found: " + id);

    std::map<std::string, std::vector<std::string>> template_variables;
    template_variables.emplace("id", std::vector<std::string> {HtmlUtil::HtmlEscape(id)});
    template_variables.emplace("expiration", std::vector<std::string> {HtmlUtil::HtmlEscape(SqlUtil::TimeTToDatetime(entry.expiration_))});
    template_variables.emplace("link_sobek", std::vector<std::string> {"<a href=\"https://sobek.ub.uni-tuebingen.de/Record/" + UrlUtil::UrlEncode(id) + "\" target=\"sobek\">test (sobek)</a>"});
    template_variables.emplace("link_ub15", std::vector<std::string> {"<a href=\"https://krimdok.uni-tuebingen.de/Record/" + UrlUtil::UrlEncode(id) + "\" target=\"ub15\">live (ub15)</a>"});

    std::vector<std::string> urls;
    std::vector<std::string> domains;
    std::vector<std::string> error_messages;
    std::vector<FullTextCache::EntryUrl> entry_urls = cache->getEntryUrls(id);
    for (const auto &entry_url : entry_urls) {
        urls.emplace_back("<a href=\"" + entry_url.url_ + "\">" + entry_url.url_ + "</a>");
        domains.emplace_back("<a href=\"http://" + entry_url.domain_ + "\">" + entry_url.domain_ + "</a>");
        error_messages.emplace_back(HtmlUtil::HtmlEscape(entry_url.error_message_));
    }
    template_variables.emplace("url", urls);
    template_variables.emplace("domain", domains);
    template_variables.emplace("error_message", error_messages);

    std::string fulltext;
    if (not cache->getFullText(id, &fulltext))
        fulltext = "-";
    template_variables.emplace("fulltext", std::vector<std::string> {HtmlUtil::HtmlEscape(fulltext)});

    ExpandTemplate("id_details", body, template_variables);
}


void ShowPageErrorSummary(FullTextCache * const cache, std::string * const body) {
    std::vector<std::string> error_messages, counts, domains, urls, ids, links_details, links_error_details;
    std::vector<FullTextCache::EntryGroup> groups = cache->getEntryGroupsByDomainAndErrorMessage();
    for (auto const &group : groups) {
        counts.emplace_back(std::to_string(group.count_));
        domains.emplace_back("<a href=\"http://" + group.domain_ + "\">" + group.domain_ + "</a>");
        error_messages.emplace_back(group.error_message_);
        ids.emplace_back(group.example_entry_.id_);
        urls.emplace_back("<a href=\"" + group.example_entry_.url_ + "\">" + group.example_entry_.url_ + "</a>");
        links_details.emplace_back("<a href=\"?page=id_details&id=" + UrlUtil::UrlEncode(group.example_entry_.id_) + "\">" + HtmlUtil::HtmlEscape(group.example_entry_.id_) + "</a>");
        links_error_details.emplace_back("<a href=\"?page=error_list&domain=" + UrlUtil::UrlEncode(group.domain_) + "&error_message=" + UrlUtil::UrlEncode(group.error_message_) + "\">Show error list</a>");
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


void ShowPageErrorList(FullTextCache * const cache, std::string * const body) {
    std::string error_message(GetCGIParameterOrDefault("error_message"));
    std::string domain(GetCGIParameterOrDefault("domain"));

    std::vector<std::string> ids;
    std::vector<std::string> urls;
    std::vector<std::string> error_messages;
    std::vector<FullTextCache::JoinedEntry> entries = cache->getJoinedEntriesByDomainAndErrorMessage(domain, error_message);
    for (const auto &entry : entries) {
        ids.emplace_back("<a href=\"?page=id_details&id=" + UrlUtil::UrlEncode(entry.id_) + "\">" + HtmlUtil::HtmlEscape(entry.id_) + "</a>");
        urls.emplace_back("<a href=\"" + entry.url_ + "\">" + entry.url_ + "</a>");
        error_messages.emplace_back(HtmlUtil::HtmlEscape(error_message));
    }

    std::map<std::string, std::vector<std::string>> template_variables;
    template_variables.emplace("domain", std::vector<std::string> { "<a href=\"http://" + domain + "\">" + domain + "</a>" });
    template_variables.emplace("error_message", std::vector<std::string> { HtmlUtil::HtmlEscape(error_message) });
    template_variables.emplace("id", ids);
    template_variables.emplace("url", urls);
    ExpandTemplate("error_list", body, template_variables);
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    try {
        FullTextCache cache;

        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
        const std::string subpage(GetCGIParameterOrDefault("page"));

        std::string body;
        ShowPageHeader(&cache, &body);

        try {
            if (subpage == "id_details") {
                ShowPageIdDetails(&cache, &body);
            } else if (subpage == "error_summary") {
                ShowPageErrorSummary(&cache, &body);
            } else if (subpage == "error_list") {
                ShowPageErrorList(&cache, &body);
            } else if (subpage != "") {
                throw PageException("Page does not exist: " + subpage);
            }
        } catch (PageException &e) {
            ShowError(e.what(), &body);
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
