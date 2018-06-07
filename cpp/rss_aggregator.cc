/** \file   rss_aggregator.cc
 *  \brief  Downloads and aggregates RSS feeds.
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
#include "SyndicationFormat.h"
#include "util.h"


namespace {


volatile sig_atomic_t sigterm_seen = false;


void SigTermHandler(int /* signum */) {
    sigterm_seen = true;
}


volatile sig_atomic_t sighup_seen = false;


void SigHupHandler(int /* signum */) {
    sighup_seen = true;
}


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbosity=min_verbosity] [--test] xml_output_path\n"
              << "       When --test has been specified no data will be stored.\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/rss_aggregator.conf");


std::unordered_map<std::string, uint64_t> section_name_to_ticks_map;


void ProcessSection(const bool /*test*/, const IniFile::Section &section, Downloader * const downloader, DbConnection * const /*db_connection*/,
                    const unsigned default_downloader_time_limit, const unsigned default_poll_interval, const uint64_t now)
{
    const std::string feed_url(section.getString("feed_url"));
    const unsigned poll_interval(section.getUnsigned("poll_interval", default_poll_interval));
    const unsigned downloader_time_limit(section.getUnsigned("downloader_time_limit", default_downloader_time_limit) * 1000);

    const std::string &section_name(section.getSectionName());
    if (now > 0) {
        const auto section_name_and_ticks(section_name_to_ticks_map.find(section_name));
        if (unlikely(section_name_and_ticks == section_name_to_ticks_map.end()))
            LOG_ERROR("unexpected: did not find \"" + section_name + "\" in our map!");
        if (section_name_and_ticks->second + poll_interval < now) {
            LOG_DEBUG(section_name + ": not yet time to do work, last work was done at " + std::to_string(section_name_and_ticks->second)
                      + ".");
            return;
        }
    }

    if (not downloader->newUrl(feed_url, downloader_time_limit))
        LOG_WARNING(section_name + ": failed to download the feed: " + downloader->getLastErrorMessage());
    else {
        std::string error_message;
        std::unique_ptr<SyndicationFormat> syndication_format(SyndicationFormat::Factory(downloader->getMessageBody(), &error_message));
        if (unlikely(syndication_format == nullptr))
            LOG_WARNING("failed to parse feed: " + error_message);
        else {
            for (const auto &item : *syndication_format) {
                (void)item;
            }
        }
    }

    section_name_to_ticks_map[section_name] = now;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    bool test(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--test") == 0) {
            test = true;
            --argc, ++argv;
        } else
            Usage();
    }

    IniFile ini_file(CONF_FILE_PATH);
    DbConnection db_connection(ini_file);

    const unsigned DEFAULT_POLL_INTERVAL(ini_file.getUnsigned("", "default_poll_interval"));
    const unsigned DEFAULT_DOWNLOADER_TIME_LIMIT(ini_file.getUnsigned("", "default_downloader_time_limit"));
    const unsigned UPDATE_INTERVAL(ini_file.getUnsigned("", "update_interval"));

    if (not test) {
        struct sigaction new_action;
        new_action.sa_handler = SigTermHandler;
        sigemptyset(&new_action.sa_mask);
        new_action.sa_flags = 0;
        if (::sigaction(SIGTERM, &new_action, nullptr) != 0)
            LOG_ERROR("sigaction(2) failed! (1)");

        new_action.sa_handler = SigHupHandler;
        sigemptyset(&new_action.sa_mask);
        new_action.sa_flags = 0;
        if (::sigaction(SIGHUP, &new_action, nullptr) != 0)
            LOG_ERROR("sigaction(2) failed! (1)");

        if (::daemon(0, 1 /* do not close file descriptors and redirect to /dev/null */) != 0)
            LOG_ERROR("we failed to deamonize our process!");
    }

    uint64_t ticks(0);
    Downloader downloader;
    for (;;) {
        LOG_DEBUG("now we're at " + std::to_string(ticks) + ".");

        const time_t before(std::time(nullptr));

        std::unordered_set<std::string> already_seen_sections;
        for (const auto &section : ini_file) {
            if (sigterm_seen) {
                LOG_INFO("caught SIGTERM, shutting down...");
                return EXIT_SUCCESS;
            }

            if (sighup_seen) {
                LOG_INFO("caught SIGHUB, rereading config file...");
                ini_file.reload();
                sighup_seen = false;
            }

            sigset_t signal_set;
            sigaddset(&signal_set, SIGTERM);
            sigaddset(&signal_set, SIGHUP);
            if (unlikely(::sigprocmask(SIG_BLOCK, &signal_set, nullptr) != 0))
                LOG_ERROR("failed to block SIGTERM and SIGHUP!");

            const std::string &section_name(section.first);
            if (not section_name.empty()) {
                if (unlikely(already_seen_sections.find(section_name) != already_seen_sections.end()))
                    LOG_ERROR("duplicate section: \"" + section_name + "\"!");
                already_seen_sections.emplace(section_name);

                LOG_INFO("Processing section \"" + section_name + "\".");
                ProcessSection(test, section.second, &downloader, &db_connection, DEFAULT_DOWNLOADER_TIME_LIMIT, DEFAULT_POLL_INTERVAL,
                               ticks);
            }

            if (unlikely(::sigprocmask(SIG_UNBLOCK, &signal_set, nullptr) != 0))
                LOG_ERROR("failed to unblock SIGTERM and SIGHUP!");
        }

        if (test) // -> only run through our loop once
            return EXIT_SUCCESS;

        const time_t after(std::time(nullptr));

        uint64_t sleep_interval;
        if (after - before > UPDATE_INTERVAL * 60)
            sleep_interval = 0;
        else
            sleep_interval = (UPDATE_INTERVAL * 60 - (after - before));

        ::sleep(static_cast<unsigned>(sleep_interval));
        ticks += UPDATE_INTERVAL;
    }
}
