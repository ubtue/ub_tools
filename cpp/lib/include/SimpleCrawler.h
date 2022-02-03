/** \file   SimpleCrawler.h
 *  \brief  Identifies URL's that we can use for further processing.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
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
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once


#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <cerrno>
#include <cstring>
#include "Downloader.h"
#include "RegexMatcher.h"
#include "util.h"


class SimpleCrawler {
    std::queue<std::string> url_queue_current_depth_;
    std::queue<std::string> url_queue_next_depth_;
    unsigned remaining_crawl_depth_;
    Downloader downloader_;
    std::shared_ptr<RegexMatcher> url_regex_matcher_;
    std::shared_ptr<RegexMatcher> url_ignore_regex_matcher_;
    TimeLimit min_url_processing_time_;

public:
    static const unsigned DEFAULT_TIMEOUT = 5000;                // ms
    static const unsigned DEFAULT_MIN_URL_PROCESSING_TIME = 200; // ms

    struct Params {
        std::string acceptable_languages_;
        unsigned timeout_;
        unsigned min_url_processing_time_;
        bool print_all_http_headers_;
        bool print_last_http_header_;
        bool ignore_robots_dot_txt_;
        bool print_redirects_;
        std::string user_agent_;
        std::string url_ignore_pattern_;
        bool ignore_ssl_certificates_;
        std::string proxy_host_and_port_;
        bool print_queued_urls_;
        bool print_skipped_urls_;

    public:
        explicit Params(const std::string &acceptable_languages = "", const unsigned timeout = DEFAULT_TIMEOUT,
                        const unsigned min_url_processing_time = DEFAULT_MIN_URL_PROCESSING_TIME, const bool print_all_http_headers = false,
                        const bool print_last_http_header = false, const bool ignore_robots_dot_txt = false,
                        const bool print_redirects = false, const std::string &user_agent = "ub_tools (https://ixtheo.de/docs/user_agents)",
                        const std::string &url_ignore_pattern = "(?i)\\.(js|css|bmp|pdf|jpg|gif|png|tif|tiff)(\\?[^?]*)?$",
                        const bool ignore_ssl_certificates_ = false, const std::string &proxy_host_and_port = "",
                        const bool print_queued_urls = false, const bool print_skipped_urls = false);
        ~Params() = default;
    } params_;

    struct SiteDesc {
        std::string start_url_;
        unsigned max_crawl_depth_;
        std::shared_ptr<RegexMatcher> url_regex_matcher_; // all non-matching URL's of subpages will be ignored
    public:
        SiteDesc() = default;
        SiteDesc(const std::string &start_url, const unsigned max_crawl_depth, RegexMatcher * const url_regex_matcher)
            : start_url_(start_url), max_crawl_depth_(max_crawl_depth), url_regex_matcher_(url_regex_matcher) { }
        ~SiteDesc() = default;
    };

    struct PageDetails {
        std::string error_message_;
        std::string url_;
        std::string header_;
        std::string body_;
    };

public:
    explicit SimpleCrawler(const SiteDesc &site_desc, const Params &params);
    ~SimpleCrawler() = default;

    /** \brief  prepare site processing.
     *          use a do ... while loop with GetNextPage afterwards
     */
    bool getNextPage(PageDetails * const page_details);

    unsigned getRemainingCallDepth() { return remaining_crawl_depth_; }

    static void ParseConfigFile(const std::string &config_path, std::vector<SiteDesc> * const site_descs);

    /** \brief  Process site and return all URL's
     *          (simple batch function for constructor & GetNextPage)
     */
    static void ProcessSite(const SiteDesc &site_desc, const Params &params, std::vector<std::string> * const extracted_urls);

    static void ProcessSites(const std::string &config_path, const Params &params, std::vector<std::string> * const extracted_urls);

private:
    /** \brief  try to continue with the next url at the current depth
     *          if all is done at the current depth, switch to next depth
     *          returns false if no URL's are left or remaining depth is 0, else true
     */
    bool continueCrawling();

    void extractLocationUrls(const std::string &header_blob, std::list<std::string> * const location_urls);
};
