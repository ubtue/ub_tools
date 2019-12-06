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


#include <atomic>
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


enum class Operation { USE_TRANSLATION_SERVER, DIRECT_QUERY };


struct Params {
    Util::HarvestableItem download_item_;
    Url translation_server_url_;
    std::string user_agent_;
    bool ignore_robots_dot_txt_;
    unsigned time_limit_;
    Operation operation_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string &translation_server_url,
                    const std::string user_agent, const bool ignore_robots_dot_txt, const unsigned time_limit,
                    const Operation operation)
     : download_item_(download_item), translation_server_url_(translation_server_url), user_agent_(user_agent),
       ignore_robots_dot_txt_(ignore_robots_dot_txt), time_limit_(time_limit), operation_(operation) {}
};


struct Result {
    enum Flags {
        ITEM_ALREADY_DELIVERED  = 1 << 1,
        FROM_CACHE              = 1 << 2,
    };


    Util::HarvestableItem source_;
    Operation operation_;
    std::string response_body_;
    std::string response_header_;
    unsigned response_code_;
    std::string error_message_;
    unsigned flags_;
public:
    explicit Result(const Util::HarvestableItem &source, const Operation operation)
     : source_(source), operation_(operation), response_code_(0), flags_(0) {}
    Result(const Result &rhs) = default;

    inline bool downloadSuccessful() const { return response_code_ == 200 and error_message_.empty(); }
    inline bool itemAlreadyDelivered() const { return flags_ & ITEM_ALREADY_DELIVERED; }
    inline bool fromCache() const { return flags_ & FROM_CACHE; }
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
    unsigned per_crawl_url_time_limit_;
    unsigned total_crawl_time_limit_;
    bool ignore_robots_dot_txt_;
    Util::HarvestableItemManager * const harvestable_manager_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string user_agent, const unsigned per_crawl_url_time_limit,
                    const unsigned total_crawl_time_limit, const bool ignore_robots_dot_txt,
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


class Crawler {
    const Params &parameters_;
    TimeLimit total_crawl_time_limit_;
    ThreadSafeRegexMatcher url_ignore_matcher_;
    std::queue<std::string> url_queue_current_depth_;
    std::queue<std::string> url_queue_next_depth_;
    std::unordered_set<std::string> crawled_urls_;
    unsigned remaining_crawl_depth_;
    DownloadManager * const download_manager_;

    bool continueCrawling();
public:
    struct CrawlResult {
        enum OutgoingUrlFlag { MARK_FOR_CRAWLING, DO_NOT_CRAWL };

        std::string current_url_;
        std::vector<std::pair<std::string, OutgoingUrlFlag>> outgoing_urls_;
    };
public:
    explicit Crawler(const Params &parameters, DownloadManager * const download_manager,
                     const std::string &url_ignore_matcher_pattern = "(?i)\\.(js|css|bmp|pdf|jpg|gif|png|tif|tiff)(\\?[^?]*)?$");

    bool getNextPage(CrawlResult * const crawl_result);
    inline bool timeoutExceeded() const
        { return total_crawl_time_limit_.limitExceeded(); }
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
        unsigned timeout_download_request_;
        unsigned timeout_crawl_operation_;
        unsigned rss_feed_harvest_interval_;
        bool force_process_rss_feeds_with_no_pub_dates_;
        bool ignore_robots_txt_;
        bool force_downloads_;
        Util::HarvestableItemManager * const harvestable_manager_;
    public:
        GlobalParams(const Config::GlobalParams &config_global_params, Util::HarvestableItemManager * const harvestable_manager);
        GlobalParams(const GlobalParams &rhs) = default;
    };
private:
    struct DelayParams {
        RobotsDotTxt robots_dot_txt_;
        TimeLimit time_limit_;
    public:
        DelayParams(const std::string &robots_dot_txt, const unsigned default_download_delay_time,
                    const unsigned max_download_delay_time);
        DelayParams(const TimeLimit &time_limit, const unsigned default_download_delay_time,
                    const unsigned max_download_delay_time);
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
        Util::HarvestableItem source_;
        DirectDownload::Operation operation_;
        std::string response_body_;
    };

    static constexpr unsigned MAX_DIRECT_DOWNLOAD_TASKLETS = 50;
    static constexpr unsigned MAX_CRAWLING_TASKLETS        = 50;
    static constexpr unsigned MAX_RSS_TASKLETS             = 50;

    GlobalParams global_params_;
    pthread_t background_thread_;
    mutable std::atomic_bool stop_background_thread_;
    ThreadUtil::ThreadSafeCounter<unsigned> direct_download_tasklet_execution_counter_;
    ThreadUtil::ThreadSafeCounter<unsigned> crawling_tasklet_execution_counter_;
    ThreadUtil::ThreadSafeCounter<unsigned> rss_tasklet_execution_counter_;
    std::unordered_map<std::string, std::unique_ptr<DomainData>> domain_data_;
    std::unordered_multimap<std::string, CachedDownloadData> cached_download_data_;
    mutable std::recursive_mutex cached_download_data_mutex_;
    std::vector<std::shared_ptr<DirectDownload::Tasklet>> ongoing_direct_downloads_;
    mutable std::recursive_mutex ongoing_direct_downloads_mutex_;
    std::deque<std::shared_ptr<DirectDownload::Tasklet>> direct_download_queue_buffer_;
    mutable std::recursive_mutex direct_download_queue_buffer_mutex_;
    std::deque<std::shared_ptr<Crawling::Tasklet>> crawling_queue_buffer_;
    mutable std::recursive_mutex crawling_queue_buffer_mutex_;
    std::deque<std::shared_ptr<RSS::Tasklet>> rss_queue_buffer_;
    mutable std::recursive_mutex rss_queue_buffer_mutex_;
    Util::UploadTracker upload_tracker_;

    static void *BackgroundThreadRoutine(void * parameter);

    DelayParams generateDelayParams(const Url &url);
    DomainData *lookupDomainData(const Url &url, bool add_if_absent);
    void processQueueBuffers();
    void processDomainQueues(DomainData * const domain_data);
    void cleanupCompletedTasklets(DomainData * const domain_data);
    void cleanupOngoingDownloadsBackingStore();
    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
        newFutureFromOngoingDownload(const Util::HarvestableItem &source, const DirectDownload::Operation operation) const;
public:
    DownloadManager(const GlobalParams &global_params);
    ~DownloadManager();

    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>> directDownload(const Util::HarvestableItem &source,
                                                                                                 const std::string &user_agent,
                                                                                                 const DirectDownload::Operation operation,
                                                                                                 const unsigned timeout = 0);
    std::unique_ptr<Util::Future<Crawling::Params, Crawling::Result>> crawl(const Util::HarvestableItem &source,
                                                                            const std::string &user_agent);

    std::unique_ptr<Util::Future<RSS::Params, RSS::Result>> rss(const Util::HarvestableItem &source,
                                                                const std::string &user_agent,
                                                                const std::string &feed_contents = "");
    void addToDownloadCache(const Util::HarvestableItem &source, const std::string &url, const std::string &response_body,
                            const DirectDownload::Operation operation);
    std::unique_ptr<DirectDownload::Result> fetchFromDownloadCache(const Util::HarvestableItem &source,
                                                                   const DirectDownload::Operation operation) const;
    bool downloadInProgress() const;
};


} // end namespace Download


} // end namespace ZoteroHarvester
