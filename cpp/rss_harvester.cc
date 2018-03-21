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
#include "SyndicationFormat.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] [--proxy=<proxy_host_and_port>] rss_url_list_filename zts_server_url map_directory marc_output\n";
    std::exit(EXIT_FAILURE);
}


// Create a MARC record from the RSS DC and PRISM metadata.
void GenerateMARCRecord(MARC::Writer * const /*marc_writer*/, const SyndicationFormat::Item &/*item*/) {
}

    
unsigned ProcessSyndicationURL(const bool verbose, const std::string &url, MARC::Writer * const marc_writer,
                               const std::shared_ptr<Zotero::HarvestParams> harvest_params,
                               const std::shared_ptr<const Zotero::HarvestMaps> harvest_maps,
                               DbConnection * const db_connection)
{
    unsigned successfully_processed_count(0);

    if (verbose)
        std::cerr << "Processing URL: " << url << '\n';

    Downloader downloader(url);
    if (downloader.anErrorOccurred()) {
        WARNING("Download problem for \"" + url + "\": " + downloader.getLastErrorMessage());
        return successfully_processed_count;
    }

    std::string err_msg;
    std::unique_ptr<SyndicationFormat> syndication_format(SyndicationFormat::Factory(downloader.getMessageBody(), &err_msg));
    if (syndication_format == nullptr) {
        WARNING("Problem parsing XML document for \"" + url + "\": " + err_msg);
        return successfully_processed_count;
    }

    std::cout << url << " (" << syndication_format->getFormatName() << "):\n";
    if (verbose) {
        std::cout << "\tTitle: " << syndication_format->getTitle() << '\n';
        std::cout << "\tLink: " << syndication_format->getLink() << '\n';
        std::cout << "\tDescription: " << syndication_format->getDescription() << '\n';
    }

    for (const auto &item : *syndication_format) {
        db_connection->queryOrDie("SELECT creation_datetime FROM rss WHERE server_url='" + db_connection->escapeString(url)
                                  + "' AND item_id='" + db_connection->escapeString(item.getId()) + "'");
        DbResultSet result_set(db_connection->getLastResultSet());
        if (not result_set.empty()) {
            if (verbose) {
                const DbRow first_row(result_set.getNextRow());
                std::cout << "Previously retrieved item w/ ID \"" << item.getId() + "\" at " << first_row["creation_datetime"]
                          << ".\n";
            }
            continue;
        }

        const std::string title(item.getTitle());
        if (not title.empty() and verbose)
            std::cout << "\t\tTitle: " << title << '\n';
        const std::unordered_map<std::string, std::string> &dc_and_prism_data(item.getDCAndPrismData());
        if (dc_and_prism_data.empty()) {
            const auto record_count_and_previously_downloaded_count(
                Zotero::Harvest(item.getLink(), harvest_params, harvest_maps, "", verbose));
            successfully_processed_count += record_count_and_previously_downloaded_count.first;
        } else
            GenerateMARCRecord(marc_writer, item);

        db_connection->queryOrDie("INSERT INTO rss SET server_url='" + db_connection->escapeString(url) + "',item_id='"
                                  + db_connection->escapeString(item.getId()) + "'");

    }

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
    ERROR("can't determine output format from MARC output filename \"" + output_filename + "\"!");
}


const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/rss_harvester.conf");


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 5)
        Usage();

    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc, ++argv;
    }

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

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        harvest_params->format_handler_->prepareProcessing();

        Zotero::MarcFormatHandler * const marc_format_handler(reinterpret_cast<Zotero::MarcFormatHandler *>(
            harvest_params->format_handler_.get()));
        if (unlikely(marc_format_handler == nullptr))
            ERROR("expected a MarcFormatHandler!");
        MARC::Writer * const marc_writer(marc_format_handler->getWriter());

        unsigned download_count(0);
        for (const auto &server_url : server_urls)
            download_count += ProcessSyndicationURL(verbose, server_url, marc_writer, harvest_params, harvest_maps,
                                                    &db_connection);

        harvest_params->format_handler_->finishProcessing();

        logger->info("Extracted metadata from " + std::to_string(download_count) + " pages.");
    } catch (const std::exception &x) {
        ERROR("caught exception: " + std::string(x.what()));
    }

}
