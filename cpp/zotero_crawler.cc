/** \file    zotero_crawler.cc
    \brief   Identifies URL's that we can send to a Zotero Translation Server.
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
#include "Downloader.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "WebUtil.h"


namespace {

// Default values in milliseconds
const unsigned DEFAULT_TIMEOUT(5000);
const unsigned DEFAULT_MIN_URL_PROCESSING_TIME(200);


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
              << "                                                            to download a page (default " + StringUtil::ToString(DEFAULT_TIMEOUT) + ").\n"
              << "\t[ (--min-url-processing-time | -m) milliseconds ]         Min time between downloading 2 URLs\n"
              << "                                                            to prevent DOS attacks (default " + StringUtil::ToString(DEFAULT_MIN_URL_PROCESSING_TIME) + ").\n"
              << "\n"
              << "The config file consists of lines specifying one site per line.\n"
              << "Each line must have a start URL, a maximum crawl depth and a PCRE URL pattern.\n"
              << "Any encountered URL that matches a URL pattern will be echoed on stdout.\n\n";

    std::exit(EXIT_FAILURE);
}


const std::string USER_AGENT("ub_tools (htts://ixtheo.de/docs/user_agents)");


void ExtractLocationUrls(const std::string &header_blob, std::list<std::string> * const location_urls) {
    location_urls->clear();

    std::vector<std::string> header_lines;
    StringUtil::SplitThenTrimWhite(header_blob, "\r\n", &header_lines);
    for (const auto header_line : header_lines) {
        if (StringUtil::StartsWith(header_line, "Location:", /* ignore_case = */ true)) {
            std::string location(header_line.substr(9));
            StringUtil::Trim(&location);
            if (not location.empty())
                location_urls->push_back(location);
        }
    }
}


void ProcessURL(const std::string &url, const bool all_headers, const bool last_header,
                const bool ignore_robots_dot_txt, const bool print_redirects, const unsigned timeout,
                TimeLimit * const min_url_processing_time,
                const std::string &acceptable_languages, unsigned remaining_crawl_depth,
                const RegexMatcher &url_regex_matcher, std::unordered_set<std::string> * const extracted_urls)
{
    Downloader::Params params;
    params.user_agent_            = USER_AGENT;
    params.acceptable_languages_  = acceptable_languages;
    params.honour_robots_dot_txt_ = not ignore_robots_dot_txt;
    min_url_processing_time->sleepUntilExpired();
    Downloader downloader(url, params, timeout);
    min_url_processing_time->restart();
    if (downloader.anErrorOccurred()) {
        logger->warning("in ProcessURL: Failed to retrieve a Web page (" + url + "): "
                        + downloader.getLastErrorMessage());
        return;
    }

    const std::string message_headers(downloader.getMessageHeader()), message_body(downloader.getMessageBody());
    if (print_redirects) {
        std::list<std::string> location_urls;
        ExtractLocationUrls(message_headers, &location_urls);
        for (const auto &location_url : location_urls)
            std::cout << "Location: " << location_url << '\n';
        std::cout << "\n\n";
    }

    if (all_headers or last_header) {
        if (all_headers or last_header)
            std::cout << StringUtil::ReplaceString("\r\n", "\n", message_headers) << '\n';
    }

    static const unsigned EXTRACT_URL_FLAGS(WebUtil::IGNORE_DUPLICATE_URLS | WebUtil::IGNORE_LINKS_IN_IMG_TAGS
                                            | WebUtil::REMOVE_DOCUMENT_RELATIVE_ANCHORS
                                            | WebUtil::CLEAN_UP_ANCHOR_TEXT
                                            | WebUtil::KEEP_LINKS_TO_SAME_MAJOR_SITE_ONLY
                                            | WebUtil::ATTEMPT_TO_EXTRACT_JAVASCRIPT_URLS);

    std::vector<WebUtil::UrlAndAnchorTexts> urls_and_anchor_texts;
    WebUtil::ExtractURLs(message_body, url, WebUtil::ABSOLUTE_URLS, &urls_and_anchor_texts, EXTRACT_URL_FLAGS);
    for (const auto &url_and_anchor_texts : urls_and_anchor_texts) {
        const std::string extracted_url(url_and_anchor_texts.getUrl());
        if (url_regex_matcher.matched(extracted_url))
            extracted_urls->emplace(extracted_url);
    }

    --remaining_crawl_depth;
    if (remaining_crawl_depth > 0) {
        for (const auto &url_and_anchor_texts : urls_and_anchor_texts)
            ProcessURL(url_and_anchor_texts.getUrl(), all_headers, last_header, ignore_robots_dot_txt,
                       print_redirects, timeout, min_url_processing_time, acceptable_languages, remaining_crawl_depth,
                       url_regex_matcher, extracted_urls);
    }
}


static struct option options[] = {
    { "help",                    no_argument,              nullptr, 'h'  },
    { "min-log-level",           required_argument,        nullptr, 'L'  },
    { "all-headers",             no_argument,              nullptr, 'a'  },
    { "last-header",             no_argument,              nullptr, 'l'  },
    { "timeout",                 required_argument,        nullptr, 't'  },
    { "min-url-processing-time", required_argument,        nullptr, 'm'  },
    { "ignore-robots-dot-txt",   no_argument,              nullptr, 'i'  },
    { "print-redirects",         no_argument,              nullptr, 'p'  },
    { "acceptable-languages",    required_argument,        nullptr, 'A'  },
    { nullptr,                   no_argument,              nullptr, '\0' }
};


Logger::LogLevel StringToLogLevel(const std::string &level_candidate) {
    if (level_candidate == "ERROR")
        return Logger::LL_ERROR;
    if (level_candidate == "WARNING")
        return Logger::LL_WARNING;
    if (level_candidate == "INFO")
        return Logger::LL_INFO;
    if (level_candidate == "DEBUG")
        return Logger::LL_DEBUG;
    ERROR("not a valid minimum log level: \"" + level_candidate + "\"! (Use ERROR, WARNING, INFO or DEBUG)");
};


void ProcessArgs(int argc, char *argv[], Logger::LogLevel *  const min_log_level, bool * const all_headers,
                 bool * const last_header, unsigned * const timeout, TimeLimit * const min_url_processing_timer,
                 bool * const ignore_robots_dot_txt, bool * const print_redirects,
                 std::string * const acceptable_languages, std::string * const config_filename)
{
    // Defaults:
    *min_log_level                   = Logger::LL_DEBUG;
    *all_headers                     = false;
    *last_header                     = false;
    *timeout                         = DEFAULT_TIMEOUT;
    unsigned min_url_processing_time = DEFAULT_MIN_URL_PROCESSING_TIME;
    *ignore_robots_dot_txt           = false;
    *print_redirects                 = false;
    acceptable_languages->clear();

    for (;;) {
        char *endptr;
        int option_index = 0;
        int option = ::getopt_long(argc, argv, "ahlqt:ipA:", options, &option_index);
        if (option == -1)
            break;
        switch (option) {
        case 'L':
            *min_log_level = StringToLogLevel(optarg);
            break;
        case 'a':
            *all_headers = true;
            break;
        case 'l':
            *last_header = true;
            break;
        case 't':
            errno = 0;
            *timeout = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or *timeout == 0) {
                std::cerr << ::progname << " invalid timeout \"" << optarg << "\"!\n";
                Usage();
            }
            break;
        case 'm':
            errno = 0;
            min_url_processing_time = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or min_url_processing_time == 0) {
                std::cerr << ::progname << " invalid min_url_processing_time \"" << optarg << "\"!\n";
                Usage();
            } else
                *min_url_processing_timer = min_url_processing_time;
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
            Usage();
        }
    }

    if (optind != argc - 1)
        Usage();
    *config_filename = argv[optind];
}


struct SiteDesc {
    std::string start_url_;
    unsigned max_crawl_depth_;
    RegexMatcher *url_regex_matcher_;
public:
    SiteDesc(const std::string &start_url, const unsigned max_crawl_depth, RegexMatcher * const url_regex_matcher)
        : start_url_(start_url), max_crawl_depth_(max_crawl_depth), url_regex_matcher_(url_regex_matcher) { }
};


void ParseConfigFile(File * const input, std::vector<SiteDesc> * const site_descs) {
    unsigned line_no(0);
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        ++line_no;
        const std::string::size_type hash_pos(line.find('#'));
        if (hash_pos != std::string::npos)
            line = line.substr(0, hash_pos);
        StringUtil::Trim(&line);
        if (line.empty())
            continue;

        std::vector<std::string> line_parts;
        if (StringUtil::SplitThenTrimWhite(line, ' ', &line_parts) != 3)
            logger->error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \""
                          + input->getPath() + "\"!");

        unsigned max_crawl_depth;
        if (not StringUtil::ToUnsigned(line_parts[1], &max_crawl_depth))
            logger->error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \""
                          + input->getPath() + "\"! (Invalid max. crawl depth: \"" + line_parts[1] + "\")");

        std::string err_msg;
        RegexMatcher * const url_regex_matcher(RegexMatcher::RegexMatcherFactory(line_parts[2], &err_msg));
        if (url_regex_matcher == nullptr)
            logger->error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \""
                          + input->getPath() + "\", regex is faulty! (" + err_msg + ")");
        site_descs->emplace_back(line_parts[0], max_crawl_depth, url_regex_matcher);
    }
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        Logger::LogLevel min_log_level;
        bool all_headers, last_header, ignore_robots_dot_txt, print_redirects;
        unsigned timeout;
        TimeLimit min_url_processing_time(DEFAULT_MIN_URL_PROCESSING_TIME);
        std::string acceptable_languages, config_filename;
        ProcessArgs(argc, argv, &min_log_level, &all_headers, &last_header, &timeout, &min_url_processing_time,
                    &ignore_robots_dot_txt, &print_redirects, &acceptable_languages, &config_filename);
        logger->setMinimumLogLevel(min_log_level);

        std::unique_ptr<File> config_file(FileUtil::OpenInputFileOrDie(config_filename));
        std::vector<SiteDesc> site_descs;
        ParseConfigFile(config_file.get(), &site_descs);

        for (const auto &site_desc : site_descs) {
            std::unordered_set<std::string> extracted_urls;
            ProcessURL(site_desc.start_url_, all_headers, last_header, ignore_robots_dot_txt, print_redirects,
                       timeout, &min_url_processing_time, acceptable_languages, site_desc.max_crawl_depth_,
                       *site_desc.url_regex_matcher_, &extracted_urls);

            for (const auto &extracted_url : extracted_urls)
                std::cout << extracted_url << '\n';
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
