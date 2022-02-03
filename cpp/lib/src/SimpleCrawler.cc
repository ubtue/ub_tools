/** \file   SimpleCrawler.cc
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
#include "SimpleCrawler.h"
#include "StringUtil.h"
#include "WebUtil.h"


SimpleCrawler::Params::Params(const std::string &acceptable_languages, const unsigned timeout, const unsigned min_url_processing_time,
                              const bool print_all_http_headers, const bool print_last_http_header, const bool ignore_robots_dot_txt,
                              const bool print_redirects, const std::string &user_agent, const std::string &url_ignore_pattern,
                              const bool ignore_ssl_certificates, const std::string &proxy_host_and_port, const bool print_queued_urls,
                              const bool print_skipped_urls)
    : acceptable_languages_(acceptable_languages), timeout_(timeout), min_url_processing_time_(min_url_processing_time),
      print_all_http_headers_(print_all_http_headers), print_last_http_header_(print_last_http_header),
      ignore_robots_dot_txt_(ignore_robots_dot_txt), print_redirects_(print_redirects), user_agent_(user_agent),
      url_ignore_pattern_(url_ignore_pattern), ignore_ssl_certificates_(ignore_ssl_certificates), proxy_host_and_port_(proxy_host_and_port),
      print_queued_urls_(print_queued_urls), print_skipped_urls_(print_skipped_urls) {
}


void SimpleCrawler::extractLocationUrls(const std::string &header_blob, std::list<std::string> * const location_urls) {
    location_urls->clear();

    std::vector<std::string> header_lines;
    StringUtil::SplitThenTrimWhite(header_blob, "\r\n", &header_lines);
    for (const auto &header_line : header_lines) {
        if (StringUtil::StartsWith(header_line, "Location:", /* ignore_case = */ true)) {
            std::string location(header_line.substr(9));
            StringUtil::Trim(&location);
            if (not location.empty())
                location_urls->push_back(location);
        }
    }
}


void SimpleCrawler::ParseConfigFile(const std::string &config_path, std::vector<SiteDesc> * const site_descs) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(config_path));

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
        const unsigned no_of_parts(StringUtil::SplitThenTrimWhite(line, ' ', &line_parts));
        if (no_of_parts != 3 and no_of_parts != 4)
            LOG_ERROR("bad input line #" + std::to_string(line_no) + " in \"" + input->getPath() + "\"!");

        unsigned max_crawl_depth;
        if (not StringUtil::ToUnsigned(line_parts[1], &max_crawl_depth))
            LOG_ERROR("bad input line #" + std::to_string(line_no) + " in \"" + input->getPath() + "\"! (Invalid max. crawl depth: \""
                      + line_parts[1] + "\")");

        std::string err_msg;
        RegexMatcher * const url_regex_matcher(RegexMatcher::RegexMatcherFactory(line_parts[2], &err_msg));
        if (url_regex_matcher == nullptr)
            LOG_ERROR("bad input line #" + std::to_string(line_no) + " in \"" + input->getPath() + "\", regex is faulty! (" + err_msg
                      + ")");

        site_descs->emplace_back(line_parts[0], max_crawl_depth, url_regex_matcher);
    }
}


SimpleCrawler::SimpleCrawler(const SiteDesc &site_desc, const Params &params)
    : remaining_crawl_depth_(site_desc.max_crawl_depth_), url_regex_matcher_(site_desc.url_regex_matcher_),
      min_url_processing_time_(params.min_url_processing_time_), params_(params) {
    url_queue_current_depth_.push(site_desc.start_url_);

    std::string err_msg;
    url_ignore_regex_matcher_.reset(
        RegexMatcher::RegexMatcherFactory(params.url_ignore_pattern_, &err_msg, RegexMatcher::Option::CASE_INSENSITIVE));
    if (url_ignore_regex_matcher_ == nullptr)
        LOG_ERROR("could not initialize URL ignore regex matcher: " + err_msg);

    downloader_.setAcceptableLanguages(params.acceptable_languages_);
    downloader_.setHonourRobotsDotTxt(not params.ignore_robots_dot_txt_);
    downloader_.setIgnoreSslCertificates(params.ignore_ssl_certificates_);
    downloader_.setProxy(params.proxy_host_and_port_);
    downloader_.setUserAgent(params.user_agent_);
}


bool SimpleCrawler::continueCrawling() {
    // stay on current depth if more URL's exist here
    if (not url_queue_current_depth_.empty())
        return true;

    // abort if no URL's left
    if (url_queue_current_depth_.empty() and url_queue_next_depth_.empty())
        return false;

    // switch to next depth if all URL's on current depth have been processed
    if (url_queue_current_depth_.empty() and not url_queue_next_depth_.empty()) {
        if (remaining_crawl_depth_ == 0)
            return false;
        else {
            --remaining_crawl_depth_;
            url_queue_current_depth_.swap(url_queue_next_depth_);
            url_queue_next_depth_ = std::queue<std::string>();
        }
    }
    return true;
}


bool SimpleCrawler::getNextPage(PageDetails * const page_details) {
    page_details->error_message_.clear();
    // get next URL and check if it can be processed
    const std::string url(url_queue_current_depth_.front());
    url_queue_current_depth_.pop();
    if (url_ignore_regex_matcher_->matched(url)) {
        page_details->error_message_ = "URL contains ignorable data (e.g. CSS file) and will be skipped";
        logger->warning("Skipping URL: " + url);
        return SimpleCrawler::continueCrawling();
    }

    // download page
    min_url_processing_time_.sleepUntilExpired();
    downloader_.newUrl(url, params_.timeout_);
    min_url_processing_time_.restart();
    if (downloader_.anErrorOccurred()) {
        page_details->error_message_ = "Download failed: " + downloader_.getLastErrorMessage();
        logger->warning("Failed to retrieve a Web page (" + url + "):\n" + downloader_.getLastErrorMessage());
        return SimpleCrawler::continueCrawling();
    }

    // print message headers if necessary
    const std::string message_headers(downloader_.getMessageHeader()), message_body(downloader_.getMessageBody());
    if (params_.print_redirects_) {
        std::list<std::string> location_urls;
        extractLocationUrls(message_headers, &location_urls);
        for (const auto &location_url : location_urls)
            logger->info("Location: " + location_url);
    }
    if (params_.print_all_http_headers_ or params_.print_last_http_header_)
        logger->info(StringUtil::ReplaceString("\r\n", "\n", message_headers) + "\n");

    // fill result
    page_details->url_ = url;
    page_details->header_ = message_headers;
    page_details->body_ = message_body;

    // extract deeper level URL's
    if (remaining_crawl_depth_ > 0) {
        constexpr unsigned EXTRACT_URL_FLAGS(WebUtil::IGNORE_DUPLICATE_URLS | WebUtil::IGNORE_LINKS_IN_IMG_TAGS
                                             | WebUtil::REMOVE_DOCUMENT_RELATIVE_ANCHORS | WebUtil::CLEAN_UP_ANCHOR_TEXT
                                             | WebUtil::KEEP_LINKS_TO_SAME_MAJOR_SITE_ONLY | WebUtil::ATTEMPT_TO_EXTRACT_JAVASCRIPT_URLS);

        std::vector<WebUtil::UrlAndAnchorTexts> urls_and_anchor_texts;
        WebUtil::ExtractURLs(message_body, url, WebUtil::ABSOLUTE_URLS, &urls_and_anchor_texts, EXTRACT_URL_FLAGS);
        for (const auto &url_and_anchor_texts : urls_and_anchor_texts) {
            const std::string extracted_url(url_and_anchor_texts.getUrl());
            if (not url_regex_matcher_ or url_regex_matcher_->matched(extracted_url)) {
                url_queue_next_depth_.push(extracted_url);

                if (params_.print_queued_urls_)
                    LOG_DEBUG("queued URL for further crawling: '" + extracted_url + "'");
            } else if (params_.print_skipped_urls_)
                LOG_DEBUG("skipped URL: '" + extracted_url + "'");
        }
    }

    return SimpleCrawler::continueCrawling();
}


void SimpleCrawler::ProcessSite(const SiteDesc &site_desc, const Params &params, std::vector<std::string> * const extracted_urls) {
    SimpleCrawler crawler(site_desc, params);
    PageDetails page_details;
    while (crawler.getNextPage(&page_details))
        if (page_details.error_message_.empty())
            extracted_urls->emplace_back(page_details.url_);
}


void SimpleCrawler::ProcessSites(const std::string &config_path, const SimpleCrawler::Params &params,
                                 std::vector<std::string> * const extracted_urls) {
    std::vector<SimpleCrawler::SiteDesc> site_descs;
    SimpleCrawler::ParseConfigFile(config_path, &site_descs);
    for (const auto &site_desc : site_descs)
        SimpleCrawler::ProcessSite(site_desc, params, extracted_urls);
}
