/** \file   SimpleCrawler.h
 *  \brief  Identifies URL's that we can use for further processing.
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
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SIMPLE_CRAWLER_H
#define SIMPLE_CRAWLER_H


#include <iostream>
#include <unordered_set>
#include <utility>
#include <cerrno>
#include <cstring>
#include "Downloader.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "SimpleCrawler.h"
#include "StringUtil.h"
#include "util.h"
#include "WebUtil.h"


class SimpleCrawler {
public:
    static const unsigned DEFAULT_TIMEOUT = 5000;
    static const unsigned DEFAULT_MIN_URL_PROCESSING_TIME = 5000;

    struct Params {
        std::string acceptable_languages_;
        unsigned timeout_;
        unsigned min_url_processing_time_;
        bool all_headers_;
        bool last_header_;
        bool ignore_robots_dot_txt_;
        bool print_redirects_;
        std::string user_agent_;
        std::string url_ignore_pattern_;
    public:
        explicit Params(const std::string &acceptable_languages = "",
                        const unsigned timeout = DEFAULT_TIMEOUT, // ms
                        const unsigned min_url_processing_time = DEFAULT_MIN_URL_PROCESSING_TIME, // ms
                        const bool all_headers = false,
                        const bool last_header = false,
                        const bool ignore_robots_dot_txt = false,
                        const bool print_redirects = false,
                        const std::string &user_agent = "ub_tools (https://ixtheo.de/docs/user_agents)",
                        const std::string &url_ignore_pattern = "\\.(js|css|bmp|pdf|jpg|gif|png|tif|tiff)(\\?[^?]*)?$"
                        );
    } params_;

    struct SiteDesc {
        std::string start_url_;
        unsigned max_crawl_depth_;
        RegexMatcher *url_regex_matcher_;
    public:
        SiteDesc(const std::string &start_url, const unsigned max_crawl_depth, RegexMatcher * const url_regex_matcher)
            : start_url_(start_url), max_crawl_depth_(max_crawl_depth), url_regex_matcher_(url_regex_matcher) { }
    };
private:
    void ExtractLocationUrls(const std::string &header_blob, std::list<std::string> * const location_urls);
    void ProcessURL(const std::string &url, unsigned remaining_crawl_depth, RegexMatcher * const url_regex_matcher,
                    TimeLimit &min_url_processing_time, const Params &params, std::unordered_set<std::string> * const extracted_urls);
public:
    void ParseConfigFile(File * const input, std::vector<SiteDesc> * const site_descs);
    void ProcessSite(const SiteDesc &site_desc, const Params &params, std::unordered_set<std::string> * const extracted_urls);
};


#endif // ifndef JSON_H
