/** \file   rss_aggregator.cc
 *  \brief  Downloads and aggregates RSS feeds.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cinttypes>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "SignalUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "UBTools.h"
#include "util.h"
#include "XmlWriter.h"


namespace {


volatile sig_atomic_t sigterm_seen(false);


void SigTermHandler(int /* signum */) {
    sigterm_seen = true;
}


volatile sig_atomic_t sighup_seen(false);


void SigHupHandler(int /* signum */) {
    sighup_seen = true;
}


[[noreturn]] void Usage() {
    ::Usage("[--one-shot] [--config-file=config_file_path] [--process-name=new_process_name] xml_output_path\n"
            "       When --one-shot has been specified the program does not daemonise and exits after generating the feed XML.\n"
            "       The default config file path is \"" + UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf\".");
}


// These must be in sync with the sizes in data/ub_tools.sql (rss_aggregator table)
const size_t MAX_ITEM_ID_LENGTH(100);
const size_t MAX_ITEM_URL_LENGTH(512);
const size_t MAX_ITEM_TITLE_LENGTH(200);
const size_t MAX_SERIAL_NAME_LENGTH(200);


struct HarvestedRSSItem {
    SyndicationFormat::Item item_;
    std::string feed_title_;
    std::string feed_url_;

    HarvestedRSSItem(const SyndicationFormat::Item item, const std::string feed_title, const std::string feed_url)
        : item_(item), feed_title_(feed_title), feed_url_(feed_url) {}
};


void WriteRSSFeedXMLOutput(const IniFile &ini_file, std::vector<HarvestedRSSItem> * const harvested_items, XmlWriter * const xml_writer) {
    xml_writer->openTag("rss", { { "version", "2.0" }, { "xmlns:tuefind", "https://github.com/ubtue/tuefind" } });
    xml_writer->openTag("channel");
    xml_writer->writeTagsWithEscapedData("title", ini_file.getString("Channel", "title"));
    xml_writer->writeTagsWithEscapedData("link", ini_file.getString("Channel", "link"));
    xml_writer->writeTagsWithEscapedData("description", ini_file.getString("Channel", "description"));

    for (const auto &harvested_item : *harvested_items) {
        xml_writer->openTag("item");

        const auto title(harvested_item.item_.getTitle());
        if (not title.empty())
            xml_writer->writeTagsWithEscapedData("title", harvested_item.item_.getTitle());

        xml_writer->writeTagsWithEscapedData("link", harvested_item.item_.getLink());

        const auto description(harvested_item.item_.getDescription());
        if (not description.empty())
            xml_writer->writeTagsWithEscapedData("description", description);

        xml_writer->writeTagsWithEscapedData("pubDate",
                                             TimeUtil::TimeTToString(harvested_item.item_.getPubDate(), TimeUtil::RFC822_FORMAT,
                                                                     TimeUtil::UTC));
        xml_writer->writeTagsWithEscapedData("guid", harvested_item.item_.getId());
        xml_writer->writeTagsWithEscapedData("tuefind:rss_title", harvested_item.feed_title_);
        xml_writer->writeTagsWithEscapedData("tuefind:rss_url", harvested_item.feed_url_);
        xml_writer->closeTag("item", /* suppress_indent */ false);
    }

    xml_writer->closeTag("channel");
    xml_writer->closeTag("rss");
}


// \return true if the item was new, else false.
bool ProcessRSSItem(const SyndicationFormat::Item &item, const std::string &section_name, const std::string &feed_url,
                    DbConnection * const db_connection)
{
    const std::string item_id(item.getId());
    db_connection->queryOrDie("SELECT insertion_time FROM rss_aggregator WHERE item_id='" + db_connection->escapeString(item_id) + "'");
    const DbResultSet result_set(db_connection->getLastResultSet());
    if (not result_set.empty())
        return false;

    const std::string item_url(item.getLink());
    if (item_url.empty()) {
        LOG_WARNING("got an item w/o a URL, ID is \"" + item.getId());
        return false;
    }

    db_connection->insertIntoTableOrDie("rss_aggregator",
                                        {
                                            { "item_id",          StringUtil::Truncate(MAX_ITEM_ID_LENGTH, item_id)            },
                                            { "item_url",         StringUtil::Truncate(MAX_ITEM_URL_LENGTH, item_url)          },
                                            { "item_title",       StringUtil::Truncate(MAX_ITEM_TITLE_LENGTH, item.getTitle()) },
                                            { "item_description", item.getDescription()                                        },
                                            { "serial_name",      StringUtil::Truncate(MAX_SERIAL_NAME_LENGTH, section_name)   },
                                            { "feed_url",         StringUtil::Truncate(MAX_ITEM_URL_LENGTH, feed_url)          },
                                            { "pub_date",         SqlUtil::TimeTToDatetime(item.getPubDate())                  }
                                        },
                                        DbConnection::DuplicateKeyBehaviour::DKB_IGNORE);

    return true;
}


void CheckForSigTermAndExitIfSeen() {
    if (sigterm_seen) {
        LOG_WARNING("caught SIGTERM, exiting...");
        std::exit(EXIT_SUCCESS);
    }
}


void CheckForSigHupAndReloadIniFileIfSeen(IniFile * const ini_file) {
    if (sighup_seen) {
        LOG_INFO("caught SIGHUP, reloading config file...");
        ini_file->reload();
        sighup_seen = false;
    }
}


std::unordered_map<std::string, uint64_t> section_name_to_ticks_map;


// \return the number of new items.
unsigned ProcessSection(const bool one_shot, const IniFile::Section &section,
                        Downloader * const downloader, DbConnection * const db_connection, const unsigned default_downloader_time_limit,
                        const unsigned default_poll_interval, const uint64_t now)
{
    SyndicationFormat::AugmentParams augment_params;

    const std::string feed_url(section.getString("feed_url"));
    const unsigned poll_interval(section.getUnsigned("poll_interval", default_poll_interval));
    const unsigned downloader_time_limit(section.getUnsigned("downloader_time_limit", default_downloader_time_limit) * 1000);
    augment_params.strptime_format_ = section.getString("strptime_format", "");
    const std::string &section_name(section.getSectionName());

    const std::string title_suppression_regex_str(section.getString("title_suppression_regex", ""));
    const auto title_suppression_regex(
        title_suppression_regex_str.empty() ? nullptr : RegexMatcher::RegexMatcherFactoryOrDie(title_suppression_regex_str));

    if (one_shot) {
        std::cout << "Processing section \"" << section_name << "\":\n"
                  << "\tfeed_url: " << feed_url << '\n'
                  << "\tpoll_interval: " << poll_interval << " (ignored)\n"
                  << "\tdownloader_time_limit: " << downloader_time_limit
                  << (augment_params.strptime_format_.empty() ? "" : augment_params.strptime_format_ + "\n")
                  << (title_suppression_regex_str.empty() ? "" : title_suppression_regex_str + "\n")
                  << "\n\n";
    }

    const auto section_name_and_ticks(section_name_to_ticks_map.find(section_name));
    if (section_name_and_ticks != section_name_to_ticks_map.end()) {
        if (section_name_and_ticks->second + poll_interval < now) {
            LOG_DEBUG(section_name + ": not yet time to do work, last work was done at " + std::to_string(section_name_and_ticks->second)
                      + ".");
            return 0;
        }
    }

    unsigned new_item_count(0);
    SignalUtil::SignalBlocker sigterm_blocker(SIGTERM);
    if (not downloader->newUrl(feed_url, downloader_time_limit))
        LOG_WARNING(section_name + ": failed to download the feed: " + downloader->getLastErrorMessage());
    else {
        sigterm_blocker.unblock();
        if (not one_shot)
            CheckForSigTermAndExitIfSeen();

        std::string error_message;
        std::unique_ptr<SyndicationFormat> syndication_format(
            SyndicationFormat::Factory(downloader->getMessageBody(), augment_params, &error_message));
        if (unlikely(syndication_format == nullptr))
            LOG_WARNING("failed to parse feed: " + error_message);
        else {
            for (const auto &item : *syndication_format) {
                if (not one_shot)
                    CheckForSigTermAndExitIfSeen();
                SignalUtil::SignalBlocker sigterm_blocker2(SIGTERM);

                if (title_suppression_regex != nullptr and title_suppression_regex->matched(item.getTitle())) {
                    LOG_INFO("Suppressed item because of title: \"" + StringUtil::ShortenText(item.getTitle(), 40) + "\".");
                    continue; // Skip suppressed item.
                }

                if (ProcessRSSItem(item, section_name, feed_url, db_connection))
                    ++new_item_count;
            }
        }
    }

    section_name_to_ticks_map[section_name] = now;
    return new_item_count;
}


const unsigned HARVEST_TIME_WINDOW(60); // days


size_t SelectItems(DbConnection * const db_connection, std::vector<HarvestedRSSItem> * const harvested_items) {
    const auto now(std::time(nullptr));
    db_connection->queryOrDie("SELECT * FROM rss_aggregator WHERE pub_date >= '"
                              + SqlUtil::TimeTToDatetime(now - HARVEST_TIME_WINDOW * 86400) + "' ORDER BY pub_date DESC");
    DbResultSet result_set(db_connection->getLastResultSet());
    while (const DbRow row = result_set.getNextRow())
        harvested_items->emplace_back(SyndicationFormat::Item(row["item_title"], row["item_description"], row["item_url"], row["item_id"],
                                                              SqlUtil::DatetimeToTimeT(row["pub_date"])),
                                                              row["serial_name"], row["feed_url"]);
    return result_set.size();
}


const unsigned DEFAULT_XML_INDENT_AMOUNT(2);


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool one_shot(false);
    if (std::strcmp(argv[1], "--one-shot") == 0) {
        one_shot = true;
        --argc, ++argv;
    }
    if (argc < 2)
        Usage();

    std::string config_file_path(UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf");
    if (StringUtil::StartsWith(argv[1], "--config-file=")) {
        config_file_path = argv[1] + __builtin_strlen("--config-file=");
        --argc, ++argv;
    }

    if (argc != 2)
        Usage();

    IniFile ini_file;
    DbConnection db_connection(ini_file);

    const unsigned DEFAULT_POLL_INTERVAL(ini_file.getUnsigned("", "default_poll_interval"));
    const unsigned DEFAULT_DOWNLOADER_TIME_LIMIT(ini_file.getUnsigned("", "default_downloader_time_limit"));
    const unsigned UPDATE_INTERVAL(ini_file.getUnsigned("", "update_interval"));
    const std::string PROXY(ini_file.getString("", "proxy", ""));

    if (not one_shot) {
        SignalUtil::InstallHandler(SIGTERM, SigTermHandler);
        SignalUtil::InstallHandler(SIGHUP, SigHupHandler);

        if (::daemon(0, 1 /* do not close file descriptors and redirect to /dev/null */) != 0)
            LOG_ERROR("we failed to deamonize our process!");
    }

    const std::string xml_output_filename(argv[1]);

    uint64_t ticks(0);
    Downloader::Params params;
    if (not PROXY.empty()) {
        LOG_INFO("using proxy: " + PROXY);
        params.proxy_host_and_port_ = PROXY;
    }
    Downloader downloader(params);

    for (;;) {
        LOG_DEBUG("now we're at " + std::to_string(ticks) + ".");

        CheckForSigHupAndReloadIniFileIfSeen(&ini_file);

        const time_t before(std::time(nullptr));

        std::unordered_set<std::string> already_seen_sections;
        for (const auto &section : ini_file) {
            if (sigterm_seen) {
                LOG_INFO("caught SIGTERM, shutting down...");
                return EXIT_SUCCESS;
            }

            SignalUtil::SignalBlocker sighup_blocker(SIGHUP);

            const std::string &section_name(section.getSectionName());
            if (not section_name.empty() and section_name != "CGI Params" and section_name != "Database" and section_name != "Channel") {
                if (unlikely(already_seen_sections.find(section_name) != already_seen_sections.end()))
                    LOG_ERROR("duplicate section: \"" + section_name + "\"!");
                already_seen_sections.emplace(section_name);

                LOG_INFO("Processing section \"" + section_name + "\".");
                try {
                    const unsigned new_item_count(ProcessSection(one_shot, section, &downloader, &db_connection,
                                                                 DEFAULT_DOWNLOADER_TIME_LIMIT, DEFAULT_POLL_INTERVAL, ticks));
                    LOG_INFO("Downloaded " + std::to_string(new_item_count) + " new items.");
                } catch (const std::runtime_error &x) {
                    LOG_WARNING("Error processing section \"" + section_name + "\": " + x.what());
                }
            }
        }

        std::vector<HarvestedRSSItem> harvested_items;
        const auto feed_item_count(SelectItems(&db_connection, &harvested_items));

        // scoped here so that we flush and close the output file right away
        {
            XmlWriter xml_writer(FileUtil::OpenOutputFileOrDie(xml_output_filename).release(),
                                 XmlWriter::WriteTheXmlDeclaration, DEFAULT_XML_INDENT_AMOUNT);
            WriteRSSFeedXMLOutput(ini_file, &harvested_items, &xml_writer);
        }
        LOG_INFO("Created our feed with " + std::to_string(feed_item_count) + " items from the last " + std::to_string(HARVEST_TIME_WINDOW)
                 + " days.");

        if (one_shot) // -> only run through our loop once
            return EXIT_SUCCESS;

        const time_t after(std::time(nullptr));

        uint64_t sleep_interval;
        if (after - before > UPDATE_INTERVAL * 60)
            sleep_interval = 0;
        else
            sleep_interval = (UPDATE_INTERVAL * 60 - (after - before));

        unsigned total_time_slept(0);
        do {
            const unsigned actual_time_slept(::sleep(static_cast<unsigned>(sleep_interval - total_time_slept)));
            CheckForSigTermAndExitIfSeen();
            CheckForSigHupAndReloadIniFileIfSeen(&ini_file);

            total_time_slept += actual_time_slept;
        } while (total_time_slept < sleep_interval);
        ticks += UPDATE_INTERVAL;
    }
}
