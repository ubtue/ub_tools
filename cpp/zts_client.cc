/** \file    zts_client.cc
    \brief   Downloads bibliographic metadata using a Zotero Translation server.
    \author  Dr. Johannes Ruscheinski
    \author  Mario Trojan

    \copyright 2018 Universitätsbibliothek Tübingen

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
#include <memory>
#include <unordered_map>
#include <cinttypes>
#include "Compiler.h"
#include "File.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "SimpleCrawler.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"
#include "Zotero.h"


namespace zts_client {


const std::string USER_AGENT("ub_tools (https://ixtheo.de/docs/user_agents)");


void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] zts_server_url map_directory output_file\n"
              << "\t[ --ignore-robots-dot-txt ]                               Nomen est omen.\n"
              << "\t[ --proxy=<proxy_host_and_port> ]                         Proxy host and port, default none.\n"
              << "\t[ --simple-crawler-config-file=<path> ]                   Nomen est omen, default: "
              << Zotero::DEFAULT_SIMPLE_CRAWLER_CONFIG_PATH << "\n"
              << "\t[ --progress-file=<path> ]                                Nomen est omen.\n"
              << "\t[ --output-format=<format> ]                              marcxml (default), marc21 or json.\n"
              << "\n"
              << "\tzts_server_url                                            URL for Zotero Translation Server.\n"
              << "\tmap_directory                                             path to a subdirectory containing all required\n"
              << "\t                                                          map files and the file containing hashes of\n"
              << "\t                                                          previously generated records.\n"
              << "\toutput_file                                               Nomen est omen.\n"
              << "\n";
    std::exit(EXIT_FAILURE);
}


void HarvestSites(const SimpleCrawler::Params &crawler_params, const std::shared_ptr<RegexMatcher> supported_urls_regex,
                  const std::vector<SimpleCrawler::SiteDesc> &site_descs,
                  const std::shared_ptr<Zotero::HarvestParams> harvest_params,
                  Zotero::AugmentParams * const augment_params, std::unique_ptr<File> &progress_file,
                  unsigned * const total_record_count, unsigned * const total_previously_downloaded_count)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;
    for (const auto &site_desc : site_descs)
        total_record_count_and_previously_downloaded_record_count
            += Zotero::HarvestSite(site_desc, crawler_params, supported_urls_regex, harvest_params, augment_params, progress_file.get());

    logger->info("Processed " + std::to_string(total_record_count_and_previously_downloaded_record_count.first)
                 + " (new: "
                 + std::to_string(total_record_count_and_previously_downloaded_record_count.first
                                  - total_record_count_and_previously_downloaded_record_count.second)
                 + ")" + " URL's.");

    *total_record_count = total_record_count_and_previously_downloaded_record_count.first;
    *total_previously_downloaded_count = total_record_count_and_previously_downloaded_record_count.second;
}


void Main(int argc, char *argv[]) {
    ::progname = argv[0];
    if (argc < 4 or argc > 9)
        Usage();

    bool ignore_robots_dot_txt(false);
    if (std::strcmp(argv[1], "--ignore-robots-dot-txt") == 0) {
        ignore_robots_dot_txt = true;
        --argc, ++argv;
    }

    std::string proxy_host_and_port;
    const std::string PROXY_FLAG_PREFIX("--proxy=");
    if (StringUtil::StartsWith(argv[1], PROXY_FLAG_PREFIX)) {
        proxy_host_and_port = argv[1] + PROXY_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::string simple_crawler_config_path;
    const std::string CONFIG_FLAG_PREFIX("--simple-crawler-config-file=");
    if (StringUtil::StartsWith(argv[1], CONFIG_FLAG_PREFIX)) {
        simple_crawler_config_path = argv[1] + CONFIG_FLAG_PREFIX.length();
        --argc, ++argv;
    } else
        simple_crawler_config_path = Zotero::DEFAULT_SIMPLE_CRAWLER_CONFIG_PATH;

    std::string progress_filename;
    const std::string PROGRESS_FILE_FLAG_PREFIX("--progress-file=");
    if (StringUtil::StartsWith(argv[1], PROGRESS_FILE_FLAG_PREFIX)) {
        progress_filename = argv[1] + PROGRESS_FILE_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    const std::string OUTPUT_FORMAT_FLAG_PREFIX("--output-format=");
    std::string output_format("marcxml");
    if (StringUtil::StartsWith(argv[1], OUTPUT_FORMAT_FLAG_PREFIX)) {
        output_format = argv[1] + OUTPUT_FORMAT_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    std::string map_directory_path(argv[2]);
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += '/';

    try {
        harvest_params->zts_server_url_ = Url(argv[1]);
        Zotero::AugmentMaps augment_maps(map_directory_path);
        Zotero::AugmentParams augment_params(&augment_maps);
        const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

        const std::string output_file(argv[3]);
        harvest_params->format_handler_ = Zotero::FormatHandler::Factory(output_format, output_file, &augment_params, harvest_params);
        unsigned total_record_count(0), total_previously_downloaded_count(0);

        std::unique_ptr<File> progress_file;
        if (not progress_filename.empty())
            progress_file = FileUtil::OpenOutputFileOrDie(progress_filename);

        SimpleCrawler::Params crawler_params;
        crawler_params.ignore_robots_dot_txt_ = ignore_robots_dot_txt;
        crawler_params.min_url_processing_time_ = Zotero::DEFAULT_MIN_URL_PROCESSING_TIME;
        crawler_params.proxy_host_and_port_ = proxy_host_and_port;
        crawler_params.timeout_ = Zotero::DEFAULT_TIMEOUT;

        std::vector<SimpleCrawler::SiteDesc> site_descs;
        SimpleCrawler::ParseConfigFile(simple_crawler_config_path, &site_descs);

        HarvestSites(crawler_params, supported_urls_regex, site_descs,
                     harvest_params, &augment_params, progress_file,
                     &total_record_count, &total_previously_downloaded_count);

        LOG_INFO("Harvested a total of " + StringUtil::ToString(total_record_count) + " records of which "
             + StringUtil::ToString(total_previously_downloaded_count) + " were already previously downloaded.");

        std::unique_ptr<File> previously_downloaded_output(
            FileUtil::OpenOutputFileOrDie(map_directory_path + "previously_downloaded.hashes"));
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}


} // namespace zts_client


int main(int argc, char *argv[]) {
    zts_client::Main(argc, argv);
}
