/** \file rss_harvester.cc
 *  \brief Downloads and evaluates RSS updates.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <iostream>
#include <cstring>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "SqlUtil.h"
#include "SyndicationFormat.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose|--test] [--proxy=<proxy_host_and_port>] rss_url_list_filename zts_server_url map_directory marc_output\n"
              << "       When --test has been specified duplicate checks are disabled and verbose mode is enabled.\n";
    std::exit(EXIT_FAILURE);
}


enum Mode { VERBOSE, TEST, NORMAL };


// Returns true if we can determine that the last_build_date column value stored in the rss_feeds table for the feed identified
// by "feed_url" is no older than the "last_build_date" time_t passed into this function.  (This is somewhat complicated by the
// fact that both, the column value as well as the time_t value may contain indeterminate values.)
bool FeedContainsNoNewItems(const Mode mode, DbConnection * const db_connection, const std::string &feed_url,
                            const time_t last_build_date)
{
    db_connection->queryOrDie("SELECT last_build_date FROM rss_feeds WHERE feed_url='" + db_connection->escapeString(feed_url)
                              + "'");
    DbResultSet result_set(db_connection->getLastResultSet());

    std::string date_string;
    if (result_set.empty()) {
        if (last_build_date == TimeUtil::BAD_TIME_T)
            date_string = "0000-00-00 00:00:00";
        else
            date_string = SqlUtil::TimeTToDatetime(last_build_date);

        if (mode == VERBOSE)
            LOG_INFO("Creating new feed entry in rss_feeds table for \"" + feed_url + "\".");
        if (mode != TEST)
            db_connection->queryOrDie("INSERT INTO rss_feeds SET feed_url='" + db_connection->escapeString(feed_url)
                                      + "',last_build_date='" + date_string + "'");
        return false;
    }

    const DbRow first_row(result_set.getNextRow());
    date_string = first_row["last_build_date"];
    if (date_string != "0000-00-00 00:00:00" and last_build_date != TimeUtil::BAD_TIME_T
        and SqlUtil::DatetimeToTimeT(date_string) >= last_build_date)
        return true;

    return false;
}


// Returns the feed ID for the URL "feed_url".
std::string GetFeedID(const Mode mode, DbConnection * const db_connection, const std::string &feed_url) {
    db_connection->queryOrDie("SELECT id FROM rss_feeds WHERE feed_url='" + db_connection->escapeString(feed_url)
                              + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (unlikely(result_set.empty())) {
        if (mode == TEST)
            return "-1"; // Must be an INT.
        LOG_ERROR("unexpected missing feed for URL \"" + feed_url + "\".");
    }
    const DbRow first_row(result_set.getNextRow());
    return first_row["id"];
}


void UpdateLastBuildDate(DbConnection * const db_connection, const std::string &feed_url, const time_t last_build_date) {
    std::string last_build_date_string;
    if (last_build_date == TimeUtil::BAD_TIME_T)
        last_build_date_string = "0000-00-00 00:00:00";
    else
        last_build_date_string = SqlUtil::TimeTToDatetime(last_build_date);
    db_connection->queryOrDie("UPDATE rss_feeds SET last_build_date='" + last_build_date_string + "' WHERE feed_url='"
                              + db_connection->escapeString(feed_url) + "'");
}


// Returns true if the item with item ID "item_id" and feed ID "feed_id" were found in the rss_items table, else
// returns false.
bool ItemAlreadyProcessed(DbConnection * const db_connection, const std::string &feed_id, const std::string &item_id) {
    db_connection->queryOrDie("SELECT creation_datetime FROM rss_items WHERE feed_id='"
                              + feed_id + "' AND item_id='" + db_connection->escapeString(item_id) + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return false;

    if (logger->getMinimumLogLevel() >= Logger::LL_DEBUG) {
        const DbRow first_row(result_set.getNextRow());
        LOG_DEBUG("Previously retrieved item w/ ID \"" + item_id + "\" at " + first_row["creation_datetime"] + ".");
    }

    return true;
}


unsigned ProcessSyndicationURL(const Mode mode, const std::string &feed_url,
                               const std::shared_ptr<Zotero::HarvestParams> harvest_params,
                               const std::shared_ptr<const Zotero::HarvestMaps> harvest_maps, DbConnection * const db_connection)
{
    unsigned successfully_processed_count(0);

    if (mode != NORMAL)
        std::cerr << "Processing URL: " << feed_url << '\n';

    Downloader downloader(feed_url);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Download problem for \"" + feed_url + "\": " + downloader.getLastErrorMessage());
        return successfully_processed_count;
    }

    std::string err_msg;
    std::unique_ptr<SyndicationFormat> syndication_format(SyndicationFormat::Factory(downloader.getMessageBody(), &err_msg));
    if (syndication_format == nullptr) {
        LOG_WARNING("Problem parsing XML document for \"" + feed_url + "\": " + err_msg);
        return successfully_processed_count;
    }

    const time_t last_build_date(syndication_format->getLastBuildDate());
    if (mode != NORMAL) {
        std::cout << feed_url << " (" << syndication_format->getFormatName() << "):\n";
        std::cout << "\tTitle: " << syndication_format->getTitle() << '\n';
        if (last_build_date != TimeUtil::BAD_TIME_T)
            std::cout << "\tLast build date: " << TimeUtil::TimeTToUtcString(last_build_date) << '\n';
        std::cout << "\tLink: " << syndication_format->getLink() << '\n';
        std::cout << "\tDescription: " << syndication_format->getDescription() << '\n'; 
   }

    if (mode != TEST and FeedContainsNoNewItems(mode, db_connection, feed_url, last_build_date))
        return successfully_processed_count;

    const std::string feed_id(mode == TEST ? "" : GetFeedID(mode, db_connection, feed_url));
    for (const auto &item : *syndication_format) {
        if (mode != TEST and ItemAlreadyProcessed(db_connection, feed_id, item.getId()))
            continue;

        const std::string title(item.getTitle());
        if (not title.empty() and mode != NORMAL)
            std::cout << "\t\tTitle: " << title << '\n';

        const auto record_count_and_previously_downloaded_count(
            Zotero::Harvest(item.getLink(), harvest_params, harvest_maps, "", /* verbose = */ mode != NORMAL));
        successfully_processed_count += record_count_and_previously_downloaded_count.first;

        if (mode != TEST)
            db_connection->queryOrDie("INSERT INTO rss_items SET feed_id='" + feed_id + "',item_id='"
                                      + db_connection->escapeString(item.getId()) + "'");

    }
    if (mode != TEST)
        UpdateLastBuildDate(db_connection, feed_url, last_build_date);

    return successfully_processed_count;
}


void LoadServerURLs(const std::string &path, std::vector<std::string> * const server_urls) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(path));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        StringUtil::TrimWhite(&line);
        if (not line.empty())
            server_urls->emplace_back(line);
    }
}


std::string GetMarcFormat(const std::string &output_filename) {
    if (StringUtil::EndsWith(output_filename, ".mrc") or StringUtil::EndsWith(output_filename, ".marc"))
        return "marc21";
    if (StringUtil::EndsWith(output_filename, ".xml"))
        return "marcxml";
    LOG_ERROR("can't determine output format from MARC output filename \"" + output_filename + "\"!");
}


const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/rss_harvester.conf");


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();

    Mode mode;
    if (std::strcmp(argv[1], "--verbose") == 0) {
        mode = VERBOSE;
        --argc, ++argv;
    } else if (std::strcmp(argv[1], "--test") == 0) {
        mode = TEST;
        --argc, ++argv;
    } else
        mode = NORMAL;

    std::string proxy_host_and_port;
    const std::string PROXY_FLAG_PREFIX("--proxy=");
    if (StringUtil::StartsWith(argv[1], PROXY_FLAG_PREFIX)) {
        proxy_host_and_port = argv[1] + PROXY_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    if (argc != 5)
        Usage();

    try {
        std::vector<std::string> server_urls;
        LoadServerURLs(argv[1], &server_urls);

        std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
        harvest_params->zts_server_url_ = Url(argv[2]);

        std::string map_directory_path(argv[3]);
        if (not StringUtil::EndsWith(map_directory_path, '/'))
            map_directory_path += '/';

        std::shared_ptr<Zotero::HarvestMaps> harvest_maps(Zotero::LoadMapFilesFromDirectory(map_directory_path));
        const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

        const std::string PREVIOUSLY_DOWNLOADED_HASHES_PATH(map_directory_path + "previously_downloaded.hashes");
        Zotero::PreviouslyDownloadedHashesManager previously_downloaded_hashes_manager(PREVIOUSLY_DOWNLOADED_HASHES_PATH,
                                                                                       &harvest_maps->previously_downloaded_);
        const std::string MARC_OUTPUT_FILE(argv[4]);
        harvest_params->format_handler_ = Zotero::FormatHandler::Factory(GetMarcFormat(MARC_OUTPUT_FILE), MARC_OUTPUT_FILE,
                                                                         harvest_maps, harvest_params);

        std::unique_ptr<DbConnection> db_connection;
        if (mode != TEST) {
            const IniFile ini_file(CONF_FILE_PATH);
            const std::string sql_database(ini_file.getString("Database", "sql_database"));
            const std::string sql_username(ini_file.getString("Database", "sql_username"));
            const std::string sql_password(ini_file.getString("Database", "sql_password"));
            db_connection.reset(new DbConnection(sql_database, sql_username, sql_password));
        }
        
        harvest_params->format_handler_->prepareProcessing();

        Zotero::MarcFormatHandler * const marc_format_handler(reinterpret_cast<Zotero::MarcFormatHandler *>(
            harvest_params->format_handler_.get()));
        if (unlikely(marc_format_handler == nullptr))
            LOG_ERROR("expected a MarcFormatHandler!");

        unsigned download_count(0);
        for (const auto &server_url : server_urls)
            download_count += ProcessSyndicationURL(mode, server_url, harvest_params, harvest_maps, db_connection.get());

        harvest_params->format_handler_->finishProcessing();

        LOG_INFO("Extracted metadata from " + std::to_string(download_count) + " page(s).");
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
