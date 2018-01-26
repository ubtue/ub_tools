/** \file   SimpleCrawler.cc
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
#include "SimpleCrawler.h"


SimpleCrawler::Params::Params(const std::string &acceptable_languages, const unsigned timeout,
                              const unsigned min_url_processing_time, const bool all_headers,
                              const bool last_header, const bool ignore_robots_dot_txt,
                              const bool print_redirects, const std::string &user_agent,
                              const std::string &url_ignore_pattern)
    : acceptable_languages_(acceptable_languages), timeout_(timeout),
      min_url_processing_time_(min_url_processing_time), all_headers_(all_headers),
      last_header_(last_header), ignore_robots_dot_txt_(ignore_robots_dot_txt),
      print_redirects_(print_redirects), user_agent_(user_agent),
      url_ignore_pattern_(url_ignore_pattern) {}


void SimpleCrawler::ExtractLocationUrls(const std::string &header_blob, std::list<std::string> * const location_urls) {
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


void SimpleCrawler::ParseConfigFile(File * const input, std::vector<SiteDesc> * const site_descs) {
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


void SimpleCrawler::ProcessURL(const std::string &url, unsigned remaining_crawl_depth, RegexMatcher * const url_regex_matcher,
                               TimeLimit &min_url_processing_time, const Params &params, std::unordered_set<std::string> * const extracted_urls)
{
    std::string err_msg;
    static RegexMatcher * const url_ignore_regex_matcher(RegexMatcher::RegexMatcherFactory(params.url_ignore_pattern_, &err_msg, RegexMatcher::Option::CASE_INSENSITIVE));
    if (url_ignore_regex_matcher == nullptr)
        ERROR("could not initialize URL regex matcher\n"
              + err_msg);

    if (url_ignore_regex_matcher->matched(url)) {
        logger->warning("Skipping URL: " + url);
        return;
    }

    Downloader::Params downloader_params;
    downloader_params.user_agent_            = params.user_agent_;
    downloader_params.acceptable_languages_  = params.acceptable_languages_;
    downloader_params.honour_robots_dot_txt_ = not params.ignore_robots_dot_txt_;
    min_url_processing_time.sleepUntilExpired();
    Downloader downloader(url, downloader_params, params.timeout_);
    min_url_processing_time.restart();
    if (downloader.anErrorOccurred()) {
        logger->warning("Failed to retrieve a Web page (" + url + "):\n"
                        + downloader.getLastErrorMessage());
        return;
    }

    const std::string message_headers(downloader.getMessageHeader()), message_body(downloader.getMessageBody());
    if (params.print_redirects_) {
        std::list<std::string> location_urls;
        ExtractLocationUrls(message_headers, &location_urls);
        for (const auto &location_url : location_urls)
            std::cout << "Location: " << location_url << '\n';
        std::cout << "\n\n";
    }

    if (params.all_headers_ or params.last_header_) {
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
        if (url_regex_matcher->matched(extracted_url))
            extracted_urls->emplace(extracted_url);
    }

    --remaining_crawl_depth;
    if (remaining_crawl_depth > 0) {
        for (const auto &url_and_anchor_texts : urls_and_anchor_texts)
            ProcessURL(url_and_anchor_texts.getUrl(), remaining_crawl_depth, url_regex_matcher,
                       min_url_processing_time, params, extracted_urls);
    }
}


void SimpleCrawler::ProcessSite(const SiteDesc &site_desc, const Params &params, std::unordered_set<std::string> * const extracted_urls)
{
    TimeLimit min_url_processing_time(params.min_url_processing_time_);
    ProcessURL(site_desc.start_url_, site_desc.max_crawl_depth_, site_desc.url_regex_matcher_, min_url_processing_time, params, extracted_urls);
}
