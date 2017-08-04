/** \file    CachedPageFetcherTest.cc
 *  \brief   Tests some aspects of the CachedPageFetcher class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2002-2009 Project iVia.
 *  \copyright 2002-2009 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include "CachedPageFetcher.h"
#include "HttpHeader.h"
#include "StringUtil.h"
#include "Url.h"
#include "util.h"


const std::string USER_AGENT("iVia/5.0 CachedPageFetcherRedirectTest (http://infomine.ucr.edu/iVia/user_agents)");


void ExtractLocationUrls(const std::vector<std::string> &headers, std::list<std::string> * const location_urls) {
    location_urls->clear();

    for (std::vector<std::string>::const_iterator header(headers.begin()); header != headers.end(); ++header) {
        const HttpHeader http_header(*header);
        const std::string location(http_header.getLocation());
        if (not location.empty())
            location_urls->push_back(location);
    }
}


void ProcessURL(const std::string &url, const bool all_headers, const bool last_header, const bool quiet,
                const bool ignore_robots_dot_txt, const bool print_redirects, const unsigned timeout,
                const std::string &acceptable_languages)
{
    CachedPageFetcher::Params params;
    if (ignore_robots_dot_txt)
        params.robots_dot_txt_option_ = CachedPageFetcher::IGNORE_ROBOTS_DOT_TXT;
    params.user_agent_ = USER_AGENT;
    params.acceptable_languages_ = acceptable_languages;
    CachedPageFetcher cached_page_fetcher(url, timeout, params);

    if (cached_page_fetcher.anErrorOccurred())
        Error("CachedPageFetcher error: " + cached_page_fetcher.getErrorMsg());

    std::vector<std::string> headers;
    if (print_redirects) {
        cached_page_fetcher.getMessageHeaders(&headers);
        std::list<std::string> location_urls;
        ExtractLocationUrls(headers, &location_urls);
        for (const auto &location_url : location_urls)
            std::cout << "Location: " << location_url << '\n';
        std::cout << "\n\n";
    }

    if (all_headers or last_header) {
        if (not headers.empty())
            cached_page_fetcher.getMessageHeaders(&headers);

        // Get all the headers as a string:
        std::string all_headers_str;
        StringUtil::Join(headers, "\n\n", &all_headers_str);
        // Get the last header as a string:
        std::string header_str = cached_page_fetcher.getMessageHeader();
        HttpHeader header(header_str);

        if (not quiet and (all_headers or last_header))
            std::cout << (all_headers ? all_headers_str : header_str) << '\n';
    } else if (not quiet)
        std::cout << cached_page_fetcher.getMessageBody() << '\n';
}


void PrintUsage() {
    std::cerr << "usage: " << ::progname << "[options] [URL]\n"
              << "\t[ (--last-header | -l) ]\n"
              << "\t[ (--all-headers | -a) ]\n"
              << "\t[ (--quiet | -q) ]\n"
              << "\t[ (--ignore-robots-dot-txt | -i) ]                        Nomen est omen.\n"
              << "\t[ (--acceptable-languages | -A) ] language_code_or_codes  Please note that if you specify more\n"
              << "                                                            than a single 2-letter language code,\n"
              << "                                                            you must separate the individual\n"
              << "                                                            codes with commas.\n"
              << "\t[ (--print-redirects | -p) ]                              Nomen est omen.\n"
              << "\t[ (--timeout | -t) milliseconds ]                         Overall time we're willinbg to wait\n"
              << "                                                            to download a page.\n\n";
    std::cerr << "If no URL is specified the program repeatedly prompts for URLs from STDIN.\n\n";

    std::exit(EXIT_FAILURE);
}


static struct option options[] = {
    { "help",                  no_argument,        nullptr, 'h'  },
    { "all-headers",           no_argument,        nullptr, 'a'  },
    { "last-header",           no_argument,        nullptr, 'l'  },
    { "quiet",                 no_argument,        nullptr, 'q'  },
    { "timeout",               required_argument,  nullptr, 't'  },
    { "ignore-robots-dot-txt", no_argument,        nullptr, 'i'  },
    { "print-redirects",       no_argument,        nullptr, 'p'  },
    { "acceptable-languages",  required_argument,  nullptr, 'A'  },
    { nullptr,                 no_argument,        nullptr, '\0' }
};


void ProcessArgs(int argc, char *argv[], bool * const all_headers, bool * const last_header, bool * const quiet,
                 unsigned * const timeout, bool * const ignore_robots_dot_txt, bool * const print_redirects,
                 std::string * const acceptable_languages, std::string * const url)
{
    // Defaults:
    *all_headers           = false;
    *last_header           = false;
    *quiet	               = false;
    *timeout               = CachedPageFetcher::DEFAULT_TIMEOUT;
    *ignore_robots_dot_txt = false;
    *print_redirects       = false;
    acceptable_languages->clear();
    url->clear();

    for (;;) {
        int option_index = 0;
        int option = ::getopt_long(argc, argv, "ahlqt:ipA:", options, &option_index);
        if (option == -1)
            break;
        switch (option) {
        case 'a':
            *all_headers = true;
            break;
        case 'l':
            *last_header = true;
            break;
        case 'q':
            *quiet = true;
            break;
        case 't':
            errno = 0;
            char *endptr;
            *timeout = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or *timeout == 0) {
                std::cerr << ::progname << " invalid timeout \"" << optarg << "\"!\n";
                PrintUsage();
            }
            break;
        case 'i':
            *ignore_robots_dot_txt = true;
            break;
        case 'p':
            *print_redirects = true;
            break;
        case 'A':
            *acceptable_languages = optarg;
            break;
        default:
            PrintUsage();
        }
    }

    if (optind < argc - 1)
        PrintUsage();
    else if (optind == argc - 1)
        *url = argv[optind];
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        bool all_headers, last_header, quiet, ignore_robots_dot_txt, print_redirects;
        unsigned timeout;
        std::string acceptable_languages, url;
        ProcessArgs(argc, argv, &all_headers, &last_header, &quiet, &timeout, &ignore_robots_dot_txt,
                    &print_redirects, &acceptable_languages, &url);

        if (not url.empty())
            ProcessURL(url, all_headers, last_header, quiet, ignore_robots_dot_txt, print_redirects, timeout,
                       acceptable_languages);
        else {
            for (;;) {
                std::cout << "url>";
                std::getline(std::cin, url);
                if (url.empty())
                    break;
                ProcessURL(url, all_headers, last_header, quiet, ignore_robots_dot_txt, print_redirects, timeout,
                           acceptable_languages);
            }
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
