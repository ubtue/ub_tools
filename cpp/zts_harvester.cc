/** \brief Handles crawling as well as RSS feeds.
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
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include "DbConnection.h"
#include "IniFile.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path [section1 section2 .. sectionN]\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--verbosity=log_level]                                     Possible log levels are ERROR, WARNING, INFO, and DEBUG with the default being WARNING.\n"
              << "\t[--test]                                                    No download information will be stored\n"
              << "\t[--ignore-robots-dot-txt]\n"
              << "\t[--map-directory=map_directory]\n"
              << "\t[--previous-downloads-db-file=previous_downloads_db_file]\n"
              << "\t[--output-file=output_file]\n"
              << "\n"
              << "\tIf any section names have been provided, only those will be processed o/w all sections will be processed.\n\n";
    std::exit(EXIT_FAILURE);
}


void ReadAugmentParamsFromIni(const IniFile::Section &section, Zotero::AugmentParams * const augment_params) {
    augment_params->override_ISSN_print_ = section.getString("issn_print", "");
    augment_params->override_ISSN_online_ = section.getString("issn_online", "");
    augment_params->strptime_format_ = section.getString("strptime_format", "");
}


UnsignedPair ProcessRSSFeed(const IniFile::Section &section, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                            Zotero::AugmentParams * const augment_params, DbConnection * const db_connection, const bool &test)
{
    ReadAugmentParamsFromIni(section, augment_params);

    const std::string feed_url(section.getString("feed"));
    LOG_DEBUG("feed_url: " + feed_url);
    Zotero::RSSHarvestMode rss_harvest_mode(Zotero::RSSHarvestMode::NORMAL);
    if (test)
        rss_harvest_mode = Zotero::RSSHarvestMode::TEST;
    return Zotero::HarvestSyndicationURL(rss_harvest_mode, feed_url, harvest_params, augment_params, db_connection);
}


void InitSiteDescFromIniFileSection(const IniFile::Section &section, SimpleCrawler::SiteDesc * const site_desc) {
    site_desc->start_url_ = section.getString("base_url");
    site_desc->max_crawl_depth_ = section.getUnsigned("max_crawl_depth");
    site_desc->url_regex_matcher_.reset(RegexMatcher::RegexMatcherFactoryOrDie(section.getString("extraction_regex")));
    site_desc->strptime_format_ = section.getString("strptime_format", "");
}


UnsignedPair ProcessCrawl(const IniFile::Section &section, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                          Zotero::AugmentParams * const augment_params, const SimpleCrawler::Params &crawler_params,
                          const std::shared_ptr<RegexMatcher> &supported_urls_regex)
{
    ReadAugmentParamsFromIni(section, augment_params);

    SimpleCrawler::SiteDesc site_desc;
    InitSiteDescFromIniFileSection(section, &site_desc);
    return Zotero::HarvestSite(site_desc, crawler_params, supported_urls_regex, harvest_params, augment_params);
}


std::string GetMarcFormat(const std::string &output_filename) {
    switch (MARC::GuessFileType(output_filename)) {
    case MARC::FileType::BINARY:
        return "marc21";
    case MARC::FileType::XML:
        return "marcxml";
    default:
        LOG_ERROR("can't determine output format from MARC output filename \"" + output_filename + "\"!");
    }
}


const std::string RSS_HARVESTER_CONF_FILE_PATH("/usr/local/var/lib/tuelib/rss_harvester.conf");


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool test(false);
    if (std::strcmp(argv[1], "--test") == 0) {
        test = true;
        --argc, ++argv;
    }

    bool ignore_robots_dot_txt(false);
    if (std::strcmp(argv[1], "--ignore-robots-dot-txt") == 0) {
        ignore_robots_dot_txt = true;
        --argc, ++argv;
    }

    std::string map_directory_path;
    const std::string MAP_DIRECTORY_FLAG_PREFIX("--map-directory=");
    if (StringUtil::StartsWith(argv[1], MAP_DIRECTORY_FLAG_PREFIX)) {
        map_directory_path = argv[1] + MAP_DIRECTORY_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::string previous_downloads_db_path;
    const std::string PREVIOUS_DOWNLOADS_DB_FLAG_PREFIX("--previous-downloads-db-file=");
    if (StringUtil::StartsWith(argv[1], PREVIOUS_DOWNLOADS_DB_FLAG_PREFIX)) {
        previous_downloads_db_path = argv[1] + PREVIOUS_DOWNLOADS_DB_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::string output_file;
    const std::string OUTPUT_FILE_FLAG_PREFIX("--output-file=");
    if (StringUtil::StartsWith(argv[1], OUTPUT_FILE_FLAG_PREFIX)) {
        output_file = argv[1] + OUTPUT_FILE_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    IniFile ini_file(argv[1]);

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    harvest_params->zts_server_url_ = Url(ini_file.getString("", "zts_server_url"));

    if (map_directory_path.empty())
        map_directory_path = ini_file.getString("", "map_directory_path");
    if (previous_downloads_db_path.empty())
        previous_downloads_db_path = ini_file.getString("", "previous_downloads_db_path");

    // ZoteroFormatHandler expects a directory path with a trailing /
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += "/";

    Zotero::AugmentMaps augment_maps(map_directory_path);
    Zotero::AugmentParams augment_params(&augment_maps);
    const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

    std::unique_ptr<DbConnection> db_connection;
    const IniFile rss_ini_file(RSS_HARVESTER_CONF_FILE_PATH);
    const std::string sql_database(rss_ini_file.getString("Database", "sql_database"));
    const std::string sql_username(rss_ini_file.getString("Database", "sql_username"));
    const std::string sql_password(rss_ini_file.getString("Database", "sql_password"));
    db_connection.reset(new DbConnection(sql_database, sql_username, sql_password));

    if (output_file.empty())
        output_file = ini_file.getString("", "marc_output_file");
    harvest_params->format_handler_ = Zotero::FormatHandler::Factory(previous_downloads_db_path,
                                                                     GetMarcFormat(output_file), output_file,
                                                                     &augment_params, harvest_params);

    SimpleCrawler::Params crawler_params;
    crawler_params.ignore_robots_dot_txt_ = ignore_robots_dot_txt;
    crawler_params.min_url_processing_time_ = Zotero::DEFAULT_MIN_URL_PROCESSING_TIME;
    crawler_params.timeout_ = Zotero::DEFAULT_TIMEOUT;

    std::unordered_map<std::string, bool> section_name_to_found_flag_map;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        section_name_to_found_flag_map.emplace(argv[arg_no], false);

    enum Type { RSS, CRAWL };
    const std::map<std::string, int> string_to_value_map{ {"RSS", RSS }, { "CRAWL", CRAWL } };

    unsigned processed_section_count(0);
    UnsignedPair total_record_count_and_previously_downloaded_record_count;

    for (const auto &section : ini_file) {
        if (section.first.empty())
            continue;       // don't parse the global parameters section

        if (not section_name_to_found_flag_map.empty()) {
            const auto section_name_and_found_flag(section_name_to_found_flag_map.find(section.first));
            if (section_name_and_found_flag == section_name_to_found_flag_map.end())
                continue;
            section_name_and_found_flag->second = true;
        }
        ++processed_section_count;

        LOG_INFO("Processing section \"" + section.first + "\".");
        const Type type(static_cast<Type>(section.second.getEnum("type", string_to_value_map)));
        if (type == RSS)
            total_record_count_and_previously_downloaded_record_count += ProcessRSSFeed(section.second, harvest_params, &augment_params,
                                                                                        db_connection.get(), test);
        else
            total_record_count_and_previously_downloaded_record_count +=
                ProcessCrawl(section.second, harvest_params, &augment_params, crawler_params, supported_urls_regex);
    }

    LOG_INFO("Extracted metadata from "
             + std::to_string(total_record_count_and_previously_downloaded_record_count.first
                              - total_record_count_and_previously_downloaded_record_count.second) + " page(s).");

    if (section_name_to_found_flag_map.size() > processed_section_count) {
        std::cerr << "The following sections were specified but not processed:\n";
        for (const auto &section_name_and_found_flag : section_name_to_found_flag_map)
            std::cerr << '\t' << section_name_and_found_flag.first << '\n';
    }

    return EXIT_SUCCESS;
}
