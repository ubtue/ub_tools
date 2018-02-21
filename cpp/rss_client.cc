/** \file rss_client.cc
 *  \brief Downloads and evaluates RSS updates.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Downloader.h"
#include "SyndicationFormat.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " [--verbose] url1 [url2 ... urlN]\n";
    std::exit(EXIT_FAILURE);
}


void ProcessSyndicationURL(const bool verbose, const std::string &url) {
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
        const time_t publication_date(item.getPubDate());
        if (publication_date != TimeUtil::BAD_TIME_T)
            std::cout << "\t\tDate: " << TimeUtil::TimeTToString(publication_date) << '\n';
        const std::unordered_map<std::string, std::string> &dc_and_prism_data(item.getDCAndPrismData());
        for (const auto &key_and_value : dc_and_prism_data)
            std::cout << "\t\t" << key_and_value.first << ": " << key_and_value.second << '\n';
    }
}


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
        for (int arg_no(1); arg_no < argc; ++arg_no)
            ProcessSyndicationURL(verbose, argv[arg_no]);
    } catch (const std::exception &x) {
        ERROR("caught exception: " + std::string(x.what()));
    }

}
