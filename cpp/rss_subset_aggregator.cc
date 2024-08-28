/** \file   rss_subset_aggregator.cc
 *  \brief  Aggregates RSS feeds.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "MiscUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "Template.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "VuFind.h"
#include "XmlWriter.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "--mode=(email|rss_xml) (user_id|sender_email) subsystem_type\n"
        "If the mode is \"rss_xml\" a VuFind user_id needs to be specified, o/w an error email address should be provided.");
}


struct HarvestedRSSItem {
    SyndicationFormat::Item item_;
    std::string feed_title_;
    std::string website_url_;

    HarvestedRSSItem(const SyndicationFormat::Item item, const std::string &feed_title, const std::string &website_url)
        : item_(item), feed_title_(feed_title), website_url_(website_url) { }
};


struct ChannelDesc {
    std::string title_;
    std::string link_;

public:
    ChannelDesc(const std::string &title, const std::string &link): title_(title), link_(link) { }
};


const std::map<std::string, ChannelDesc> subsystem_type_to_channel_desc_map = {
    { "relbib", ChannelDesc("RelBib RSS Aggregator", "https://relbib.de/") },
    { "ixtheo", ChannelDesc("IxTheo RSS Aggregator", "https://ixtheo.de/") },
    { "krimdok", ChannelDesc("KrimDok RSS Aggregator", "https://krimdok.uni-tuebingen.de/") },
};


std::string GetChannelDescEntry(const std::string &subsystem_type, const std::string &entry) {
    const auto subsystem_type_and_channel_desc(subsystem_type_to_channel_desc_map.find(subsystem_type));
    if (subsystem_type_and_channel_desc == subsystem_type_to_channel_desc_map.cend())
        LOG_ERROR("unknown subsystem type \"" + subsystem_type + "\"!");

    if (entry == "title")
        return subsystem_type_and_channel_desc->second.title_;
    if (entry == "link")
        return subsystem_type_and_channel_desc->second.link_;
    LOG_ERROR("unknown entry name \"" + entry + "\"!");
}


void WriteRSSFeedXMLOutput(const std::string &subsystem_type, const std::vector<HarvestedRSSItem> &harvested_items,
                           XmlWriter * const xml_writer) {
    xml_writer->openTag("rss", { { "version", "2.0" } });
    xml_writer->openTag("channel");
    xml_writer->writeTagsWithData("title", GetChannelDescEntry(subsystem_type, "title"));
    xml_writer->writeTagsWithData("link", GetChannelDescEntry(subsystem_type, "link"));
    xml_writer->writeTagsWithData("description", "RSS Aggregator");

    for (const auto &harvested_item : harvested_items) {
        xml_writer->openTag("item");

        const auto title(harvested_item.item_.getTitle());
        if (not title.empty())
            xml_writer->writeTagsWithData("title", harvested_item.item_.getTitle());

        xml_writer->writeTagsWithData("link", harvested_item.item_.getLink());

        const auto description(HtmlUtil::ShortenText(harvested_item.item_.getDescription(), /*max_length = */ 500));
        if (not description.empty())
            xml_writer->writeTagsWithData("description", description);

        xml_writer->writeTagsWithData("pubDate",
                                      TimeUtil::TimeTToString(harvested_item.item_.getPubDate(), TimeUtil::RFC822_FORMAT, TimeUtil::UTC));
        xml_writer->writeTagsWithData("guid", harvested_item.item_.getId());
        xml_writer->closeTag("item", /* suppress_indent */ false);
    }

    xml_writer->closeTag("channel");
    xml_writer->closeTag("rss");
}


struct FeedNameAndURL {
    std::string name_;
    std::string url_;

public:
    FeedNameAndURL() = default;
    FeedNameAndURL(const FeedNameAndURL &other) = default;
    FeedNameAndURL(const std::string &name, const std::string &url): name_(name), url_(url) { }
};


// \return True if a notification email was sent successfully, o/w false.
bool SendEmail(const std::string &subsystem_type, const std::string &email_sender, const std::string &user_email,
               const std::string &user_address, const std::string &language, const std::vector<HarvestedRSSItem> &harvested_items) {
    const auto template_filename_prefix(UBTools::GetTuelibPath() + "rss_email.template");
    std::string template_filename(template_filename_prefix + "." + language);
    if (not FileUtil::Exists(template_filename))
        template_filename = template_filename_prefix + ".en";
    const std::string email_template(FileUtil::ReadStringOrDie(template_filename));


    std::string list("<ul>\n");
    std::string previous_feed_title;
    for (const auto &harvested_item : harvested_items) {
        const bool new_feed(previous_feed_title != harvested_item.feed_title_);
        if (new_feed) {
            if (not previous_feed_title.empty()) { // not before the first feed
                list += "\t</ul>\n";               // end feed item list
            }
            list +=
                "\t<li><a href=\"" + harvested_item.website_url_ + "\">" + HtmlUtil::HtmlEscape(harvested_item.feed_title_) + "</a></li>\n";
            list += "\t<ul>\n"; // begin feed item list
        }

        list += "\t\t<li><a href=\"" + harvested_item.item_.getLink() + "\">" + HtmlUtil::HtmlEscape(harvested_item.item_.getTitle())
                + "</a></li>\n";
        previous_feed_title = harvested_item.feed_title_;
    }
    list += "\t</ul>\n"; // end feed item list
    list += "</ul>\n";   // end whole list

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("user_name", user_address);
    names_to_values_map.insertScalar("list", list);
    names_to_values_map.insertScalar("system", VuFind::CapitalizedUserType(subsystem_type));
    names_to_values_map.insertScalar("email_reply_to", subsystem_type + "@ub.uni-tuebingen.de");

    const auto email_body(Template::ExpandTemplate(email_template, names_to_values_map));
    const auto retcode(EmailSender::SimplerSendEmail(email_sender, { user_email }, GetChannelDescEntry(subsystem_type, "title"), email_body,
                                                     EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML));
    if (retcode <= 299)
        return true;

    LOG_WARNING("EmailSender::SimplerSendEmail returned " + std::to_string(retcode) + " while trying to send to \"" + user_email + "\"!");
    return false;
}


const unsigned DEFAULT_XML_INDENT_AMOUNT(2);


void GenerateFeed(const std::string &subsystem_type, const std::vector<HarvestedRSSItem> &harvested_items) {
    XmlWriter xml_writer(FileUtil::OpenOutputFileOrDie("/dev/stdout").release(), XmlWriter::WriteTheXmlDeclaration,
                         DEFAULT_XML_INDENT_AMOUNT);
    WriteRSSFeedXMLOutput(subsystem_type, harvested_items, &xml_writer);
}


bool ProcessFeeds(const std::string &user_id, const std::string &rss_feed_last_notification, const std::string &email_sender,
                  const std::string &user_email, const std::string &user_address, const std::string &language, const bool send_email,
                  const std::string &subsystem_type, DbConnection * const db_connection) {
    db_connection->queryOrDie(
        "SELECT rss_feeds_id, feed_name, website_url "
        "FROM tuefind_rss_subscriptions "
        "LEFT JOIN tuefind_rss_feeds ON tuefind_rss_subscriptions.rss_feeds_id = tuefind_rss_feeds.id "
        "WHERE tuefind_rss_subscriptions.user_id=" + user_id + " "
        "ORDER BY feed_name ASC "
    );
    auto feeds_result_set(db_connection->getLastResultSet());
    std::vector<HarvestedRSSItem> harvested_items;
    std::string max_insertion_time;

    while (const auto feed_row = feeds_result_set.getNextRow()) {
        const auto feed_id(feed_row["rss_feeds_id"]);
        const auto feed_name(feed_row["feed_name"]);
        const auto website_url(feed_row["website_url"]);

        std::string query(
            "SELECT item_title,item_description,item_url,item_id,pub_date,insertion_time FROM "
            "tuefind_rss_items WHERE rss_feeds_id="
            + feed_id);
        if (send_email)
            query += " AND insertion_time > '" + rss_feed_last_notification + "' ";
        query += " ORDER BY pub_date ASC";
        db_connection->queryOrDie(query);

        auto items_result_set(db_connection->getLastResultSet());
        while (const auto item_row = items_result_set.getNextRow()) {
            harvested_items.emplace_back(SyndicationFormat::Item(item_row["item_title"], item_row["item_description"], item_row["item_url"],
                                                                 item_row["item_id"], SqlUtil::DatetimeToTimeT(item_row["pub_date"])),
                                         feed_name, website_url);
            const auto insertion_time(item_row["insertion_time"]);
            if (insertion_time > max_insertion_time)
                max_insertion_time = insertion_time;
        }
    }

    if (send_email) {
        if (harvested_items.empty())
            return false;
        if (not SendEmail(subsystem_type, email_sender, user_email, user_address, language, harvested_items))
            return true;
        db_connection->queryOrDie("UPDATE user SET tuefind_rss_feed_last_notification='" + max_insertion_time + "' WHERE id=" + user_id);
    } else
        GenerateFeed(subsystem_type, harvested_items);

    return true;
}


} // unnamed namespace


struct UserInfo {
    std::string user_id_;
    std::string first_name_;
    std::string last_name_;
    std::string email_;
    std::string rss_feed_last_notification_;
    std::string language_code_;

public:
    UserInfo() = default;
    UserInfo(const std::string &user_id, const std::string &first_name, const std::string &last_name, const std::string &email,
             const std::string &rss_feed_last_notification, const std::string &language_code)
        : user_id_(user_id), first_name_(first_name), last_name_(last_name), email_(email),
          rss_feed_last_notification_(rss_feed_last_notification), language_code_(language_code) { }
};


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    std::string sender_email, vufind_user_id;
    if (std::strcmp(argv[1], "--mode=email") == 0)
        sender_email = argv[2];
    else if (std::strcmp(argv[1], "--mode=rss_xml") == 0)
        vufind_user_id = argv[2];
    else
        Usage();
    const std::string subsystem_type(argv[3]);
    if (subsystem_type != "ixtheo" and subsystem_type != "relbib" and subsystem_type != "krimdok")
        LOG_ERROR("subsystem_type must be one of {ixtheo,relbib,krimdok}!");

    auto db_connection(DbConnection::VuFindMySQLFactory());

    std::string sql_query(
        "SELECT id,firstname,lastname,email,tuefind_rss_feed_send_emails"
        ",tuefind_rss_feed_last_notification,last_language FROM user");
    if (vufind_user_id.empty())
        sql_query += " WHERE tuefind_rss_feed_send_emails IS TRUE";
    else
        sql_query += " WHERE id=" + db_connection.escapeAndQuoteString(vufind_user_id);
    sql_query += " AND ixtheo_user_type='" + subsystem_type + "'";
    db_connection.queryOrDie(sql_query);

    auto user_result_set(db_connection.getLastResultSet());
    std::unordered_map<std::string, UserInfo> ids_to_user_infos_map;
    while (const auto user_row = user_result_set.getNextRow()) {
        const std::string last_language(user_row["last_language"]);
        ids_to_user_infos_map[user_row["id"]] =
            UserInfo(user_row["id"], user_row["firstname"], user_row["lastname"], user_row["email"],
                     user_row["tuefind_rss_feed_last_notification"], (last_language.empty() ? "en" : last_language));
    }

    unsigned feed_generation_count(0), email_sent_count(0);
    for (const auto &[user_id, user_info] : ids_to_user_infos_map) {
        if (vufind_user_id.empty() and not EmailSender::IsValidEmailAddress(user_info.email_)) {
            LOG_WARNING("no valid email address for vufind.user.id " + user_id + "!");
            continue;
        }

        if (ProcessFeeds(user_id, user_info.rss_feed_last_notification_, sender_email, user_info.email_,
                         MiscUtil::GenerateSubscriptionRecipientName(user_info.first_name_, user_info.last_name_, user_info.language_code_),
                         user_info.language_code_, vufind_user_id.empty(), subsystem_type, &db_connection))
        {
            if (vufind_user_id.empty())
                ++email_sent_count;
            ++feed_generation_count;
        }
    }
    LOG_INFO("Generated " + std::to_string(feed_generation_count) + " RSS feed(s) and sent " + std::to_string(email_sent_count)
             + " email(s).");

    return EXIT_SUCCESS;
}
