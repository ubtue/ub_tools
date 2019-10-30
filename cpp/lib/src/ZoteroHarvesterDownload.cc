/** \brief Classes related to the Zotero Harvester's download API
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "StringUtil.h"
#include "ZoteroHarvesterDownload.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Download {


namespace DirectDownload {


static void PostToTranslationServer(const Url &translation_server_url, const TimeLimit &time_limit, const std::string &user_agent,
                                    const std::string &request_body, std::string * const response_body, unsigned * response_code,
                                    std::string * const error_message)
{
    Downloader::Params downloader_params;
    downloader_params.user_agent_ = user_agent;
    downloader_params.additional_headers_ = { "Accept: application/json", "Content-Type: text/plain" };
    downloader_params.post_data_ = request_body;

    Downloader downloader(translation_server_url.toString() + "/web", downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *response_code = 0;
        *error_message = downloader.getLastErrorMessage();
        return;
    }

    *response_body = downloader.getMessageBody();
    *response_code = downloader.getResponseCode();
}


void Tasklet::Run(const Params &parameters, Result * const result) {
    PostToTranslationServer(parameters.translation_server_url_, parameters.time_limit_, parameters.user_agent_,
                            parameters.download_item_.url_, &result->response_body_, &result->response_code_, &result->error_message_);

    // 300 => multiple matches found, try to harvest children (send the response_body right back to the server, to get all of them)
    if (result->response_code_ == 300) {
        LOG_DEBUG("multiple articles found => trying to harvest children");
        PostToTranslationServer(parameters.translation_server_url_, parameters.time_limit_, parameters.user_agent_,
                                result->response_body_, &result->response_body_, &result->response_code_, &result->error_message_);
    }
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned long> * const instance_counter, std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "DirectDownload: " + parameters->download_item_.url_.toString(), &Tasklet::Run,
                                 std::move(parameters), std::unique_ptr<Result>(new Result())) {}


} // end namespace DirectDownload


namespace Crawling {


void Tasklet::Run(const Params &parameters, Result * const result) {
    // The crawler implementation is different from the direct download implemenation in that
    // the meat of the cralwer is implemented in the generic SimpleCrawler class. This means it
    // cannot be coupled to the download waiting/caching infrastructure offered by the DownloadManager
    // class. A future improvement would be to write a wrapper for the SimpleCrawler class that will
    // let us plug-in our own downloader.
    SimpleCrawler::Params crawler_params;
    crawler_params.ignore_robots_dot_txt_ = parameters.ignore_robots_dot_txt_;
    crawler_params.timeout_ = parameters.time_limit_.getLimit();
    crawler_params.user_agent_ = parameters.user_agent_;

    SimpleCrawler::SiteDesc site_desc;
    site_desc.start_url_ = parameters.download_item_.url_;
    site_desc.max_crawl_depth_ = parameters.download_item_.journal_.crawl_params_.max_crawl_depth_;

    std::string crawl_url_regex_str;
    if (parameters.download_item_.journal_.crawl_params_.crawl_url_regex_ != nullptr)
        crawl_url_regex_str = parameters.download_item_.journal_.crawl_params_.crawl_url_regex_->getPattern();

    if (not crawl_url_regex_str.empty()) {
        // the crawl URL regex needs to be combined with the extraction URL regex if they aren't the same
        // we combine the two here to prevent unnecessary duplication in the config file
        const auto extraction_url_regex_pattern(parameters.download_item_.journal_.crawl_params_.extraction_regex_ != nullptr ?
                                                parameters.download_item_.journal_.crawl_params_.extraction_regex_->getPattern() : "");

        if (not extraction_url_regex_pattern.empty() and extraction_url_regex_pattern != crawl_url_regex_str)
            crawl_url_regex_str = "((" + crawl_url_regex_str + ")|(" + extraction_url_regex_pattern + "))";

        site_desc.url_regex_matcher_.reset(RegexMatcher::RegexMatcherFactoryOrDie(crawl_url_regex_str));
    }

    LOG_DEBUG("\n\nStarting crawl at base URL: " +  parameters.download_item_.url_);

    SimpleCrawler crawler(site_desc, crawler_params);
    SimpleCrawler::PageDetails page_details;
    unsigned processed_url_count(0);

    while (crawler.getNextPage(&page_details)) {
        if (page_details.error_message_.empty()) {
            const auto url(page_details.url_);
            if (parameters.download_item_.journal_.crawl_params_.extraction_regex_->matched(url)) {
                const auto new_download_item(Util::Harvestable::New(page_details.url_, parameters.download_item_.journal_));
                // TODO enqueue the item in the download manager, add the future to the result
            }
        }
    }

    // wait until the queued URLs have been downloaded or the time-out is reached
    TimeLimit max_waiting_time(10 * 60 * 1000);    // 10 minutes
    while (true) {
        unsigned completed_downloads(0);
        for (const auto &download_result : result->downloaded_urls_) {
            if (download_result->isComplete())
                ++completed_downloads;
        }

        if (completed_downloads == result->downloaded_urls_.size())
            break;
        else if (max_waiting_time.limitExceeded()) {
            LOG_WARNING("crawling timed-out on start URL '" + parameters.download_item_.url_.toString() + "'. " +
                        "completed items: " + std::to_string(completed_downloads) + "/" + std::to_string(result->downloaded_urls_.size()));

            // remove incomplete downloads from the results
            for (auto iter(result->downloaded_urls_.begin()); iter != result->downloaded_urls_.end();) {
                if (not (*iter)->isComplete()) {
                    iter = result->downloaded_urls_.erase(iter);
                    continue;
                }

                ++iter;
            }

            break;
        }

        ::usleep(16 * 1000 * 1000);
    }
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned long> * const instance_counter, std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "Crawling: " + parameters->download_item_.url_.toString(), &Tasklet::Run,
                                 std::move(parameters), std::unique_ptr<Result>(new Result())) {}


} // end namespace Crawling


} // end namespace Download


} // end namespace ZoteroHarvester
