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
#include "FileUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose|--test] [--proxy=<proxy_host_and_port>] [--strptime_format=<strptime_format>] rss_url_list_filename zts_server_url map_directory marc_output\n"
              << "       When --test has been specified duplicate checks are disabled and verbose mode is enabled.\n";
    std::exit(EXIT_FAILURE);
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


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();

    Zotero::RSSHarvestMode mode;
    if (std::strcmp(argv[1], "--verbose") == 0) {
        mode = Zotero::RSSHarvestMode::VERBOSE;
        --argc, ++argv;
    } else if (std::strcmp(argv[1], "--test") == 0) {
        mode = Zotero::RSSHarvestMode::TEST;
        --argc, ++argv;
    } else
        mode = Zotero::RSSHarvestMode::NORMAL;

    std::string proxy_host_and_port;
    const std::string PROXY_FLAG_PREFIX("--proxy=");
    if (StringUtil::StartsWith(argv[1], PROXY_FLAG_PREFIX)) {
        proxy_host_and_port = argv[1] + PROXY_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::string strptime_format;
    const std::string STRPTIME_FORMAT_FLAG_PREFIX("--strptime_format=");
    if (StringUtil::StartsWith(argv[1], STRPTIME_FORMAT_FLAG_PREFIX)) {
        strptime_format = argv[1] + STRPTIME_FORMAT_FLAG_PREFIX.length();
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

        BSZTransform::AugmentMaps augment_maps(map_directory_path);
        Zotero::GobalAugmentParams global_augment_params(&augment_maps);
        Zotero::SiteParams augment_params;
        DbConnection db_connection;
        Zotero::HarvesterErrorLogger error_logger;

        augment_params.global_params_ = &global_augment_params;
        augment_params.strptime_format_ = strptime_format;
        const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));
        const std::string MARC_OUTPUT_FILE(argv[4]);
        harvest_params->format_handler_ = Zotero::FormatHandler::Factory(&db_connection,
                                                                         GetMarcFormat(MARC_OUTPUT_FILE), MARC_OUTPUT_FILE, harvest_params);
        harvest_params->format_handler_->setAugmentParams(&augment_params);



        Zotero::MarcFormatHandler * const marc_format_handler(reinterpret_cast<Zotero::MarcFormatHandler *>(
            harvest_params->format_handler_.get()));
        if (unlikely(marc_format_handler == nullptr))
            LOG_ERROR("expected a MarcFormatHandler!");

        UnsignedPair total_record_count_and_previously_downloaded_record_count;
        for (const auto &server_url : server_urls) {
            total_record_count_and_previously_downloaded_record_count +=
                Zotero::HarvestSyndicationURL(mode, server_url, harvest_params, augment_params, &error_logger, &db_connection);
        }

        LOG_INFO("Extracted metadata from "
                 + std::to_string(total_record_count_and_previously_downloaded_record_count.first
                                  - total_record_count_and_previously_downloaded_record_count.second) + " page(s).");
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
