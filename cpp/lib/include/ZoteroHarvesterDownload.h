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
#include <queue>
#include <set>
#include <unordered_map>
#include "Downloader.h"
#include "JSON.h"
#include "RegexMatcher.h"
#include "RobotsDotTxt.h"
#include "SyndicationFormat.h"
#include "ThreadUtil.h"
#include "TimeLimit.h"
#include "Url.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace ZoteroHarvester {


// This namespace contains classes that facilitate the different harvesting operations.
// The operations are split into three categories: DirectDownload, RSS and Crawl.
// All operations correspond to a HarvestableItem that represents the context of the download
// and are orchestrated by a single dispatcher (DownloadManager) that implements rate-limiting and caching.
// Individual operations execute concurrently to ensure a steady throughput.
namespace Download {


// Temporarily reduced in order to see if this results in fewer errors
static constexpr unsigned MAX_DIRECT_DOWNLOAD_TASKLETS = 5;
static constexpr unsigned MAX_CRAWLING_TASKLETS        = 5;
static constexpr unsigned MAX_RSS_TASKLETS             = 5;
static constexpr unsigned MAX_APIQUERY_TASKLETS        = 1;
static constexpr unsigned MAX_EMAILCRAWL_TASKLETS      = 5;
// Set to 20 empirically. Larger numbers increase the incidence of the
// translation server bug that returns an empty/broken response.
static constexpr unsigned MAX_CONCURRENT_TRANSLATION_SERVER_REQUESTS = 15;


class DownloadManager;

// Given a HarvestableItem, i.e, a URL, either download the resource at the location directly or
// use the Zotero Translation Server to extract metadata from said resource. Successful downloads
// and successfully retrieved metadata are cached locally to reduce the number of outbound requests.
// Returns the remote server's response with additional extra data.
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
    unsigned items_skipped_since_already_delivered_; // Trace multiple results from ZTS
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


// Given an entry point URL, download the web page/resource at the location, parse the markup for outgoing
// links, determine which links have harvestable metadata and which require further crawling and repeat the
// process for each link until a stopping condition is reached. Returns a vector of futures that yield the
// metadata harvested from URLs determined to be harvestable.
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
    unsigned num_crawled_successful_;
    unsigned num_crawled_unsuccessful_;
    unsigned num_crawled_cache_hits_;
    unsigned num_queued_for_harvest_;
    unsigned num_skipped_since_already_delivered_;
    std::vector<std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>> downloaded_items_;
public:
    explicit Result()
        : num_crawled_successful_(0), num_crawled_unsuccessful_(0), num_crawled_cache_hits_(0),
          num_queued_for_harvest_(0), num_skipped_since_already_delivered_(0) {}
    Result(const Result &rhs) = delete;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;
    const Util::UploadTracker &upload_tracker_;
    bool force_downloads_;

    void run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
            DownloadManager * const download_manager, const Util::UploadTracker &upload_tracker,
            std::unique_ptr<Params> parameters, const bool force_downloads);
    virtual ~Tasklet() override = default;
};


class Crawler {
    const Params &parameters_;
    TimeLimit total_crawl_time_limit_;
    ThreadSafeRegexMatcher url_ignore_matcher_;
    std::queue<std::string> url_queue_current_depth_;
    std::queue<std::string> url_queue_next_depth_;
    std::unordered_set<std::string> crawled_urls_;
    unsigned num_crawled_successful_;
    unsigned num_crawled_unsuccessful_;
    unsigned num_crawled_cache_hits_;
    unsigned remaining_crawl_depth_;
    DownloadManager * const download_manager_;

    bool continueCrawling();
public:
    // Stores the details of the last page that was crawled.
    struct CrawlResult {
        enum OutgoingUrlFlag { MARK_FOR_CRAWLING, DO_NOT_CRAWL };


        // URL of the page that was crawled.
        std::string current_url_;

        // Outgoing URLs found in the crawled page.
        // All URLs are marked for crawling by default.
        std::vector<std::pair<std::string, OutgoingUrlFlag>> outgoing_urls_;
    };
public:
    explicit Crawler(const Params &parameters, DownloadManager * const download_manager,
                     const std::string &url_ignore_matcher_pattern = "(?i)\\.(js|css|bmp|pdf|jpg|gif|png|tif|tiff)(\\?[^?]*)?$");
public:
    // Attempts to download the next queued page and extracts outgoing URLs in it.
    // If successful, returns true and 'crawl_result' will updated with the page's outgoing URLs.
    // The caller can determine which outgoing URLs are at be queued for further crawling by
    // updating each URL's flag and passing the updated CrawlResult instance back to the next
    // function call.
    bool getNextPage(CrawlResult * const crawl_result);

    inline bool timeoutExceeded() const
        { return total_crawl_time_limit_.limitExceeded(); }
    inline unsigned numUrlsSuccessfullyCrawled() const
        { return num_crawled_successful_; }
    inline unsigned numUrlsUnsuccessfullyCrawled() const
        { return num_crawled_unsuccessful_; }
    inline unsigned numCacheHitsForCrawls() const
        { return num_crawled_cache_hits_; }
};


} // end namespace Crawling


// Given a link to a RSS feed, download it and parse its contents. Determine if the feed has been updated
// and continue harvesting its individual items. Returns a vector of futures that yield the metadata of
// URLs that were harvested.
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
    unsigned items_skipped_since_already_delivered_;
    std::vector<std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>> downloaded_items_;
public:
    explicit Result(): items_skipped_since_already_delivered_(0) {}
    Result(const Result &rhs) = delete;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;
    const Util::UploadTracker &upload_tracker_;
    bool force_downloads_;

    void run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
            std::unique_ptr<Params> parameters, const Util::UploadTracker &upload_tracker, const bool force_downloads);
    virtual ~Tasklet() override = default;
};


} // end namespace RSS


namespace ApiQuery {

struct Params {
    Util::HarvestableItem download_item_;
    Url translation_server_url_;
    std::string user_agent_;
    bool ignore_robots_dot_txt_;
    unsigned time_limit_;
    DirectDownload::Operation operation_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string &translation_server_url,
                    const std::string user_agent, const bool ignore_robots_dot_txt, const unsigned time_limit,
                    const DirectDownload::Operation operation)
        : download_item_(download_item), translation_server_url_(translation_server_url), user_agent_(user_agent),
          ignore_robots_dot_txt_(ignore_robots_dot_txt), time_limit_(time_limit), operation_(operation) {}
    operator DirectDownload::Params() const { return DirectDownload::Params(download_item_, translation_server_url_, user_agent_,
                                                                           ignore_robots_dot_txt_, time_limit_, operation_); }
};


class Tasklet : public Util::Tasklet<DirectDownload::Params, DirectDownload::Result> {
    DownloadManager * const download_manager_;
    const Util::UploadTracker &upload_tracker_;
    bool force_downloads_;
    void run(const DirectDownload::Params &parameters, DirectDownload::Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
            DownloadManager * const download_manager, const Util::UploadTracker &upload_tracker,
            const std::unique_ptr<DirectDownload::Params> parameters, const bool force_downloads);
    virtual ~Tasklet() override = default;
};


} // end namespace ApiQuery


namespace EmailCrawl {

struct Params {
    Util::HarvestableItem download_item_;
    std::string user_agent_;
    unsigned per_crawl_url_time_limit_;
    unsigned total_crawl_time_limit_;
    bool ignore_robots_dot_txt_;
    Util::HarvestableItemManager * const harvestable_manager_;
    std::vector<std::string> emailcrawl_mboxes_;
public:
    explicit Params(const Util::HarvestableItem &download_item, const std::string user_agent, const unsigned per_crawl_url_time_limit,
                    const unsigned total_crawl_time_limit, const bool ignore_robots_dot_txt,
                    Util::HarvestableItemManager * const harvestable_manager, std::vector<std::string> emailcrawl_mboxes)
        : download_item_(download_item), user_agent_(user_agent), per_crawl_url_time_limit_(per_crawl_url_time_limit),
          total_crawl_time_limit_(total_crawl_time_limit), ignore_robots_dot_txt_(ignore_robots_dot_txt),
          harvestable_manager_(harvestable_manager), emailcrawl_mboxes_(emailcrawl_mboxes) {}
};


struct Result {
    unsigned num_email_crawled_successful_;
    unsigned num_email_crawled_unsuccessful_;
    unsigned num_email_crawled_cache_hits_;
    unsigned num_email_queued_for_harvest_;
    unsigned num_email_skipped_since_already_delivered_;
    std::vector<std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>> downloaded_items_;
public:
    explicit Result()
        : num_email_crawled_successful_(0), num_email_crawled_unsuccessful_(0), num_email_crawled_cache_hits_(0),
          num_email_queued_for_harvest_(0), num_email_skipped_since_already_delivered_(0) {}
    Result(const Result &rhs) = delete;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    DownloadManager * const download_manager_;
    const Util::UploadTracker &upload_tracker_;
    bool force_downloads_;

    void run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
            DownloadManager * const download_manager, const Util::UploadTracker &upload_tracker,
            std::unique_ptr<Params> parameters, const bool force_downloads);
    virtual ~Tasklet() override = default;

};
}// end namespace EmailCrawl



// Orchestrates all downloads and manages the relevant state. Consumers of this class can
// queue downloads as if they were synchronous operations and await their results at a later
// point in time. RSS and Crawl operations are decomposed into individual DirectDownload operations
// wherever possible. DirectDownload operations are categorised based on their URLs' domain name.
// Each domain has its own queue for each type of operation and its corresponding rate-limiting
// parameters. The rate-limiter ensures that there is no more than one download executing per
// domain at a given point in time (unless overriden globally). Successful DirectDownload operations
// are cached.
//
// A background thread performs the necessary housekeeping related to moving operations between queues,
// tracking download delay parameters and cleaning up completed operations.
//
// The public interface provides non-blocking functions to queue the different download operations. Callers
// can pass the returned future objects around and wait on the result as required.
class DownloadManager {
public:
    struct GlobalParams {
        Url translation_server_url_;
        Config::DownloadDelayParams download_delay_params_;
        unsigned timeout_download_request_;
        unsigned timeout_crawl_operation_;
        bool ignore_robots_txt_;
        bool force_downloads_;
        Util::HarvestableItemManager * const harvestable_manager_;
    public:
        GlobalParams(const Config::GlobalParams &config_global_params, Util::HarvestableItemManager * const harvestable_manager);
        GlobalParams(const GlobalParams &rhs) = default;
    };
private:
    // Specifies the download delay parameters to be used by the rate-limiter for a given domain.
    // Attempts to read the domain's robots.txt file to retrieve the parameters and falls back to
    // defaults if need be.
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


    // Per-domain data that tracks active and queued operations.
    // Multiple queues are used as buffers to minimize contention.
    struct DomainData {
        DelayParams delay_params_;
        std::deque<std::shared_ptr<DirectDownload::Tasklet>> active_direct_downloads_;
        std::deque<std::shared_ptr<DirectDownload::Tasklet>> queued_direct_downloads_translation_server_;
        std::deque<std::shared_ptr<DirectDownload::Tasklet>> queued_direct_downloads_direct_query_;
        std::deque<std::shared_ptr<Crawling::Tasklet>> active_crawls_;
        std::deque<std::shared_ptr<Crawling::Tasklet>> queued_crawls_;
        std::deque<std::shared_ptr<RSS::Tasklet>> active_rss_feeds_;
        std::deque<std::shared_ptr<RSS::Tasklet>> queued_rss_feeds_;
        std::deque<std::shared_ptr<ApiQuery::Tasklet>> active_apiqueries_;
        std::deque<std::shared_ptr<ApiQuery::Tasklet>> queued_apiqueries_;
        std::deque<std::shared_ptr<EmailCrawl::Tasklet>> active_emailcrawls_;
        std::deque<std::shared_ptr<EmailCrawl::Tasklet>> queued_emailcrawls_;
    public:
        DomainData(const DelayParams &delay_params) : delay_params_(delay_params) {};
    };


    struct TaskletCounters {
        ThreadUtil::ThreadSafeCounter<unsigned> direct_download_tasklet_execution_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> crawling_tasklet_execution_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> rss_tasklet_execution_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> direct_downloads_translation_server_queue_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> direct_downloads_direct_query_queue_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> crawls_queue_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> rss_feeds_queue_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> apiquery_tasklet_execution_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> apiquery_queue_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> emailcrawl_tasklet_execution_counter_;
        ThreadUtil::ThreadSafeCounter<unsigned> emailcrawl_queue_counter_;
    };


    struct CachedDownloadData {
        Util::HarvestableItem source_;
        DirectDownload::Operation operation_;
        std::string response_body_;
    };


    GlobalParams global_params_;
    pthread_t background_thread_;
    mutable std::atomic_bool stop_background_thread_;
    std::unordered_map<std::string, std::unique_ptr<DomainData>> domain_data_;
    std::unordered_multimap<std::string, CachedDownloadData> cached_download_data_;
    std::vector<std::shared_ptr<DirectDownload::Tasklet>> ongoing_direct_downloads_;
    std::deque<std::shared_ptr<DirectDownload::Tasklet>> direct_download_queue_buffer_;
    std::deque<std::shared_ptr<Crawling::Tasklet>> crawling_queue_buffer_;
    std::deque<std::shared_ptr<RSS::Tasklet>> rss_queue_buffer_;
    std::deque<std::shared_ptr<ApiQuery::Tasklet>> apiquery_queue_buffer_;
    std::deque<std::shared_ptr<EmailCrawl::Tasklet>> emailcrawl_queue_buffer_;
    mutable std::recursive_mutex cached_download_data_mutex_;
    mutable std::recursive_mutex ongoing_direct_downloads_mutex_;
    mutable std::recursive_mutex direct_download_queue_buffer_mutex_;
    mutable std::recursive_mutex crawling_queue_buffer_mutex_;
    mutable std::recursive_mutex rss_queue_buffer_mutex_;
    mutable std::recursive_mutex apiquery_queue_buffer_mutex_;
    mutable std::recursive_mutex emailcrawl_queue_buffer_mutex_;
    Util::UploadTracker upload_tracker_;
    TaskletCounters tasklet_counters_;

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
public:
    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>> directDownload(const Util::HarvestableItem &source,
                                                                                                 const std::string &user_agent,
                                                                                                 const DirectDownload::Operation operation,
                                                                                                 const unsigned timeout = 0);
    std::unique_ptr<Util::Future<Crawling::Params, Crawling::Result>> crawl(const Util::HarvestableItem &source,
                                                                            const std::string &user_agent);

    std::unique_ptr<Util::Future<RSS::Params, RSS::Result>> rss(const Util::HarvestableItem &source,
                                                                const std::string &user_agent,
                                                                const std::string &feed_contents = "");
    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>> apiQuery(const Util::HarvestableItem &source);
    std::unique_ptr<Util::Future<EmailCrawl::Params, EmailCrawl::Result>> emailCrawl(const Util::HarvestableItem &source,
                                                                                     const std::vector<std::string> &mbox_files,                                                                                                                         const std::string &user_agent);
    void addToDownloadCache(const Util::HarvestableItem &source, const std::string &url, const std::string &response_body,
                            const DirectDownload::Operation operation);
    std::unique_ptr<DirectDownload::Result> fetchFromDownloadCache(const Util::HarvestableItem &source,
                                                                   const DirectDownload::Operation operation) const;
    bool downloadInProgress() const;
    inline unsigned numActiveDirectDownloads() const
        { return tasklet_counters_.direct_download_tasklet_execution_counter_; }
    inline unsigned numActiveCrawls() const
        { return tasklet_counters_.crawling_tasklet_execution_counter_; }
    inline unsigned numActiveRssFeeds() const
        { return tasklet_counters_.rss_tasklet_execution_counter_; }
    inline unsigned numQueuedDirectDownloads() const
        { return tasklet_counters_.direct_downloads_direct_query_queue_counter_ +
                 tasklet_counters_.direct_downloads_translation_server_queue_counter_; }
    inline unsigned numQueuedCrawls() const
        { return tasklet_counters_.crawls_queue_counter_; }
    inline unsigned numQueuedRssFeeds() const
        { return tasklet_counters_.rss_feeds_queue_counter_; }
};


} // end namespace Download


} // end namespace ZoteroHarvester
