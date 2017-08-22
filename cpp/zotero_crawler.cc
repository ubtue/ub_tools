/** \file    zotero_crawler.cc
 *  \brief   Identifies URL's that we can send to a Zotero Translation Server.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2017 Universitätsbibliothek Tübingen
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
#include <unordered_set>
#include <utility>
#include <cerrno>
#include <cstring>
#include <getopt.h>
#include "FileUtil.h"
#include "PageFetcher.h"
#include "RegexMatcher.h"
#include "HttpHeader.h"
#include "StringUtil.h"
#include "Url.h"
#include "util.h"
#include "WebUtil.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << "[options] config_file\n"
              << "\t[ (--last-header | -l) ]\n"
              << "\t[ (--all-headers | -a) ]\n"
              << "\t[ (--ignore-robots-dot-txt | -i) ]                        Nomen est omen.\n"
              << "\t[ (--acceptable-languages | -A) ] language_code_or_codes  Please note that if you specify more\n"
              << "                                                            than a single 2-letter language code,\n"
              << "                                                            you must separate the individual\n"
              << "                                                            codes with commas.\n"
              << "\t[ (--print-redirects | -p) ]                              Nomen est omen.\n"
              << "\t[ (--timeout | -t) milliseconds ]                         Overall time we're willinbg to wait\n"
              << "                                                            to download a page.\n\n"
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
                const std::string &acceptable_languages, unsigned remaining_crawl_depth,
                const RegexMatcher &url_regex_matcher, std::unordered_set<std::string> * const extracted_urls)
{
    const PageFetcher::RobotsDotTxtOption robots_dot_txt_option(
        ignore_robots_dot_txt ? PageFetcher::IGNORE_ROBOTS_DOT_TXT : PageFetcher::CONSULT_ROBOTS_DOT_TXT);
    PageFetcher page_fetcher(url, /* additional_http_headers = */ "", timeout, /* max_redirects = */ 7,
                             /* ignore_redirect_errors = */ false, /* transparently_unzip_content = */true,
                             USER_AGENT, acceptable_languages, robots_dot_txt_option);
    if (page_fetcher.anErrorOccurred()) {
        Warning("in ProcessURL: Failed to retrieve a Web page (" + url + "): " + page_fetcher.getErrorMsg());
        return;
    }

    std::string message_headers, message_body;
    if (not PageFetcher::SplitHttpHeadersFromBody(page_fetcher.getData(), &message_headers, &message_body))
        Error("in ProcessURL: Failed to separate message headers and message body!");

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
                       print_redirects, timeout, acceptable_languages, remaining_crawl_depth, url_regex_matcher,
                       extracted_urls);
    }
}


static struct option options[] = {
    { "help",                  no_argument,        nullptr, 'h'  },
    { "all-headers",           no_argument,        nullptr, 'a'  },
    { "last-header",           no_argument,        nullptr, 'l'  },
    { "timeout",               required_argument,  nullptr, 't'  },
    { "ignore-robots-dot-txt", no_argument,        nullptr, 'i'  },
    { "print-redirects",       no_argument,        nullptr, 'p'  },
    { "acceptable-languages",  required_argument,  nullptr, 'A'  },
    { nullptr,                 no_argument,        nullptr, '\0' }
};


const unsigned DEFAULT_TIMEOUT(5000); // milliseconds


void ProcessArgs(int argc, char *argv[], bool * const all_headers, bool * const last_header,
                 unsigned * const timeout, bool * const ignore_robots_dot_txt, bool * const print_redirects,
                 std::string * const acceptable_languages, std::string * const config_filename)
{
    // Defaults:
    *all_headers           = false;
    *last_header           = false;
    *timeout               = DEFAULT_TIMEOUT;
    *ignore_robots_dot_txt = false;
    *print_redirects       = false;
    acceptable_languages->clear();

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
        case 't':
            errno = 0;
            char *endptr;
            *timeout = std::strtoul(optarg, &endptr, 10);
            if (errno != 0 or *endptr != '\0' or *timeout == 0) {
                std::cerr << ::progname << " invalid timeout \"" << optarg << "\"!\n";
                Usage();
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
            Error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \"" + input->getPath()
                  + "\"!");

        unsigned max_crawl_depth;
        if (not StringUtil::ToUnsigned(line_parts[1], &max_crawl_depth))
            Error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \"" + input->getPath()
                  + "\"! (Invalid max. crawl depth: \"" + line_parts[1] + "\")");

        std::string err_msg;
        RegexMatcher * const url_regex_matcher(RegexMatcher::RegexMatcherFactory(line_parts[2], &err_msg));
        if (url_regex_matcher == nullptr)
            Error("in ParseConfigFile: bad input line #" + std::to_string(line_no) + " in \"" + input->getPath()
                  + "\", regex is faulty! (" + err_msg + ")");
        site_descs->emplace_back(line_parts[0], max_crawl_depth, url_regex_matcher);
    }
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        bool all_headers, last_header, ignore_robots_dot_txt, print_redirects;
        unsigned timeout;
        std::string acceptable_languages, config_filename;
        ProcessArgs(argc, argv, &all_headers, &last_header, &timeout, &ignore_robots_dot_txt, &print_redirects,
                    &acceptable_languages, &config_filename);

        std::unique_ptr<File> config_file(FileUtil::OpenInputFileOrDie(config_filename));
        std::vector<SiteDesc> site_descs;
        ParseConfigFile(config_file.get(), &site_descs);

        for (const auto &site_desc : site_descs) {
            std::unordered_set<std::string> extracted_urls;
            ProcessURL(site_desc.start_url_, all_headers, last_header, ignore_robots_dot_txt, print_redirects,
                       timeout, acceptable_languages, site_desc.max_crawl_depth_, *site_desc.url_regex_matcher_,
                       &extracted_urls);

            for (const auto &extracted_url : extracted_urls)
                std::cout << extracted_url << '\n';
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
