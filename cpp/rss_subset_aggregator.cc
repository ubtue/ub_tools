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
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "Template.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"
#include "XmlWriter.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("email_address");
}


struct HarvestedRSSItem {
    SyndicationFormat::Item item_;
    std::string feed_title_;
    std::string feed_url_;

    HarvestedRSSItem(const SyndicationFormat::Item item, const std::string feed_title, const std::string feed_url)
        : item_(item), feed_title_(feed_title), feed_url_(feed_url) {}
};


struct ChannelDesc {
    std::string title_;
    std::string link_;
public:
    ChannelDesc(const std::string &title, const std::string &link)
        : title_(title), link_(link) { }
};


const std::map<std::string, ChannelDesc> subsystem_type_to_channel_desc_map = {
    { "relbib",  ChannelDesc("RelBib Aggregator",  "https://relbib.de/")                },
    { "ixtheo",  ChannelDesc("IxTheo Aggregator",  "https://itheo.de/")                 },
    { "krimdok", ChannelDesc("KrimDok Aggregator", "https://krimdok.uni-tuebingen.de/") },
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
                           XmlWriter * const xml_writer)
{
    xml_writer->openTag("rss", { { "version", "2.0" }, { "xmlns:tuefind", "https://github.com/ubtue/tuefind" } });
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

        const auto description(HtmlUtil::ShortenText(harvested_item.item_.getDescription(), /*max_length = */500));
        if (not description.empty())
            xml_writer->writeTagsWithData("description", description);

        xml_writer->writeTagsWithData("pubDate",
                                      TimeUtil::TimeTToString(harvested_item.item_.getPubDate(), TimeUtil::RFC822_FORMAT,
                                                                     TimeUtil::UTC));
        xml_writer->writeTagsWithData("guid", harvested_item.item_.getId());
        xml_writer->writeTagsWithData("tuefind:rss_title", harvested_item.feed_title_);
        xml_writer->writeTagsWithData("tuefind:rss_url", harvested_item.feed_url_);
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
    FeedNameAndURL(const std::string &name, const std::string &url)
        : name_(name), url_(url) { }
};


void SendEmail(const std::string &email_sender, const std::string &user_email, const std::string &user_address,
               const std::string &language, const std::vector<HarvestedRSSItem> &harvested_items)
{
    const auto template_filename_prefix(UBTools::GetTuelibPath() + "rss_email.template");
    std::string template_filename(template_filename_prefix + "." + language);
    if (not FileUtil::Exists(template_filename))
        template_filename = template_filename_prefix + ".en";
    static const std::string email_template(FileUtil::ReadStringOrDie(template_filename));

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("user_email", user_email);
    names_to_values_map.insertScalar("user_address", user_address);

    std::vector<std::string> titles, links, descriptions;
    for (const auto &harvested_item : harvested_items) {
        titles.emplace_back(HtmlUtil::HtmlEscape(harvested_item.item_.getTitle()));
        links.emplace_back(harvested_item.item_.getLink());
        descriptions.emplace_back(HtmlUtil::HtmlEscape(harvested_item.item_.getDescription()));
    }
    names_to_values_map.insertArray("titles", titles);
    names_to_values_map.insertArray("links", links);
    names_to_values_map.insertArray("descriptions", descriptions);

    const auto email_body(Template::ExpandTemplate(email_template, names_to_values_map));
    const auto retcode(EmailSender::SimplerSendEmail(email_sender, { user_email }, "RSS Feeds Update",
                                                     email_body, EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML));
    if (retcode > 299)
        LOG_WARNING("EmailSender::SimplerSendEmail returned " + std::to_string(retcode) + " while trying to send to \""
                    + user_email + "\"!");
}


const unsigned DEFAULT_XML_INDENT_AMOUNT(2);


void GenerateFeed(const std::string &user_id, const std::string &subsystem_type,
                  const std::vector<HarvestedRSSItem> &harvested_items)
{
    static const std::string PATH_PREFIX("/var/www/custom_rss_feeds/");
    const std::string xml_output_filename(PATH_PREFIX + subsystem_type + "_" + user_id + "_rss.xml");
    XmlWriter xml_writer(FileUtil::OpenOutputFileOrDie(xml_output_filename).release(),
                         XmlWriter::WriteTheXmlDeclaration, DEFAULT_XML_INDENT_AMOUNT);
    WriteRSSFeedXMLOutput(subsystem_type, harvested_items, &xml_writer);
}


void ProcessFeeds(const std::string &user_id, const std::string &email_sender, const std::string &user_email,
                  const std::string &user_address, const std::string &language, const std::string &rss_feed_notification_type,
                  const std::string &subsystem_type, DbConnection * const db_connection)
{
    db_connection->queryOrDie("SELECT rss_feeds_id FROM tuefind_rss_subscriptions WHERE user_id=" + user_id);
    auto rss_subscriptions_result_set(db_connection->getLastResultSet());
    std::vector<std::string> feed_ids;
    while (const auto row = rss_subscriptions_result_set.getNextRow())\
        feed_ids.emplace_back(row["rss_feeds_id"]);
    if (feed_ids.empty())
        return;

    std::vector<HarvestedRSSItem> harvested_items;
    for (const auto &feed_id : feed_ids) {
        db_connection->queryOrDie("SELECT feed_name,feed_url FROM tuefind_rss_feeds WHERE id=" + feed_id);
        auto feed_result_set(db_connection->getLastResultSet());
        const auto feed_row(feed_result_set.getNextRow());
        const auto feed_name(feed_row["feed_name"]);
        const auto feed_url(feed_row["feed_url"]);
        feed_result_set.~DbResultSet();

        db_connection->queryOrDie("SELECT item_title,item_description,item_url,item_id,pub_date FROM "
                                  "tuefind_rss_items WHERE rss_feeds_id=" + feed_id);
        auto items_result_set(db_connection->getLastResultSet());
        while (const auto item_row = items_result_set.getNextRow())
            harvested_items.emplace_back(SyndicationFormat::Item(item_row["item_title"], item_row["item_description"],
                                                                 item_row["item_url"], item_row["item_id"],
                                                                 SqlUtil::DatetimeToTimeT(item_row["pub_date"])),
                                         feed_name, feed_url);
    }

    if (rss_feed_notification_type == "email")
        SendEmail(email_sender, user_email, user_address, language, harvested_items);
    else
        GenerateFeed(user_id, subsystem_type, harvested_items);
}


// Yes, this function has a confusing name but I could not think of a better one.
// What is meant is how to address a user!
std::string GenerateUserAddress(const std::string &appellation, const std::string &first_name, const std::string &last_name) {
    if (last_name.empty())
        return first_name;

    if (appellation.empty())
        return first_name + " " + last_name;

    return appellation + " " + last_name;
}


} // unnamed namespace


struct UserInfo {
    std::string user_id_;
    std::string first_name_;
    std::string last_name_;
    std::string email_;
    std::string rss_feed_notification_type_;
    time_t rss_feed_last_notification_;
public:
    UserInfo() = default;
    UserInfo(const UserInfo &other) = default;
    UserInfo(const std::string &user_id, const std::string &first_name, const std::string &last_name,
             const std::string &email, const std::string &rss_feed_notification_type,
             const time_t rss_feed_last_notification)
        : user_id_(user_id), first_name_(first_name), last_name_(last_name), email_(email),
          rss_feed_notification_type_(rss_feed_notification_type), rss_feed_last_notification_(rss_feed_last_notification) { }
};


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const std::string email_address(argv[1]);
    const auto db_connection(VuFind::GetDbConnection());

    db_connection->queryOrDie("SELECT id,firstname,lastname,email,tuefind_rss_feed_notification_type"
                              ",tuefind_rss_feed_last_notification FROM user "
                              "WHERE tuefind_rss_feed_notification_type IS NOT NULL");
    auto user_result_set(db_connection->getLastResultSet());
    std::unordered_map<std::string, UserInfo> ids_to_user_infos_map;
    while (const auto user_row = user_result_set.getNextRow())
        ids_to_user_infos_map[user_row["id"]] =
            UserInfo(user_row["id"], user_row["firstname"], user_row["lastname"], user_row["email"],
                     user_row["tuefind_rss_feed_notification_type"],
                     TimeUtil::StringToTimeT(user_row["tuefind_rss_feed_last_notification"]));

    unsigned feed_generation_count(0), email_sent_count(0);
    for (const auto &[user_id, user_info] : ids_to_user_infos_map) {
        if (user_info.rss_feed_notification_type_ == "email" and not EmailSender::IsValidEmailAddress(user_info.email_)) {
            LOG_WARNING("no valid email address for vfind.user.id " + user_id + "!");
            continue;
        }

        db_connection->queryOrDie("SELECT appellation,language,user_type FROM ixtheo_user");
        auto ixtheo_user_row(db_connection->getLastResultSet().getNextRow());
        const auto appellation(ixtheo_user_row.getValue("appellation"));
        const auto language(ixtheo_user_row.getValue("language", "en"));
        const auto subsystem_type(ixtheo_user_row["user_type"]);
        ProcessFeeds(user_id, email_address, user_info.email_,
                     GenerateUserAddress(appellation, user_info.first_name_, user_info.last_name_),
                     language, user_info.rss_feed_notification_type_, subsystem_type, db_connection.get());
        user_info.rss_feed_notification_type_ == "email" ? ++email_sent_count : ++feed_generation_count;
    }
    LOG_INFO("Generated " + std::to_string(feed_generation_count) + " RSS feed(s) and sent "
             + std::to_string(email_sent_count) + " email(s).");

    return EXIT_SUCCESS;
}
