/** \file    simple_crawler.cc
    \brief   Identifies URL's that we can use for further processing..
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
#include <unordered_set>
#include <utility>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include "SimpleCrawler.h"
#include "util.h"


namespace {


void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file\n"
              << "\t[ (--min-log-level | -L) level]                           default is INFO.\n"
              << "\t[ (--last-header | -l) ]\n"
              << "\t[ (--all-headers | -a) ]\n"
              << "\t[ (--ignore-robots-dot-txt | -i) ]                        Nomen est omen.\n"
              << "\t[ (--acceptable-languages | -A) ] language_code_or_codes  Please note that if you specify more\n"
              << "                                                            than a single 2-letter language code,\n"
              << "                                                            you must separate the individual\n"
              << "                                                            codes with commas.\n"
              << "\t[ (--print-redirects | -p) ]                              Nomen est omen.\n"
              << "\t[ (--timeout | -t) milliseconds ]                         Overall time we're willing to wait\n"
              << "                                                            to download a page (default "
                     + std::to_string(SimpleCrawler::DEFAULT_TIMEOUT) + ").\n"
              << "\t[ (--min-url-processing-time | -m) milliseconds ]         Min time between downloading 2 URL's\n"
              << "                                                            to prevent accidental DOS attacks (default "
                     + std::to_string(SimpleCrawler::DEFAULT_MIN_URL_PROCESSING_TIME) + ").\n"
              << "\n"
              << "The config file consists of lines specifying one site per line.\n"
              << "Each line must have a start URL, a maximum crawl depth and a PCRE URL pattern, that each sub-url must match.\n"
              << "Any encountered URL that matches a URL pattern will be echoed on stdout.\n\n";

    std::exit(EXIT_FAILURE);
}


static struct option options[] = { { "help", no_argument, nullptr, 'h' },
                                   { "min-log-level", required_argument, nullptr, 'L' },
                                   { "all-headers", no_argument, nullptr, 'a' },
                                   { "last-header", no_argument, nullptr, 'l' },
                                   { "timeout", required_argument, nullptr, 't' },
                                   { "min-url-processing-time", required_argument, nullptr, 'm' },
                                   { "ignore-robots-dot-txt", no_argument, nullptr, 'i' },
                                   { "print-redirects", no_argument, nullptr, 'p' },
                                   { "acceptable-languages", required_argument, nullptr, 'A' },
                                   { nullptr, no_argument, nullptr, '\0' } };


void ProcessArgs(int argc, char *argv[], Logger::LogLevel * const min_log_level, std::string * const config_filename,
                 SimpleCrawler::Params *params) {
    // Defaults:
    *min_log_level = Logger::LL_DEBUG;
    unsigned min_url_processing_time;
    unsigned timeout;

    for (;;) {
        char *endptr;
        int option_index = 0;
        int option = ::getopt_long(argc, argv, "ahlqt:ipA:", options, &option_index);
        if (option == -1)
            break;
        switch (option) {
        case 'L':
            *min_log_level = Logger::StringToLogLevel(optarg);
            break;
        case 'a':
            params->print_all_http_headers_ = true;
            break;
        case 'l':
            params->print_last_http_header_ = true;
            break;
        case 't':
            errno = 0;
            timeout = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or timeout == 0) {
                std::cerr << ::progname << " invalid timeout \"" << optarg << "\"!\n";
                Usage();
            }
            params->timeout_ = timeout;
            break;
        case 'm':
            errno = 0;
            min_url_processing_time = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or min_url_processing_time == 0) {
                std::cerr << ::progname << " invalid min_url_processing_time \"" << optarg << "\"!\n";
                Usage();
            }
            params->min_url_processing_time_ = min_url_processing_time;
            break;
        case 'i':
            params->ignore_robots_dot_txt_ = true;
            break;
        case 'p':
            params->print_redirects_ = true;
            break;
        case 'A':
            params->acceptable_languages_ = optarg;
            break;
        default:
            Usage();
        }
    }

    if (optind != argc - 1)
        Usage();
    *config_filename = argv[optind];
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        Logger::LogLevel min_log_level;
        std::string config_filename;
        SimpleCrawler::Params params;
        ProcessArgs(argc, argv, &min_log_level, &config_filename, &params);

        logger->setMinimumLogLevel(min_log_level);
        std::vector<std::string> extracted_urls;
        SimpleCrawler::ProcessSites(config_filename, params, &extracted_urls);
        for (const auto &extracted_url : extracted_urls)
            std::cout << extracted_url << '\n';

        return EXIT_SUCCESS;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
