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
#include "IniFile.h"
#include "SyndicationFormat.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " [--verbose] url1 [url2 ... urlN]\n";
    std::exit(EXIT_FAILURE);
}


void ProcessSyndicationURL(const bool verbose, const std::string &url, DbConnection * const db_connection) {
    if (verbose)
        std::cerr << "Processing URL: " << url << '\n';

    Downloader downloader(url);
    if (downloader.anErrorOccurred()) {
        WARNING("Download problem for \"" + url + "\": " + downloader.getLastErrorMessage());
        return;
    }

    std::string err_msg;
    std::unique_ptr<SyndicationFormat> syndication_format(SyndicationFormat::Factory(downloader.getMessageBody(), &err_msg));
    if (syndication_format == nullptr) {
        WARNING("Problem parsing XML document for \"" + url + "\": " + err_msg);
        return;
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
        
        std::cout << "\tItem:\n";
        const std::string title(item.getTitle());
        if (not title.empty())
            std::cout << "\t\tTitle: " << title << '\n';
        const std::string description(item.getDescription());
        if (not description.empty())
            std::cout << "\t\tDescription: " << description << '\n';
        const std::string link(item.getLink());
        if (not link.empty())
            std::cout << "\t\tLink: " << link << '\n';
        const std::string id(item.getId());
        if (not id.empty())
            std::cout << "\t\tID: " << id << '\n';
        const time_t publication_date(item.getPubDate());
        if (publication_date != TimeUtil::BAD_TIME_T)
            std::cout << "\t\tDate: " << TimeUtil::TimeTToString(publication_date) << '\n';
        const std::unordered_map<std::string, std::string> &dc_and_prism_data(item.getDCAndPrismData());
        for (const auto &key_and_value : dc_and_prism_data)
            std::cout << "\t\t" << key_and_value.first << ": " << key_and_value.second << '\n';

        db_connection->queryOrDie("INSERT INTO rss SET server_url='" + db_connection->escapeString(url) + "',item_id='"
                                  + db_connection->escapeString(item.getId()) + "'");
    }
}


const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/rss_client.conf");


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    try {

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        for (int arg_no(1); arg_no < argc; ++arg_no)
            ProcessSyndicationURL(verbose, argv[arg_no], &db_connection);
    } catch (const std::exception &x) {
        ERROR("caught exception: " + std::string(x.what()));
    }

}
