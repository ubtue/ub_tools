/** \file    ztracker_tool.cc
    \brief   A utility to inspect and manipulate our Zotero tracker database.
    \author  Dr. Johannes Ruscheinski

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
#include "DbConnection.h"
#include "TimeUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " delivery_mode command\n"
              << "       Possible commands are:\n"
              << "       clear [url|zulu_timestamp]        => if no arguments are provided, this empties the entire database\n"
              << "                                            if a URL has been provided, just the entry with key \"url\"\n"
              << "                                            will be erased, and if a Zulu (ISO 8601) timestamp has been\n"
              << "                                            provided, all entries that are not newer are erased.\n"
              << "       insert url journal [error_message] => inserts or replaces the entry for \"url\".\n"
              << "       lookup url                         => displays the timestamp and, if found, the optional message\n"
              << "                                             for this URL.\n"
              << "       list [pcre]                        => list either all entries in the database or, if the PCRE has\n"
              << "                                             been provided, ony the ones with matching URL\'s.\n"
              << "       is_present url                     => prints either \"true\" or \"false\".\n\n";
    std::exit(EXIT_FAILURE);
}


void Clear(Zotero::DownloadTracker * const download_tracker, Zotero::DeliveryMode delivery_mode, const std::string &url_or_zulu_timestamp) {
    time_t timestamp;
    std::string err_msg;

    if (url_or_zulu_timestamp.empty()) {
        std::cout << "Deleted " << download_tracker->clear(delivery_mode) << " entries from the tracker database.\n";
    } else if (TimeUtil::Iso8601StringToTimeT(url_or_zulu_timestamp, &timestamp, &err_msg))
        std::cout << "Deleted " << download_tracker->deleteOldEntries(delivery_mode, timestamp) << " entries from the tracker database.\n";
    else { // Assume url_or_zulu_timestamp contains a URL.
        if (download_tracker->deleteSingleEntry(delivery_mode, url_or_zulu_timestamp))
            std::cout << "Deleted one entry from the tracker database.\n";
        else
            std::cerr << "Entry for URL \"" << url_or_zulu_timestamp << "\" could not be deleted!\n";
    }
}


void Insert(Zotero::DownloadTracker * const download_tracker, Zotero::DeliveryMode delivery_mode, const std::string &url,
            const std::string &journal_name, const std::string &optional_message)
{
    download_tracker->addOrReplace(delivery_mode, url, journal_name, optional_message, (optional_message.empty() ? "*bogus hash*" : ""));
    std::cout << "Created an entry for the URL \"" << url << "\".\n";
}


void Lookup(Zotero::DownloadTracker * const download_tracker, Zotero::DeliveryMode delivery_mode, const std::string &url) {
    Zotero::DownloadTracker::Entry entry;
    if (not download_tracker->hasAlreadyBeenDownloaded(delivery_mode, url, /* hash = */"", &entry))
        std::cerr << "Entry for URL \"" << url << "\" could not be found!\n";
    else {
        if (entry.error_message_.empty())
            std::cout << url << ": " << TimeUtil::TimeTToLocalTimeString(entry.last_harvest_time_) << '\n';
        else
            std::cout << url << ": " << TimeUtil::TimeTToLocalTimeString(entry.last_harvest_time_) << " (" << entry.error_message_ << ")\n";
    }
}


void List(Zotero::DownloadTracker * const download_tracker, Zotero::DeliveryMode delivery_mode, const std::string &pcre) {
    std::vector<Zotero::DownloadTracker::Entry> entries;
    download_tracker->listMatches(delivery_mode, pcre, &entries);

    for (const auto &entry : entries) {
        std::cout << entry.url_ << ": " << TimeUtil::TimeTToLocalTimeString(entry.last_harvest_time_);
        if (not entry.error_message_.empty())
            std::cout << ", " << entry.error_message_;
        std::cout << '\n';
    }
}


void IsPresent(Zotero::DownloadTracker * const download_tracker, Zotero::DeliveryMode delivery_mode, const std::string &url) {
    std::cout << (download_tracker->hasAlreadyBeenDownloaded(delivery_mode, url) ? "true\n" : "false\n");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    std::unique_ptr<DbConnection> db_connection(new DbConnection);
    Zotero::DownloadTracker download_tracker(db_connection.get());

    const std::string delivery_mode_string(StringUtil::ToUpper(argv[1]));
    Zotero::DeliveryMode delivery_mode(Zotero::DeliveryMode::NONE);

    if (Zotero::STRING_TO_DELIVERY_MODE_MAP.find(delivery_mode_string) == Zotero::STRING_TO_DELIVERY_MODE_MAP.end())
        LOG_ERROR("Unknown delivery mode '" + std::string(delivery_mode_string) + "'");
    else
        delivery_mode = static_cast<Zotero::DeliveryMode>(Zotero::STRING_TO_DELIVERY_MODE_MAP.at(delivery_mode_string));

    --argc, ++argv;

    if (argc < 2 or argc > 4)
        Usage();

    if (std::strcmp(argv[1], "clear") == 0) {
        if (argc > 3)
            LOG_ERROR("clear takes 0 or 1 arguments!");
        Clear(&download_tracker, delivery_mode, argc == 2 ? "" : argv[2]);
    } else if (std::strcmp(argv[1], "insert") == 0) {
        if (argc < 4 or argc > 5)
            LOG_ERROR("insert takes 2 or 3 arguments!");
        Insert(&download_tracker, delivery_mode, argv[2], argv[3], argc == 4 ? "" : argv[4]);
    } else if (std::strcmp(argv[1], "lookup") == 0) {
        if (argc != 3)
            LOG_ERROR("lookup takes 1 argument!");
        Lookup(&download_tracker, delivery_mode, argv[2]);
    } else if (std::strcmp(argv[1], "list") == 0) {
        if (argc > 3)
            LOG_ERROR("list takes 0 or 1 arguments!");
        List(&download_tracker, delivery_mode, argc == 2 ? ".*" : argv[2]);
    } else if (std::strcmp(argv[1], "is_present") == 0) {
        if (argc != 3)
            LOG_ERROR("is_present takes 1 argument!");
        IsPresent(&download_tracker, delivery_mode, argv[2]);
    } else
        LOG_ERROR("unknown command: \"" + std::string(argv[1]) + "\"!");

    return EXIT_SUCCESS;
}
