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
#include "SyndicationFormat.h"
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


void Tasklet::run(const Params &parameters, Result * const result) {
    PostToTranslationServer(parameters.translation_server_url_, parameters.time_limit_, parameters.user_agent_,
                            parameters.download_item_.url_, &result->response_body_, &result->response_code_, &result->error_message_);

    // 300 => multiple matches found, try to harvest children (send the response_body right back to the server, to get all of them)
    if (result->response_code_ == 300) {
        LOG_DEBUG("multiple articles found => trying to harvest children");
        PostToTranslationServer(parameters.translation_server_url_, parameters.time_limit_, parameters.user_agent_,
                                result->response_body_, &result->response_body_, &result->response_code_, &result->error_message_);
    }

    download_manager_->addToDownloadCache(parameters.download_item_.url_.toString(), result->response_body_,
                                                     result->response_code_, result->error_message_);
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "DirectDownload: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::move(parameters), std::unique_ptr<Result>(new Result(parameters->download_item_))),
   download_manager_(download_manager) {}


} // end namespace DirectDownload


namespace Crawling {


void Tasklet::run(const Params &parameters, Result * const result) {
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

    LOG_DEBUG("\n\nStarting crawl at base URL: " +  parameters.download_item_.url_.toString());

    SimpleCrawler crawler(site_desc, crawler_params);
    SimpleCrawler::PageDetails page_details;

    while (crawler.getNextPage(&page_details)) {
        if (page_details.error_message_.empty()) {
            const auto url(page_details.url_);
            if (parameters.download_item_.journal_.crawl_params_.extraction_regex_->matched(url)) {
                const auto new_download_item(Util::Harvestable::New(page_details.url_, parameters.download_item_.journal_));
                result->downloaded_items_.emplace_back(download_manager_->directDownload(new_download_item, parameters.user_agent_));
            }
        }
    }
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "Crawling: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::move(parameters), std::unique_ptr<Result>(new Result())),
   download_manager_(download_manager) {}


} // end namespace Crawling


namespace RSS {


void Tasklet::run(const Params &parameters, Result * const result) {
    std::unique_ptr<SyndicationFormat> syndication_format;
    std::string syndication_format_parse_err_msg;
    SyndicationFormat::AugmentParams syndication_format_augment_parameters;
    syndication_format_augment_parameters.strptime_format_ = parameters.download_item_.journal_.strptime_format_string_;

    if (not parameters.feed_contents_.empty()) {
        syndication_format.reset(SyndicationFormat::Factory(parameters.feed_contents_, syndication_format_augment_parameters,
                                &syndication_format_parse_err_msg).release());
    } else {
        Downloader::Params downloader_params;
        downloader_params.user_agent_ = parameters.user_agent_;

        Downloader downloader(parameters.download_item_.url_.toString(), downloader_params);
        if (downloader.anErrorOccurred()) {
            LOG_WARNING("could not download RSS feed '" + parameters.download_item_.url_.toString() + "'!");
            return;
        }

        syndication_format.reset(SyndicationFormat::Factory(downloader.getMessageBody(), syndication_format_augment_parameters,
                                 &syndication_format_parse_err_msg).release());
    }

    if (syndication_format == nullptr) {
        LOG_WARNING("problem parsing XML document for RSS feed '" + parameters.download_item_.url_.toString() + "': "
                    + syndication_format_parse_err_msg);
        return;
    }

    LOG_DEBUG(parameters.download_item_.url_.toString() + " (" + syndication_format->getFormatName() + "):");
    LOG_DEBUG("\tTitle: " + syndication_format->getTitle());
    LOG_DEBUG("\tLink: " + syndication_format->getLink());
    LOG_DEBUG("\tDescription: " + syndication_format->getDescription());

    for (const auto &item : *syndication_format) {
        const auto item_id(item.getId());
        const std::string title(item.getTitle());
        if (not title.empty())
            LOG_DEBUG("\n\nFeed Item: " + title);

        const auto new_download_item(Util::Harvestable::New(item.getLink(), parameters.download_item_.journal_));
        result->downloaded_items_.emplace_back(download_manager_->directDownload(new_download_item, parameters.user_agent_));
    }
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "RSS: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::move(parameters), std::unique_ptr<Result>(new Result())),
   download_manager_(download_manager) {}


} // end namespace RSS


void *DownloadManager::BackgroundThreadRoutine(void * parameter) {
    static const unsigned BACKGROUND_THREAD_SLEEP_TIME(16 * 1000 * 1000);   // in ms

    DownloadManager * const download_manager(reinterpret_cast<DownloadManager *>(parameter));
    pthread_detach(pthread_self());

    while (true) {
        download_manager->processQueueBuffers();

        // we don't need to lock access to the domain data store
        // as it's exclusively accessed in this background thread
        for (const auto &domain_entry : download_manager->domain_data_) {
            download_manager->processDomainQueues(domain_entry.second.get());
            download_manager->cleanupCompletedTasklets(domain_entry.second.get());
        }

        ::usleep(BACKGROUND_THREAD_SLEEP_TIME);
    }

    pthread_exit(nullptr);
    return nullptr;
}


DownloadManager::DelayParams DownloadManager::generateDelayParams(const Url &url) {
    const auto hostname(url.getAuthority());
    Downloader robots_txt_downloader(url.getRobotsDotTxtUrl());
    if (robots_txt_downloader.anErrorOccurred()) {
        LOG_DEBUG("couldn't retrieve robots.txt for domain '" + hostname + "'");
        return DelayParams(static_cast<unsigned>(0), global_params_.default_download_delay_time_,
                           global_params_.max_download_delay_time_);
    }

    DelayParams new_delay_params(robots_txt_downloader.getMessageBody(), global_params_.default_download_delay_time_,
                                 global_params_.max_download_delay_time_);

    LOG_INFO("set crawl-delay for domain '" + hostname + "' to " +
             std::to_string(new_delay_params.time_limit_.getLimit()) + " ms");
    return new_delay_params;
}


DownloadManager::DomainData *DownloadManager::lookupDomainData(const Url &url, bool add_if_absent) {
    const auto hostname(url.getAuthority());
    const auto match(domain_data_.find(hostname));
    if (match != domain_data_.end())
        return match->second.get();
    else if (add_if_absent) {
        DomainData * const new_domain_data(new DomainData(generateDelayParams(url)));
        domain_data_.insert(std::make_pair(hostname, new_domain_data));
        return new_domain_data;
    }

    return nullptr;
}


void DownloadManager::processQueueBuffers() {
    // enqueue the tasks in their domain-specific queues
    {
        std::lock_guard<std::recursive_mutex> direct_download_queue_buffer_lock(direct_download_queue_buffer_mutex_);
        std::lock_guard<std::recursive_mutex> download_cache_lock(cached_download_data_mutex_);
        while (not direct_download_queue_buffer_.empty()) {
            std::shared_ptr<DirectDownload::Tasklet> tasklet(direct_download_queue_buffer_.front());
            // we lookup the cached data elsewhere and return it immediately instead of queueing a task
            if (cached_download_data_.find(tasklet->getParameter().download_item_.url_) != cached_download_data_.end()) {
                LOG_ERROR("cached data found for queued download item '" + tasklet->getParameter().download_item_.url_.toString() +
                          "' - this should never happen!");
            }

            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_direct_downloads_.emplace_back(tasklet);

            direct_download_queue_buffer_.pop_front();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> crawling_queue_buffer_lock(crawling_queue_buffer_mutex_);
        while (not crawling_queue_buffer_.empty()) {
            std::shared_ptr<Crawling::Tasklet> tasklet(crawling_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_crawls_.emplace_back(tasklet);

            crawling_queue_buffer_.pop_front();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> rss_queue_buffer_lock(rss_queue_buffer_mutex_);
        while (not rss_queue_buffer_.empty()) {
            std::shared_ptr<RSS::Tasklet> tasklet(rss_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_rss_feeds_.emplace_back(tasklet);

            rss_queue_buffer_.pop_front();
        }
    }
}


void DownloadManager::processDomainQueues(DomainData * const domain_data) {
    // apply download delays and create tasklets for downloads/crawls
    if (direct_download_tasklet_execution_counter_ == MAX_DIRECT_DOWNLOAD_TASKLETS
        and crawling_tasklet_execution_counter_ == MAX_CRAWLING_TASKLETS
        and rss_tasklet_execution_counter_ == MAX_RSS_TASKLETS)
    {
        return;
    }

    if (domain_data->queued_direct_downloads_.empty() and domain_data->queued_crawls_.empty() and domain_data->queued_rss_feeds_.empty())
        return;
    else if (not global_params_.ignore_robots_txt_ and not domain_data->delay_params_.time_limit_.limitExceeded())
        return;

    domain_data->delay_params_.time_limit_.restart();

    std::shared_ptr<DirectDownload::Tasklet> direct_download_tasklet(domain_data->queued_direct_downloads_.front());
    domain_data->active_direct_downloads_.emplace_back(direct_download_tasklet);
    domain_data->queued_direct_downloads_.pop_front();
    direct_download_tasklet->start();

    std::shared_ptr<Crawling::Tasklet> crawling_tasklet(domain_data->queued_crawls_.front());
    domain_data->active_crawls_.emplace_back(crawling_tasklet);
    domain_data->queued_crawls_.pop_front();
    crawling_tasklet->start();

    std::shared_ptr<RSS::Tasklet> rss_tasklet(domain_data->queued_rss_feeds_.front());
    domain_data->active_rss_feeds_.emplace_back(rss_tasklet);
    domain_data->queued_rss_feeds_.pop_front();
    rss_tasklet->start();
}


void DownloadManager::cleanupCompletedTasklets(DomainData * const domain_data) {
    for (auto iter(domain_data->active_direct_downloads_.begin()); iter != domain_data->active_direct_downloads_.end();) {
        if ((*iter)->isComplete()) {
            iter = domain_data->active_direct_downloads_.erase(iter);
            continue;
        }
        ++iter;
    }

    for (auto iter(domain_data->active_crawls_.begin()); iter != domain_data->active_crawls_.end();) {
        if ((*iter)->isComplete()) {
            iter = domain_data->active_crawls_.erase(iter);
            continue;
        }
        ++iter;
    }

    for (auto iter(domain_data->active_rss_feeds_.begin()); iter != domain_data->active_rss_feeds_.end();) {
        if ((*iter)->isComplete()) {
            iter = domain_data->active_rss_feeds_.erase(iter);
            continue;
        }
        ++iter;
    }
}


DownloadManager::DownloadManager(const GlobalParams &global_params)
 : global_params_(global_params)
{
    if (::pthread_create(&background_thread_, nullptr, BackgroundThreadRoutine, this) != 0)
            LOG_ERROR("background download manager thread creation failed!");
}


DownloadManager::~DownloadManager() {
    if (::pthread_cancel(background_thread_) != 0)
        LOG_ERROR("failed to cancel background download manager thread '" + std::to_string(background_thread_) + "'!");
}


std::unique_ptr<DownloadResult<DirectDownload::Params, DirectDownload::Result>>
    DownloadManager::directDownload(const Util::Harvestable &source, const std::string &user_agent)
{
    // check if we have a cached response and return it immediately, if any
    const auto cache_hit(cached_download_data_.find(source.url_.toString()));
    if (cache_hit != cached_download_data_.end()) {
        std::unique_ptr<DirectDownload::Result> cached_result(new DirectDownload::Result(source));
        cached_result->response_body_ = cache_hit->second.response_body_;
        cached_result->response_code_ = cache_hit->second.response_code_;
        cached_result->error_message_ = cache_hit->second.error_message_;

        std::unique_ptr<DownloadResult<DirectDownload::Params, DirectDownload::Result>>
            download_result(new DownloadResult<DirectDownload::Params, DirectDownload::Result>(std::move(cached_result)));
        return download_result;
    }


    std::unique_ptr<DirectDownload::Params> parameters(new DirectDownload::Params(source,
                                                       global_params_.translation_server_url_.toString(), user_agent,
                                                       DOWNLOAD_TIMEOUT));
    std::shared_ptr<DirectDownload::Tasklet> new_tasklet(new DirectDownload::Tasklet(&direct_download_tasklet_execution_counter_,
                                                         this, std::move(parameters)));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(direct_download_queue_buffer_mutex_);
        direct_download_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<DownloadResult<DirectDownload::Params, DirectDownload::Result>>
        download_result(new DownloadResult<DirectDownload::Params, DirectDownload::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<DownloadResult<Crawling::Params, Crawling::Result>> DownloadManager::crawl(const Util::Harvestable &source,
                                                                                           const std::string &user_agent)
{
    std::unique_ptr<Crawling::Params> parameters(new Crawling::Params(source, user_agent, DOWNLOAD_TIMEOUT,
                                                 global_params_.ignore_robots_txt_));
    std::shared_ptr<Crawling::Tasklet> new_tasklet(new Crawling::Tasklet(&direct_download_tasklet_execution_counter_,
                                                   this, std::move(parameters)));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(crawling_queue_buffer_mutex_);
        crawling_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<DownloadResult<Crawling::Params, Crawling::Result>>
        download_result(new DownloadResult<Crawling::Params, Crawling::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<DownloadResult<RSS::Params, RSS::Result>> DownloadManager::rss(const Util::Harvestable &source,
                                                                               const std::string &user_agent,
                                                                               const std::string &feed_contents)
{
    std::unique_ptr<RSS::Params> parameters(new RSS::Params(source, user_agent, feed_contents));
    std::shared_ptr<RSS::Tasklet> new_tasklet(new RSS::Tasklet(&rss_tasklet_execution_counter_,
                                              this, std::move(parameters)));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(rss_queue_buffer_mutex_);
        rss_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<DownloadResult<RSS::Params, RSS::Result>>
        download_result(new DownloadResult<RSS::Params, RSS::Result>(new_tasklet));
    return download_result;
}


void DownloadManager::addToDownloadCache(const std::string &url, const std::string &response_body,
                                         const unsigned response_code, const std::string &error_message)
{
    std::lock_guard<std::recursive_mutex> download_cache_lock(cached_download_data_mutex_);

    const auto cache_hit(cached_download_data_.find(url));
    if (cache_hit == cached_download_data_.end()) {
        cached_download_data_.insert(std::make_pair(url, CachedDownloadData { response_body, response_code, error_message }));
        return;
    }

    LOG_WARNING("cached download data overwritten for URL '" + url + "'");
    cache_hit->second.response_body_ = response_body;
    cache_hit->second.response_code_ = response_code;
    cache_hit->second.error_message_ = error_message;
}


} // end namespace Download


} // end namespace ZoteroHarvester
