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
#pragma once


#include <memory>
#include <set>
#include <unordered_map>
#include "Downloader.h"
#include "JSON.h"
#include "RegexMatcher.h"
#include "RobotsDotTxt.h"
#include "SimpleCrawler.h"
#include "SyndicationFormat.h"
#include "ThreadUtil.h"
#include "TimeLimit.h"
#include "Url.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace ZoteroHarvester {


namespace Download {


class DownloadManager;


namespace DirectDownload {


struct Params {
    Util::HarvestableItem download_item_;
    Url translation_server_url_;
    std::string user_agent_;
    TimeLimit time_limit_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string &translation_server_url,
                    const std::string user_agent, const TimeLimit time_limit)
     : download_item_(download_item), translation_server_url_(translation_server_url), user_agent_(user_agent),
       time_limit_(time_limit) {}
};


struct Result {
    Util::HarvestableItem source_;
    std::string response_body_;
    unsigned response_code_;
    std::string error_message_;
public:
    explicit Result(const Util::HarvestableItem &source) : source_(source), response_code_(0) {}
    Result(const Result &rhs) = default;
    inline bool isValid() const { return response_code_ == 200 and error_message_.empty(); }
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;

    void run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
            DownloadManager * const download_manager, std::unique_ptr<Params> parameters);
    virtual ~Tasklet() override = default;
};


} // end namespace DirectDownload


namespace Crawling {


struct Params {
    Util::HarvestableItem download_item_;
    std::string user_agent_;
    TimeLimit per_crawl_url_time_limit_;
    TimeLimit total_crawl_time_limit_;
    bool ignore_robots_dot_txt_;
    Util::HarvestableItemManager * const harvestable_manager_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string user_agent, const TimeLimit per_crawl_url_time_limit,
                    const TimeLimit total_crawl_time_limit, const bool ignore_robots_dot_txt,
                    Util::HarvestableItemManager * const harvestable_manager)
     : download_item_(download_item), user_agent_(user_agent), per_crawl_url_time_limit_(per_crawl_url_time_limit),
       total_crawl_time_limit_(total_crawl_time_limit), ignore_robots_dot_txt_(ignore_robots_dot_txt),
       harvestable_manager_(harvestable_manager) {}
};


struct Result {
    std::vector<std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>> downloaded_items_;
public:
    Result() = default;
    Result(const Result &rhs) = delete;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;
    void run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
            DownloadManager * const download_manager, std::unique_ptr<Params> parameters);
    virtual ~Tasklet() override = default;
};


} // end namespace Crawling


namespace RSS {


struct Params {
    Util::HarvestableItem download_item_;
    std::string user_agent_;
    std::string feed_contents_;
    Util::HarvestableItemManager * const harvestable_manager_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string user_agent, const std::string &feed_contents,
                    Util::HarvestableItemManager * const harvestable_manager)
     : download_item_(download_item), user_agent_(user_agent), feed_contents_(feed_contents),
       harvestable_manager_(harvestable_manager) {}
};


struct Result {
    std::vector<std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>> downloaded_items_;
public:
    Result() = default;
    Result(const Result &rhs) = delete;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;
    const Util::UploadTracker &upload_tracker_;
    bool force_downloads_;
    unsigned feed_harvest_interval_;
    bool force_process_feeds_with_no_pub_dates_;

    void run(const Params &parameters, Result * const result);
    bool feedNeedsToBeHarvested(const std::string &feed_contents, const Config::JournalParams &journal_params,
                                const SyndicationFormat::AugmentParams &syndication_format_site_params) const;
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
            std::unique_ptr<Params> parameters, const Util::UploadTracker &upload_tracker, const bool force_downloads,
            const unsigned feed_harvest_interval, const bool force_process_feeds_with_no_pub_dates);
    virtual ~Tasklet() override = default;
};


} // end namespace RSS


class DownloadManager {
public:
    struct GlobalParams {
        Url translation_server_url_;
        unsigned default_download_delay_time_;
        unsigned max_download_delay_time_;
        unsigned rss_feed_harvest_interval_;
        bool force_process_rss_feeds_with_no_pub_dates_;
        bool ignore_robots_txt_;
        bool force_downloads_;
        Util::HarvestableItemManager * const harvestable_manager_;
    public:
        GlobalParams(const Config::GlobalParams &config_global_params, Util::HarvestableItemManager * const harvestable_manager)
         : translation_server_url_(config_global_params.translation_server_url_),
           default_download_delay_time_(config_global_params.download_delay_params_.default_delay_),
           max_download_delay_time_(config_global_params.download_delay_params_.max_delay_),
           rss_feed_harvest_interval_(config_global_params.rss_harvester_operation_params_.harvest_interval_),
           force_process_rss_feeds_with_no_pub_dates_(config_global_params.rss_harvester_operation_params_.force_process_feeds_with_no_pub_dates_),
           ignore_robots_txt_(false), force_downloads_(false), harvestable_manager_(harvestable_manager) {}
        GlobalParams(const GlobalParams &rhs) = default;
    };
private:
    struct DelayParams {
        RobotsDotTxt robots_dot_txt_;
        TimeLimit time_limit_;
    public:
        DelayParams(const std::string &robots_dot_txt, const unsigned default_download_delay_time,
                    const unsigned max_download_delay_time)
         : robots_dot_txt_(robots_dot_txt), time_limit_(robots_dot_txt_.getCrawlDelay("*") * 1000)
        {
            if (time_limit_.getLimit() < default_download_delay_time)
                time_limit_ = default_download_delay_time;
            else if (time_limit_.getLimit() > max_download_delay_time)
                time_limit_ = max_download_delay_time;

            time_limit_.restart();
        }
        DelayParams(const TimeLimit &time_limit, const unsigned default_download_delay_time,
                    const unsigned max_download_delay_time)
         : time_limit_(time_limit)
        {
            if (time_limit_.getLimit() < default_download_delay_time)
                time_limit_ = default_download_delay_time;
            else if (time_limit_.getLimit() > max_download_delay_time)
                time_limit_ = max_download_delay_time;

            time_limit_.restart();
        }
        DelayParams(const DelayParams &rhs) = default;
    };

    struct DomainData {
        DelayParams delay_params_;
        std::deque<std::shared_ptr<DirectDownload::Tasklet>> active_direct_downloads_;
        std::deque<std::shared_ptr<DirectDownload::Tasklet>> queued_direct_downloads_;
        std::deque<std::shared_ptr<Crawling::Tasklet>> active_crawls_;
        std::deque<std::shared_ptr<Crawling::Tasklet>> queued_crawls_;
        std::deque<std::shared_ptr<RSS::Tasklet>> active_rss_feeds_;
        std::deque<std::shared_ptr<RSS::Tasklet>> queued_rss_feeds_;
    public:
        DomainData(const DelayParams &delay_params) : delay_params_(delay_params) {};
    };

    struct CachedDownloadData {
        std::string response_body_;
        unsigned response_code_;
        std::string error_message_;
    };

    static constexpr unsigned MAX_DIRECT_DOWNLOAD_TASKLETS = 50;
    static constexpr unsigned MAX_CRAWLING_TASKLETS        = 50;
    static constexpr unsigned MAX_RSS_TASKLETS             = 50;
    static constexpr unsigned DOWNLOAD_TIMEOUT             = 1000 * 30;      // in ms
    static constexpr unsigned MAX_CRAWL_TIMEOUT            = 1000 * 60 * 10; // in ms

    GlobalParams global_params_;
    pthread_t background_thread_;
    ThreadUtil::ThreadSafeCounter<unsigned> direct_download_tasklet_execution_counter_;
    ThreadUtil::ThreadSafeCounter<unsigned> crawling_tasklet_execution_counter_;
    ThreadUtil::ThreadSafeCounter<unsigned> rss_tasklet_execution_counter_;
    std::unordered_map<std::string, std::unique_ptr<DomainData>> domain_data_;
    std::unordered_map<std::string, CachedDownloadData> cached_download_data_;
    std::recursive_mutex cached_download_data_mutex_;
    std::deque<std::shared_ptr<DirectDownload::Tasklet>> direct_download_queue_buffer_;
    std::recursive_mutex direct_download_queue_buffer_mutex_;
    std::deque<std::shared_ptr<Crawling::Tasklet>> crawling_queue_buffer_;
    std::recursive_mutex crawling_queue_buffer_mutex_;
    std::deque<std::shared_ptr<RSS::Tasklet>> rss_queue_buffer_;
    std::recursive_mutex rss_queue_buffer_mutex_;
    Util::UploadTracker upload_tracker_;

    static void *BackgroundThreadRoutine(void * parameter);

    DelayParams generateDelayParams(const Url &url);
    DomainData *lookupDomainData(const Url &url, bool add_if_absent);
    void processQueueBuffers();
    void processDomainQueues(DomainData * const domain_data);
    void cleanupCompletedTasklets(DomainData * const domain_data);
public:
    DownloadManager(const GlobalParams &global_params);
    ~DownloadManager();

    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>> directDownload(const Util::HarvestableItem &source,
                                                                                                 const std::string &user_agent);
    std::unique_ptr<Util::Future<Crawling::Params, Crawling::Result>> crawl(const Util::HarvestableItem &source,
                                                                            const std::string &user_agent);

    std::unique_ptr<Util::Future<RSS::Params, RSS::Result>> rss(const Util::HarvestableItem &source,
                                                                const std::string &user_agent,
                                                                const std::string &feed_contents = "");
    void addToDownloadCache(const std::string &url, const std::string &response_body, const unsigned response_code,
                            const std::string &error_message);
    inline bool downloadInProgress() const {
        return direct_download_tasklet_execution_counter_ != 0 or crawling_tasklet_execution_counter_ != 0
               or rss_tasklet_execution_counter_ != 0;
    }
};


} // end namespace Download


} // end namespace ZoteroHarvester
