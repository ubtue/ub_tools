/** \brief Classes related to the Zotero Harvester's download API
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "JSON.h"
#include "StringUtil.h"
#include "WebUtil.h"
#include "util.h"
#include "ZoteroHarvesterDownload.h"
#include "ZoteroHarvesterZederInterop.h"


namespace ZoteroHarvester {


namespace Download {


namespace DirectDownload {


ThreadUtil::Semaphore translation_server_request_semaphore(MAX_CONCURRENT_TRANSLATION_SERVER_REQUESTS);


void PostToTranslationServer(const Url &translation_server_url, const std::string &translation_server_endpoint, const unsigned time_limit,
                             const std::string &user_agent,
                             const std::string &request_body, const bool request_is_json, std::string * const response_body,
                             unsigned * response_code, std::string * const error_message)
{
    // throttle requests to prevent the harvester from overwhelming the translation server
    TimeLimit translation_server_wait_timeout(time_limit * 3);

    while (not translation_server_request_semaphore.tryWait()) {
        if (translation_server_wait_timeout.limitExceeded()) {
            *error_message = "translation server busy";
            LOG_WARNING("failed to fetch response from the translation server! error: timed out due to busy server");
            return;
        }
        ::usleep(16 * 1000);
    }

    Downloader::Params downloader_params;
    downloader_params.user_agent_ = user_agent;
    downloader_params.additional_headers_ = { "Accept: application/json",
                                              std::string("Content-Type: ") + (request_is_json ? "application/json" : "text/plain") };
    downloader_params.post_data_ = request_body;

    try {
        LOG_DEBUG("Sending request to translation server: " + request_body);
        const Url endpoint_url(translation_server_url.toString() + '/' + translation_server_endpoint);
        Downloader downloader(endpoint_url, downloader_params, time_limit);
        if (downloader.anErrorOccurred()) {
            *response_code = downloader.getLastErrorCode();
            *error_message = downloader.getLastErrorMessage();
            LOG_WARNING("failed to fetch response from the translation server! error: " + *error_message);
        } else {
            *response_body = downloader.getMessageBody();
            *response_code = downloader.getResponseCode();
            LOG_DEBUG("Successful response from translation server for request: " + request_body + ", response code: " + std::to_string(downloader.getResponseCode()));
        }
    } catch (std::runtime_error &err) {
        LOG_WARNING("failed to fetch response from the translation server! runtime error: " + std::string(err.what()));
    } catch (...) {
        LOG_WARNING("failed to fetch response from the translation server! unknown error");
    }

    translation_server_request_semaphore.post();
}


void QueryRemoteUrl(const std::string &url, const unsigned time_limit, const bool ignore_robots_dot_txt,
                    const std::string &user_agent, std::string * const response_header, std::string * const response_body,
                    unsigned * response_code, std::string * const error_message)
{
    LOG_DEBUG("Querying remote URL: " + url);
    Downloader::Params downloader_params;
    downloader_params.user_agent_ = user_agent;
    downloader_params.honour_robots_dot_txt_ = not ignore_robots_dot_txt;
    Downloader downloader(url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *response_code = 0;
        *error_message = downloader.getLastErrorMessage();

        LOG_WARNING("failed to fetch response from remote url '" + url + "'! error: " + *error_message);
        return;
    }

    LOG_DEBUG("Query for remote URL successful: " + url);
    *response_body = downloader.getMessageBody();
    *response_header = downloader.getMessageHeader();
    *response_code = downloader.getResponseCode();
}


void Tasklet::run(const Params &parameters, Result * const result) {
    if (parameters.operation_ == Operation::USE_TRANSLATION_SERVER)
        LOG_INFO("Harvesting URL " + parameters.download_item_.toString());
    else
        LOG_INFO("Downloading URL " + parameters.download_item_.toString());

    auto cached_result(download_manager_->fetchFromDownloadCache(parameters.download_item_, parameters.operation_));
    if (cached_result != nullptr) {
        LOG_INFO("Returning cached result");

        result->response_body_ = cached_result->response_body_;
        result->response_header_ = cached_result->response_header_;
        result->response_code_ = cached_result->response_code_;
        return;
    }

    if (parameters.operation_ == Operation::USE_TRANSLATION_SERVER) {
        PostToTranslationServer(parameters.translation_server_url_, "web", parameters.time_limit_, parameters.user_agent_,
                                parameters.download_item_.url_, /* request_is_json = */ false, &result->response_body_,
                                &result->response_code_, &result->error_message_);

        // 300 => multiple matches found, try to harvest children (send the response_body right back to the server to get all of them)
        if (result->response_code_ == 300) {
            LOG_DEBUG("multiple articles found => trying to harvest children");
            //LOG_DEBUG("\tresponse: " + result->response_body_);

            PostToTranslationServer(parameters.translation_server_url_, "web", parameters.time_limit_ * 2, parameters.user_agent_,
                                    result->response_body_, /* request_is_json = */ true, &result->response_body_,
                                    &result->response_code_, &result->error_message_);

            // additionally cache individual items in the response
            if (result->downloadSuccessful()) {
                std::shared_ptr<JSON::JSONNode> tree_root;
                JSON::Parser json_parser(result->response_body_);

                if (json_parser.parse(&tree_root)) {
                    unsigned num_individual_items(0);
                    const auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));

                    for (const auto &entry : *JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root)) {
                        const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
                        const auto url(json_object->getOptionalStringValue("url"));

                        // this appears to happen randomly when the translation server is processing multiple requests
                        if (array_node->size() == 1 and url == parameters.download_item_.url_.toString()) {
                            result->error_message_ = "translation server returned an invalid multiple-match response!";
                            result->response_code_ = 500;
                            break;
                        }

                        if (url.empty())
                            continue;

                        // the response must always be an array of objects
                        std::string json_string("[" + json_object->toString() + "]");

                        LOG_DEBUG("caching download for URL " + url + " @ item " + parameters.download_item_.toString());
                        //LOG_DEBUG("\tresponse: " + json_string + "\n");

                        download_manager_->addToDownloadCache(parameters.download_item_, url, json_string,
                                                              parameters.operation_);
                        ++num_individual_items;
                    }

                    if (num_individual_items > 0)
                        LOG_DEBUG("cached " + std::to_string(num_individual_items) + " items from a multi-result response");
                }
            }
        }
    } else {
        QueryRemoteUrl(parameters.download_item_.url_, parameters.time_limit_, parameters.ignore_robots_dot_txt_,
                       parameters.user_agent_, &result->response_header_, &result->response_body_, &result->response_code_,
                       &result->error_message_);
    }

    if (result->downloadSuccessful()) {
        download_manager_->addToDownloadCache(parameters.download_item_, parameters.download_item_.url_.toString(),
                                              result->response_body_, parameters.operation_);
        if (parameters.operation_ == Operation::USE_TRANSLATION_SERVER)
            LOG_INFO("Harvest successful");
        else
            LOG_INFO("Download successful");
    }
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 std::unique_ptr<Params> parameters)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "DirectDownload: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::unique_ptr<Result>(new Result(parameters->download_item_, parameters->operation_)),
                                 std::move(parameters), ResultPolicy::COPY),
   download_manager_(download_manager) {}


} // end namespace DirectDownload


namespace Crawling {


void Tasklet::run(const Params &parameters, Result * const result) {
    LOG_INFO("Crawling URL " + parameters.download_item_.toString());

    Crawler crawler(parameters, download_manager_);
    Crawler::CrawlResult crawl_result;
    std::unordered_set<std::string> queued_urls;

    while (not crawler.timeoutExceeded() and crawler.getNextPage(&crawl_result)) {
        for (auto &outgoing_url : crawl_result.outgoing_urls_) {
            bool harvest_url(queued_urls.find(outgoing_url.first) == queued_urls.end()
                             and (parameters.download_item_.journal_.crawl_params_.extraction_regex_ == nullptr
                             or parameters.download_item_.journal_.crawl_params_.extraction_regex_->match(outgoing_url.first)));
            bool crawl_url(parameters.download_item_.journal_.crawl_params_.crawl_url_regex_ == nullptr
                           or parameters.download_item_.journal_.crawl_params_.crawl_url_regex_->match(outgoing_url.first));

            if (harvest_url) {
                if (not force_downloads_ and upload_tracker_.urlAlreadyInDatabase(outgoing_url.first, /*delivery_states_to_ignore=*/Util::UploadTracker::DELIVERY_STATES_TO_RETRY)) {
                    LOG_INFO("Skipping already delivered URL: " + outgoing_url.first);
                    ++result->num_skipped_since_already_delivered_;
                    continue;
                }
                const auto new_item(parameters.harvestable_manager_->newHarvestableItem(outgoing_url.first,
                                                                                        parameters.download_item_.journal_));
                result->downloaded_items_.emplace_back(download_manager_->directDownload(new_item, parameters.user_agent_,
                                                                                         DirectDownload::Operation::USE_TRANSLATION_SERVER));
                queued_urls.emplace(outgoing_url.first);
                ++result->num_queued_for_harvest_;
            }

            outgoing_url.second = crawl_url ? Crawler::CrawlResult::MARK_FOR_CRAWLING : Crawler::CrawlResult::DO_NOT_CRAWL;
        }
    }

    if (crawler.timeoutExceeded())
        LOG_WARNING("process timed-out - not all URLs were crawled");

    result->num_crawled_successful_ = crawler.numUrlsSuccessfullyCrawled();
    result->num_crawled_unsuccessful_ = crawler.numUrlsUnsuccessfullyCrawled();
    result->num_crawled_cache_hits_ = crawler.numCacheHitsForCrawls();

    LOG_INFO("crawled " + std::to_string(result->num_crawled_successful_) + " URLs, queued "
             + std::to_string(result->num_queued_for_harvest_) + " URLs for extraction");
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 const Util::UploadTracker &upload_tracker, std::unique_ptr<Params> parameters, const bool force_downloads)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "Crawling: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::unique_ptr<Result>(new Result()), std::move(parameters), ResultPolicy::YIELD),
   download_manager_(download_manager), upload_tracker_(upload_tracker), force_downloads_(force_downloads) {}


bool Crawler::continueCrawling() {
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

        --remaining_crawl_depth_;
        url_queue_current_depth_.swap(url_queue_next_depth_);
        url_queue_next_depth_ = std::queue<std::string>();
    }

    return true;
}


Crawler::Crawler(const Params &parameters, DownloadManager * const download_manager, const std::string &url_ignore_matcher_pattern)
 : parameters_(parameters), total_crawl_time_limit_(parameters.total_crawl_time_limit_),
   url_ignore_matcher_(url_ignore_matcher_pattern, ThreadSafeRegexMatcher::Option::CASE_INSENSITIVE),
   num_crawled_successful_(0), num_crawled_unsuccessful_(0), num_crawled_cache_hits_(0),
   remaining_crawl_depth_(parameters.download_item_.journal_.crawl_params_.max_crawl_depth_), download_manager_(download_manager)
{
    url_queue_current_depth_.push(parameters.download_item_.url_.toString());
}


bool Crawler::getNextPage(CrawlResult * const crawl_result) {
    // enqueue outgoing URLs from the previous crawl that were marked for crawling
    if (not crawl_result->outgoing_urls_.empty() and remaining_crawl_depth_ > 0) {
        for (const auto &url : crawl_result->outgoing_urls_) {
            if (url.second == CrawlResult::OutgoingUrlFlag::MARK_FOR_CRAWLING) {
                url_queue_next_depth_.emplace(url.first);
                LOG_DEBUG("queued URL for further crawling: '" + url.first + "'");
            } else
                LOG_DEBUG("skipped URL: '" + url.first + "'");
        }
    }

    if (not continueCrawling())
        return false;

    const auto next_url(url_queue_current_depth_.front());
    url_queue_current_depth_.pop();

    if (url_ignore_matcher_.match(next_url)) {
        LOG_WARNING("Skipping URL: " + next_url + ": URL contains ignorable data");
        return continueCrawling();
    } else if (crawled_urls_.find(next_url) != crawled_urls_.end()) {
        LOG_WARNING("Skipping URL: " + next_url + ": URL was already crawled");
        return continueCrawling();
    }

    // request a download of the URL and await the response
    const auto new_download_item(parameters_.harvestable_manager_->newHarvestableItem(next_url, parameters_.download_item_.journal_));
    auto future(download_manager_->directDownload(new_download_item, parameters_.user_agent_,
                                                  DirectDownload::Operation::DIRECT_QUERY, parameters_.per_crawl_url_time_limit_));
    future->await();
    if (not future->hasResult() or not future->getResult().downloadSuccessful()) {
        LOG_WARNING("Unsuccessful crawl attempt for " + next_url);
        ++num_crawled_unsuccessful_;
        return continueCrawling();
    }

    const auto &download_result(future->getResult());
    crawled_urls_.emplace(next_url);
    ++num_crawled_successful_;
    if (download_result.fromCache())
        ++num_crawled_cache_hits_;

    // extract outgoing URLs from the downloaded URL
    unsigned EXTRACT_URL_FLAGS(WebUtil::IGNORE_DUPLICATE_URLS | WebUtil::IGNORE_LINKS_IN_IMG_TAGS
                                           | WebUtil::REMOVE_DOCUMENT_RELATIVE_ANCHORS
                                           | WebUtil::CLEAN_UP_ANCHOR_TEXT
                                           | WebUtil::ATTEMPT_TO_EXTRACT_JAVASCRIPT_URLS);

    const bool url_is_file(StringUtil::StartsWith(next_url, "file://", true /* case insensitive */));
    EXTRACT_URL_FLAGS |= url_is_file ? 0 : WebUtil::KEEP_LINKS_TO_SAME_MAJOR_SITE_ONLY;

    std::vector<WebUtil::UrlAndAnchorTexts> urls_and_anchor_texts;
    WebUtil::ExtractURLs(download_result.response_body_, url_is_file ? "" : next_url, WebUtil::ABSOLUTE_URLS, &urls_and_anchor_texts, EXTRACT_URL_FLAGS);

    crawl_result->current_url_ = next_url;
    crawl_result->outgoing_urls_.clear();

    // mark all outgoing URLs for crawling, by default
    for (const auto &url_and_anchor_texts : urls_and_anchor_texts) {
        crawl_result->outgoing_urls_.emplace_back(url_and_anchor_texts.getUrl(),
                                                  CrawlResult::OutgoingUrlFlag::MARK_FOR_CRAWLING);
    }

    if (crawl_result->outgoing_urls_.empty())
        return continueCrawling();
    else
        return true;
}


} // end namespace Crawling


namespace RSS {


void Tasklet::run(const Params &parameters, Result * const result) {
    LOG_INFO("Harvesting feed " + parameters.download_item_.toString());

    std::unique_ptr<SyndicationFormat> syndication_format;
    std::string feed_contents, syndication_format_parse_err_msg;
    SyndicationFormat::AugmentParams syndication_format_augment_parameters;
    syndication_format_augment_parameters.strptime_format_ = parameters.download_item_.journal_.strptime_format_string_;

    if (not parameters.feed_contents_.empty())
        feed_contents = parameters.feed_contents_;
    else {
        Downloader::Params downloader_params;
        downloader_params.user_agent_ = parameters.user_agent_;

        Downloader downloader(parameters.download_item_.url_.toString(), downloader_params);
        if (downloader.anErrorOccurred()) {
            LOG_WARNING("could not download RSS feed '" + parameters.download_item_.url_.toString() + "'!");
            return;
        }

        feed_contents = downloader.getMessageBody();
    }

    syndication_format.reset(SyndicationFormat::Factory(feed_contents, syndication_format_augment_parameters,
                             &syndication_format_parse_err_msg).release());

    if (syndication_format == nullptr) {
        LOG_WARNING("problem parsing XML document for RSS feed '" + parameters.download_item_.url_.toString() + "': "
                    + syndication_format_parse_err_msg);
        return;
    }

    unsigned num_items_queued(0);
    for (const auto &item : *syndication_format) {
        if (not force_downloads_ and upload_tracker_.urlAlreadyInDatabase(item.getLink(), /*delivery_states_to_ignore=*/Util::UploadTracker::DELIVERY_STATES_TO_RETRY)) {
            LOG_INFO("Skipping already delivered URL: " + item.getLink());
            ++result->items_skipped_since_already_delivered_;
            continue;
        }

        const auto new_download_item(parameters.harvestable_manager_->newHarvestableItem(item.getLink(), parameters.download_item_.journal_));
        result->downloaded_items_.emplace_back(download_manager_->directDownload(new_download_item, parameters.user_agent_,
                                                                                 DirectDownload::Operation::USE_TRANSLATION_SERVER));
        ++num_items_queued;
    }

    LOG_INFO("Queued " + std::to_string(num_items_queued) + " items");
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 std::unique_ptr<Params> parameters, const Util::UploadTracker &upload_tracker, const bool force_downloads)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "RSS: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::unique_ptr<Result>(new Result()), std::move(parameters), ResultPolicy::YIELD),
   download_manager_(download_manager), upload_tracker_(upload_tracker), force_downloads_(force_downloads) {}


} // end namespace RSS


namespace ApiQuery {

const unsigned MAX_PER_REQUEST_DOWNLOADS(400);

void SelectNonExistingEntriesFromZTSMultiple(const std::string &all_items_object, std::string * const filtered_items_object, const Util::UploadTracker &upload_tracker,
                                             const bool force_downloads, unsigned * const all_items_num, unsigned * const filtered_items_num) {
     std::shared_ptr<JSON::JSONNode> tree_root;
     JSON::Parser json_parser(all_items_object);

     if (not json_parser.parse(&tree_root))
         LOG_ERROR("Could not appropriately parse " + all_items_object);

     auto select_object(JSON::JSONNode::CastToObjectNodeOrDie("select_object", tree_root));
     auto items(select_object->getObjectNode("items"));
     const std::shared_ptr<JSON::ObjectNode> new_items(new JSON::ObjectNode());
     *all_items_num = 0;
     *filtered_items_num = 0;
     for (const auto &doi_and_index : *items) {
         ++(*all_items_num);
         // Hard limit for number of downloads in one batch since we encounter problems...
         if (*filtered_items_num >= MAX_PER_REQUEST_DOWNLOADS)
             break;

         // Handling only here to get valid stats
         if (force_downloads) {
             new_items->insert(doi_and_index.first, doi_and_index.second);
             ++(*filtered_items_num);
             continue;
         }

         if (not upload_tracker.urlAlreadyInDatabase(doi_and_index.first, /*delivery_states_to_ignore=*/ Util::UploadTracker::DELIVERY_STATES_TO_RETRY)) {
             new_items->insert(doi_and_index.first, doi_and_index.second);
             ++(*filtered_items_num);
         }
     }

     select_object->remove("items");
     select_object->insert("items", new_items);
     *filtered_items_object = select_object->toString();
}


void Tasklet::run(const DirectDownload::Params &parameters, DirectDownload::Result * const result) {

    DirectDownload::PostToTranslationServer(parameters.translation_server_url_, "searchmultiple", parameters.time_limit_, parameters.user_agent_,
                            parameters.download_item_.url_, /* request_is_json = */ false, &result->response_body_,
                            &result->response_code_, &result->error_message_);

    // 300 => multiple matches found, try to harvest children
    if (result->response_code_ == 300) {
        LOG_DEBUG("Received expected multiple entries => harvest individual results");

        std::string filtered_multiple_object;
        unsigned all_items_num;
        unsigned filtered_items_num;

        SelectNonExistingEntriesFromZTSMultiple(result->response_body_, &filtered_multiple_object, upload_tracker_, force_downloads_, &all_items_num, &filtered_items_num);
        LOG_INFO("NEW REQUEST: " + filtered_multiple_object);
        if (filtered_items_num == 0) {
            LOG_INFO("No non-delivered items detected - not sending back any JSON object");
            result->items_skipped_since_already_delivered_ += all_items_num;
            return;
        }

        result->items_skipped_since_already_delivered_ += (all_items_num - filtered_items_num);

        DirectDownload::PostToTranslationServer(parameters.translation_server_url_, "searchmultiple", parameters.time_limit_ * 100 /*give plenty of time */,
                                parameters.user_agent_,
                                filtered_multiple_object, /* request_is_json = */ true, &result->response_body_,
                                &result->response_code_, &result->error_message_);

        // additionally cache individual items in the response
        if (result->downloadSuccessful()) {
            std::shared_ptr<JSON::JSONNode> tree_root;
            JSON::Parser json_parser(result->response_body_);

            if (json_parser.parse(&tree_root)) {
                unsigned num_individual_items(0);
                const auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));

                for (const auto &entry : *JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root)) {
                    const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
                    const auto url(json_object->getOptionalStringValue("url"));

                    // this appears to happen randomly when the translation server is processing multiple requests
                    if (array_node->size() == 1 and url == parameters.download_item_.url_.toString()) {
                        result->error_message_ = "translation server returned an invalid multiple-match response!";
                        result->response_code_ = 500;
                        break;
                    }

                    if (url.empty())
                        continue;

                    // the response must always be an array of objects
                    std::string json_string("[" + json_object->toString() + "]");

                    LOG_DEBUG("caching download for URL " + url + " @ item " + parameters.download_item_.toString());
                    LOG_DEBUG("\tresponse: " + json_string + "\n");

                    download_manager_->addToDownloadCache(parameters.download_item_, url, json_string,
                                                          parameters.operation_);
                    ++num_individual_items;
                }

                if (num_individual_items > 0)
                    LOG_DEBUG("cached " + std::to_string(num_individual_items) + " items from a multi-result response");
            }
        }
    }

    if (result->downloadSuccessful())
        download_manager_->addToDownloadCache(parameters.download_item_, parameters.download_item_.url_.toString(),
                                              result->response_body_, parameters.operation_);
}


Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 const Util::UploadTracker &upload_tracker, std::unique_ptr<DirectDownload::Params> parameters, const bool force_downloads)
 : Util::Tasklet<DirectDownload::Params, DirectDownload::Result>(instance_counter, parameters->download_item_,
                                 "ApiQuery: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::unique_ptr<DirectDownload::Result>(new DirectDownload::Result(parameters->download_item_, parameters->operation_)),
                                 std::move(parameters), ResultPolicy::COPY),
                                 download_manager_(download_manager), upload_tracker_(upload_tracker),  force_downloads_(force_downloads) {}


} // end namespace ApiQuery


namespace EmailCrawl {
void Tasklet::run(const Params &parameters, Result * const result) {
    LOG_INFO("Email Harvesting Crawling URL " + parameters.download_item_.toString());

    const auto crawl_item(parameters.harvestable_manager_->newHarvestableItem("file:///tmp/jewifilmnewmedi", parameters.download_item_.journal_));

    const Crawling::Params crawl_parameters(crawl_item, parameters.user_agent_,
                                      parameters.per_crawl_url_time_limit_, parameters.total_crawl_time_limit_,
                                      parameters.ignore_robots_dot_txt_, parameters.harvestable_manager_);

    Crawling::Crawler crawler(crawl_parameters, download_manager_);
    Crawling::Crawler::CrawlResult crawl_result;
    std::unordered_set<std::string> queued_urls;

    while (not crawler.timeoutExceeded() and crawler.getNextPage(&crawl_result)) {
        for (auto &outgoing_url : crawl_result.outgoing_urls_) {
            bool harvest_url(queued_urls.find(outgoing_url.first) == queued_urls.end()
                             and (parameters.download_item_.journal_.crawl_params_.extraction_regex_ == nullptr
                             or parameters.download_item_.journal_.crawl_params_.extraction_regex_->match(outgoing_url.first)));
            bool crawl_url(parameters.download_item_.journal_.crawl_params_.crawl_url_regex_ == nullptr
                           or parameters.download_item_.journal_.crawl_params_.crawl_url_regex_->match(outgoing_url.first));

            if (harvest_url) {
                if (not force_downloads_ and upload_tracker_.urlAlreadyInDatabase(outgoing_url.first, /*delivery_states_to_ignore=*/Util::UploadTracker::DELIVERY_STATES_TO_RETRY)) {
                    LOG_INFO("Skipping already delivered URL: " + outgoing_url.first);
                    ++result->num_email_skipped_since_already_delivered_;
                    continue;
                }
                const auto new_item(crawl_parameters.harvestable_manager_->newHarvestableItem(outgoing_url.first,
                                                                                        parameters.download_item_.journal_));
                result->downloaded_items_.emplace_back(download_manager_->directDownload(new_item, parameters.user_agent_,
                                                                                         DirectDownload::Operation::USE_TRANSLATION_SERVER));
                queued_urls.emplace(outgoing_url.first);
                ++result->num_email_queued_for_harvest_;
            }

            outgoing_url.second = crawl_url ? Crawling::Crawler::CrawlResult::MARK_FOR_CRAWLING : Crawling::Crawler::CrawlResult::DO_NOT_CRAWL;
        }
    }

    if (crawler.timeoutExceeded())
        LOG_WARNING("email crawl process timed-out - not all URLs were crawled");

    result->num_email_crawled_successful_ = crawler.numUrlsSuccessfullyCrawled();
    result->num_email_crawled_unsuccessful_ = crawler.numUrlsUnsuccessfullyCrawled();
    result->num_email_crawled_cache_hits_ = crawler.numCacheHitsForCrawls();

    LOG_INFO("email crawled " + std::to_string(result->num_email_crawled_successful_) + " URLs, queued "
             + std::to_string(result->num_email_queued_for_harvest_) + " URLs for extraction");
}


    Tasklet::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter, DownloadManager * const download_manager,
                 const Util::UploadTracker &upload_tracker, std::unique_ptr<Params> parameters, const bool force_downloads)
 : Util::Tasklet<Params, Result>(instance_counter, parameters->download_item_,
                                 "Email Crawling: " + parameters->download_item_.url_.toString(),
                                 std::bind(&Tasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                 std::unique_ptr<Result>(new Result()), std::move(parameters), ResultPolicy::YIELD),
   download_manager_(download_manager), upload_tracker_(upload_tracker), force_downloads_(force_downloads) {}
} // end namespace EmailCrawl


void DownloadManager::addToDownloadCache(const Util::HarvestableItem &source, const std::string &url,
                                         const std::string &response_body, const DirectDownload::Operation operation)
{
    std::lock_guard<std::recursive_mutex> download_cache_lock(cached_download_data_mutex_);

    auto cache_hit(cached_download_data_.equal_range(url));
    for (auto itr(cache_hit.first); itr != cache_hit.second; ++itr) {
        if (itr->second.operation_ == operation) {
            LOG_WARNING("cached download data collision for URL '" + url + "' (operation "
                        + std::to_string(static_cast<int>(operation)) + ")");
            LOG_WARNING("\told source: " + itr->second.source_.toString());
            LOG_WARNING("\tnew source: " + source.toString());

            if (itr->second.response_body_ != response_body) {
                LOG_WARNING("\tresponse mismatch!");
                LOG_WARNING("\t\told: " + itr->second.response_body_);
                LOG_WARNING("\t\tnew: " + response_body);
            }

            return;
        }
    }

    cached_download_data_.emplace(url, CachedDownloadData { source, operation, response_body });
}



DownloadManager::GlobalParams::GlobalParams(const Config::GlobalParams &config_global_params,
                                            Util::HarvestableItemManager * const harvestable_manager)
 : translation_server_url_(config_global_params.translation_server_url_),
   download_delay_params_(config_global_params.download_delay_params_),
   timeout_download_request_(config_global_params.timeout_download_request_),
   timeout_crawl_operation_(config_global_params.timeout_crawl_operation_),
   ignore_robots_txt_(false), force_downloads_(false), harvestable_manager_(harvestable_manager) {}


DownloadManager::DelayParams::DelayParams(const std::string &robots_dot_txt, const unsigned default_download_delay_time,
                                          const unsigned max_download_delay_time)
 : robots_dot_txt_(robots_dot_txt), time_limit_(robots_dot_txt_.getCrawlDelay("*") * 1000)
{
    if (time_limit_.getLimit() < default_download_delay_time)
        time_limit_ = default_download_delay_time;
    else if (time_limit_.getLimit() > max_download_delay_time)
        time_limit_ = max_download_delay_time;

    time_limit_.restart();
}


DownloadManager::DelayParams::DelayParams(const TimeLimit &time_limit, const unsigned default_download_delay_time,
                                          const unsigned max_download_delay_time)
 : time_limit_(time_limit)
{
    if (time_limit_.getLimit() < default_download_delay_time)
        time_limit_ = default_download_delay_time;
    else if (time_limit_.getLimit() > max_download_delay_time)
        time_limit_ = max_download_delay_time;

    time_limit_.restart();
}


void *DownloadManager::BackgroundThreadRoutine(void * parameter) {
    static const unsigned BACKGROUND_THREAD_SLEEP_TIME(32 * 1000);   // ms -> us

    DownloadManager * const download_manager(reinterpret_cast<DownloadManager *>(parameter));

    while (not download_manager->stop_background_thread_.load()) {
        download_manager->processQueueBuffers();

        // we don't need to lock access to the domain data store
        // as it's exclusively accessed in this background thread
        for (const auto &domain_entry : download_manager->domain_data_) {
            download_manager->processDomainQueues(domain_entry.second.get());
            download_manager->cleanupCompletedTasklets(domain_entry.second.get());
        }

        download_manager->cleanupOngoingDownloadsBackingStore();
        ::usleep(BACKGROUND_THREAD_SLEEP_TIME);
    }

    pthread_exit(nullptr);
}


DownloadManager::DelayParams DownloadManager::generateDelayParams(const Url &url) {
    const auto hostname(url.getAuthority());
    bool delay_is_default;
    bool max_delay_is_default;
    const auto default_delay(global_params_.download_delay_params_.getDefaultDelayForDomainOrDefault(hostname, &delay_is_default));
    const auto max_delay(global_params_.download_delay_params_.getMaxDelayForDomainOrDefault(hostname, &max_delay_is_default));

    if (not delay_is_default or not max_delay_is_default) {
        LOG_DEBUG("use configured domain-specific delay settings for domain '" + hostname + '"');
        return DelayParams(TimeLimit(default_delay), default_delay, max_delay);
    }

    Downloader robots_txt_downloader(url.getRobotsDotTxtUrl());
    if (robots_txt_downloader.anErrorOccurred()) {
        LOG_DEBUG("couldn't retrieve robots.txt for domain '" + hostname + "'");
        return DelayParams(TimeLimit(0), default_delay, max_delay);
    }

    DelayParams new_delay_params(robots_txt_downloader.getMessageBody(), default_delay,
                                 max_delay);

    LOG_DEBUG("set download-delay for domain '" + hostname + "' to " +
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
    // Enqueue the tasks in their domain-specific queues.
    {
        std::lock_guard<std::recursive_mutex> direct_download_queue_buffer_lock(direct_download_queue_buffer_mutex_);
        while (not direct_download_queue_buffer_.empty()) {
            std::shared_ptr<DirectDownload::Tasklet> tasklet(direct_download_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            if (tasklet->getParameter().operation_ == DirectDownload::Operation::DIRECT_QUERY) {
                domain_data->queued_direct_downloads_direct_query_.emplace_back(tasklet);
                ++tasklet_counters_.direct_downloads_direct_query_queue_counter_;
            } else {
                domain_data->queued_direct_downloads_translation_server_.emplace_back(tasklet);
                ++tasklet_counters_.direct_downloads_translation_server_queue_counter_;
            }

            direct_download_queue_buffer_.pop_front();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> crawling_queue_buffer_lock(crawling_queue_buffer_mutex_);
        while (not crawling_queue_buffer_.empty()) {
            std::shared_ptr<Crawling::Tasklet> tasklet(crawling_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_crawls_.emplace_back(tasklet);
            ++tasklet_counters_.crawls_queue_counter_;

            crawling_queue_buffer_.pop_front();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> rss_queue_buffer_lock(rss_queue_buffer_mutex_);
        while (not rss_queue_buffer_.empty()) {
            std::shared_ptr<RSS::Tasklet> tasklet(rss_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_rss_feeds_.emplace_back(tasklet);
            ++tasklet_counters_.rss_feeds_queue_counter_;

            rss_queue_buffer_.pop_front();
        }
    }


    {
        std::lock_guard<std::recursive_mutex> apiquery_queue_buffer_lock(apiquery_queue_buffer_mutex_);
        while (not apiquery_queue_buffer_.empty()) {
            std::shared_ptr<ApiQuery::Tasklet> tasklet(apiquery_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_apiqueries_.emplace_back(tasklet);
            ++tasklet_counters_.apiquery_queue_counter_;

            apiquery_queue_buffer_.pop_front();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> emailcrawl_queue_buffer_lock(emailcrawl_queue_buffer_mutex_);
        while (not emailcrawl_queue_buffer_.empty()) {
            std::shared_ptr<EmailCrawl::Tasklet> tasklet(emailcrawl_queue_buffer_.front());
            auto domain_data(lookupDomainData(tasklet->getParameter().download_item_.url_, /* add_if_absent = */ true));
            domain_data->queued_emailcrawls_.emplace_back(tasklet);
            ++tasklet_counters_.emailcrawl_queue_counter_;

            emailcrawl_queue_buffer_.pop_front();
        }
    }
}


void DownloadManager::processDomainQueues(DomainData * const domain_data) {
    // Apply download delays and create tasklets for downloads/crawls.
    const bool adhere_to_download_limit(not global_params_.ignore_robots_txt_);

    if (adhere_to_download_limit and not domain_data->delay_params_.time_limit_.limitExceeded())
        return;

    // DirectDownloads that do not involve querying the Zotero Translation Server
    // need to be prioritised over the former to prevent bottlenecks in Crawl operations.
    while (not domain_data->queued_direct_downloads_direct_query_.empty()
           and tasklet_counters_.direct_download_tasklet_execution_counter_ < MAX_DIRECT_DOWNLOAD_TASKLETS)
    {
        std::shared_ptr<DirectDownload::Tasklet> direct_download_tasklet(domain_data->queued_direct_downloads_direct_query_.front());
        domain_data->active_direct_downloads_.emplace_back(direct_download_tasklet);
        domain_data->queued_direct_downloads_direct_query_.pop_front();
        direct_download_tasklet->start();
        --tasklet_counters_.direct_downloads_direct_query_queue_counter_;

        if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

    while (not domain_data->queued_direct_downloads_translation_server_.empty()
           and tasklet_counters_.direct_download_tasklet_execution_counter_ < MAX_DIRECT_DOWNLOAD_TASKLETS)
    {
        std::shared_ptr<DirectDownload::Tasklet> direct_download_tasklet(domain_data->queued_direct_downloads_translation_server_.front());
        domain_data->active_direct_downloads_.emplace_back(direct_download_tasklet);
        domain_data->queued_direct_downloads_translation_server_.pop_front();
        direct_download_tasklet->start();
        --tasklet_counters_.direct_downloads_translation_server_queue_counter_;

        if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

    while (not domain_data->queued_crawls_.empty()
           and tasklet_counters_.crawling_tasklet_execution_counter_ < MAX_CRAWLING_TASKLETS)
    {
        std::shared_ptr<Crawling::Tasklet> crawling_tasklet(domain_data->queued_crawls_.front());
        domain_data->active_crawls_.emplace_back(crawling_tasklet);
        domain_data->queued_crawls_.pop_front();
        crawling_tasklet->start();
        --tasklet_counters_.crawls_queue_counter_;

        if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

    while(not domain_data->queued_rss_feeds_.empty()
          and tasklet_counters_.rss_tasklet_execution_counter_ < MAX_RSS_TASKLETS)
    {
        std::shared_ptr<RSS::Tasklet> rss_tasklet(domain_data->queued_rss_feeds_.front());
        domain_data->active_rss_feeds_.emplace_back(rss_tasklet);
        domain_data->queued_rss_feeds_.pop_front();
        rss_tasklet->start();
        --tasklet_counters_.rss_feeds_queue_counter_;

        if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

    while(not domain_data->queued_apiqueries_.empty()
          and tasklet_counters_.apiquery_tasklet_execution_counter_ < MAX_APIQUERY_TASKLETS)
    {
       std::shared_ptr<ApiQuery::Tasklet> apiquery_tasklet(domain_data->queued_apiqueries_.front());
       domain_data->active_apiqueries_.emplace_back(apiquery_tasklet);
       domain_data->queued_apiqueries_.pop_front();
       apiquery_tasklet->start();
       --tasklet_counters_.apiquery_queue_counter_;

       if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

    while(not domain_data->queued_emailcrawls_.empty()
          and tasklet_counters_.emailcrawl_tasklet_execution_counter_ < MAX_EMAILCRAWL_TASKLETS)
    {
       std::shared_ptr<EmailCrawl::Tasklet> emailcrawl_tasklet(domain_data->queued_emailcrawls_.front());
       domain_data->active_emailcrawls_.emplace_back(emailcrawl_tasklet);
       domain_data->queued_emailcrawls_.pop_front();
       emailcrawl_tasklet->start();
       --tasklet_counters_.emailcrawl_queue_counter_;

       if (adhere_to_download_limit) {
            domain_data->delay_params_.time_limit_.restart();
            return;
        }
    }

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

    for (auto iter(domain_data->active_apiqueries_.begin()); iter != domain_data->active_apiqueries_.end();) {
        if ((*iter)->isComplete()) {
            iter = domain_data->active_apiqueries_.erase(iter);
            continue;
        }
        ++iter;
    }


   for (auto iter(domain_data->active_emailcrawls_.begin()); iter != domain_data->active_emailcrawls_.end();) {
        if ((*iter)->isComplete()) {
            iter = domain_data->active_emailcrawls_.erase(iter);
            continue;
        }
        ++iter;
    }


}


void DownloadManager::cleanupOngoingDownloadsBackingStore() {
    std::lock_guard<std::recursive_mutex> ongoing_direct_downloads_lock(ongoing_direct_downloads_mutex_);

    for (auto iter(ongoing_direct_downloads_.begin()); iter != ongoing_direct_downloads_.end();) {
        if ((*iter)->isComplete()) {
            iter = ongoing_direct_downloads_.erase(iter);
            continue;
        }
        ++iter;
    }
}


std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
    DownloadManager::newFutureFromOngoingDownload(const Util::HarvestableItem &source,                                                                                                 const DirectDownload::Operation operation) const
{
    std::lock_guard<std::recursive_mutex> ongoing_direct_downloads_lock(ongoing_direct_downloads_mutex_);

    for (const auto &tasklet : ongoing_direct_downloads_) {
        const auto &params(tasklet->getParameter());

        if (params.operation_ == operation
           and params.download_item_.url_ == source.url_
           and &params.download_item_.journal_ == &source.journal_)
        {
            // this will essentially generate a duplicate of the harvestable (ID included)
            std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
                new_future(new Util::Future<DirectDownload::Params, DirectDownload::Result>(tasklet));
            return new_future;
        }
    }

    return nullptr;
}


DownloadManager::DownloadManager(const GlobalParams &global_params)
 : global_params_(global_params), stop_background_thread_(false)
{
    if (::pthread_create(&background_thread_, /* attr = */nullptr, BackgroundThreadRoutine, this) != 0)
        LOG_ERROR("background download manager thread creation failed!");
}


DownloadManager::~DownloadManager() {
    stop_background_thread_.store(true);
    const auto retcode(::pthread_join(background_thread_, nullptr));
    if (retcode != 0)
        LOG_WARNING("couldn't join with the download manager background thread! result = " + std::to_string(retcode));

    domain_data_.clear();
    cached_download_data_.clear();
    direct_download_queue_buffer_.clear();
    rss_queue_buffer_.clear();
    crawling_queue_buffer_.clear();
}


std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
    DownloadManager::directDownload(const Util::HarvestableItem &source, const std::string &user_agent,
                                    const DirectDownload::Operation operation, const unsigned timeout)
{
    // check if we have already delivered this URL
    if (not global_params_.force_downloads_
        and operation == DirectDownload::Operation::USE_TRANSLATION_SERVER
        and upload_tracker_.urlAlreadyInDatabase(source.url_.toString(), /*delivery_states_to_ignore=*/Util::UploadTracker::DELIVERY_STATES_TO_RETRY))
    {
        LOG_INFO("Skipping already delivered URL: " + source.url_.toString());
        std::unique_ptr<DirectDownload::Result> result(new DirectDownload::Result(source, operation));
        result->flags_ |= DirectDownload::Result::Flags::ITEM_ALREADY_DELIVERED;

        std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
                download_result(new Util::Future<DirectDownload::Params, DirectDownload::Result>(std::move(result)));
        return download_result;
    }

    // check if we have a cached response and return it immediately, if any
    auto cached_result(fetchFromDownloadCache(source, operation));
    if (cached_result != nullptr){
        std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
                download_result(new Util::Future<DirectDownload::Params, DirectDownload::Result>(std::move(cached_result)));
        return download_result;
    }

    // if we have an ongoing download of the same harvestable, return a new Future to wait on it
    auto ongoing_task_future(newFutureFromOngoingDownload(source, operation));
    if (ongoing_task_future != nullptr)
        return ongoing_task_future;

    std::unique_ptr<DirectDownload::Params> parameters(new DirectDownload::Params(source,
                                                       global_params_.translation_server_url_.toString(), user_agent,
                                                       global_params_.ignore_robots_txt_,
                                                       timeout == 0 ? global_params_.timeout_download_request_ : timeout, operation));
    std::shared_ptr<DirectDownload::Tasklet> new_tasklet(
        new DirectDownload::Tasklet(&tasklet_counters_.direct_download_tasklet_execution_counter_, this, std::move(parameters)));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(direct_download_queue_buffer_mutex_);
        direct_download_queue_buffer_.emplace_back(new_tasklet);
    }
    {
        std::lock_guard<std::recursive_mutex> ongoing_direct_downloads_lock(ongoing_direct_downloads_mutex_);
        ongoing_direct_downloads_.emplace_back(new_tasklet);
    }

    std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
        download_result(new Util::Future<DirectDownload::Params, DirectDownload::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<Util::Future<Crawling::Params, Crawling::Result>> DownloadManager::crawl(const Util::HarvestableItem &source,
                                                                                         const std::string &user_agent)
{
    std::unique_ptr<Crawling::Params> parameters(new Crawling::Params(source, user_agent, global_params_.timeout_download_request_,
                                                                      global_params_.timeout_crawl_operation_,
                                                                      global_params_.ignore_robots_txt_,
                                                                      global_params_.harvestable_manager_));
    std::shared_ptr<Crawling::Tasklet> new_tasklet(
        new Crawling::Tasklet(&tasklet_counters_.crawling_tasklet_execution_counter_, this, upload_tracker_, std::move(parameters), global_params_.force_downloads_));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(crawling_queue_buffer_mutex_);
        crawling_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<Util::Future<Crawling::Params, Crawling::Result>>
        download_result(new Util::Future<Crawling::Params, Crawling::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<Util::Future<RSS::Params, RSS::Result>> DownloadManager::rss(const Util::HarvestableItem &source,
                                                                             const std::string &user_agent,
                                                                             const std::string &feed_contents)
{
    std::unique_ptr<RSS::Params> parameters(new RSS::Params(source, user_agent, feed_contents, global_params_.harvestable_manager_));
    std::shared_ptr<RSS::Tasklet> new_tasklet(
        new RSS::Tasklet(&tasklet_counters_.rss_tasklet_execution_counter_, this, std::move(parameters),
        upload_tracker_, global_params_.force_downloads_));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(rss_queue_buffer_mutex_);
        rss_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<Util::Future<RSS::Params, RSS::Result>>
        download_result(new Util::Future<RSS::Params, RSS::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
    DownloadManager::apiQuery(const Util::HarvestableItem &source)
{
      std::unique_ptr<DirectDownload::Params> parameters(new DirectDownload::Params(source,
                                                       global_params_.translation_server_url_.toString(), "" /*user_agent*/,
                                                       true /*ignore robots.txt*/, 0 /*time limit*/, DirectDownload::Operation::USE_TRANSLATION_SERVER));
      std::shared_ptr<ApiQuery::Tasklet> new_tasklet(
        new ApiQuery::Tasklet(&tasklet_counters_.apiquery_tasklet_execution_counter_, this, upload_tracker_, std::move(parameters), global_params_.force_downloads_));

      {
         std::lock_guard<std::recursive_mutex> queue_buffer_lock(apiquery_queue_buffer_mutex_);
         apiquery_queue_buffer_.emplace_back(new_tasklet);
      }

       std::unique_ptr<Util::Future<DirectDownload::Params, DirectDownload::Result>>
        download_result(new Util::Future<DirectDownload::Params, DirectDownload::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<Util::Future<EmailCrawl::Params, EmailCrawl::Result>> DownloadManager::emailCrawl(const Util::HarvestableItem &source,
                                                                                         const std::string &user_agent)

{
    std::unique_ptr<EmailCrawl::Params> parameters(new EmailCrawl::Params(source, user_agent, global_params_.timeout_download_request_,
                                                                      global_params_.timeout_crawl_operation_,
                                                                      global_params_.ignore_robots_txt_,
                                                                      global_params_.harvestable_manager_));
    std::shared_ptr<EmailCrawl::Tasklet> new_tasklet(
        new EmailCrawl::Tasklet(&tasklet_counters_.emailcrawl_tasklet_execution_counter_, this, upload_tracker_, std::move(parameters), global_params_.force_downloads_));

    {
        std::lock_guard<std::recursive_mutex> queue_buffer_lock(emailcrawl_queue_buffer_mutex_);
        emailcrawl_queue_buffer_.emplace_back(new_tasklet);
    }

    std::unique_ptr<Util::Future<EmailCrawl::Params, EmailCrawl::Result>>
        download_result(new Util::Future<EmailCrawl::Params, EmailCrawl::Result>(new_tasklet));
    return download_result;
}


std::unique_ptr<DirectDownload::Result> DownloadManager::fetchFromDownloadCache(const Util::HarvestableItem &source,
                                                                                const DirectDownload::Operation operation) const
{
    std::lock_guard<std::recursive_mutex> download_cache_lock(cached_download_data_mutex_);

    const auto cache_hit(cached_download_data_.equal_range(source.url_.toString()));
    for (auto itr(cache_hit.first); itr != cache_hit.second; ++itr) {
        if (itr->second.operation_ == operation) {
            std::unique_ptr<DirectDownload::Result> cached_result(new DirectDownload::Result(source, operation));

            cached_result->response_body_ = itr->second.response_body_;
            cached_result->response_code_ = 200;
            cached_result->flags_ |= DirectDownload::Result::Flags::FROM_CACHE;

            return cached_result;
        }
    }

    return nullptr;
}


bool DownloadManager::downloadInProgress() const {
    return tasklet_counters_.direct_download_tasklet_execution_counter_ != 0
           or tasklet_counters_.crawling_tasklet_execution_counter_ != 0
           or tasklet_counters_.rss_tasklet_execution_counter_ != 0
           or tasklet_counters_.direct_downloads_translation_server_queue_counter_ != 0
           or tasklet_counters_.direct_downloads_direct_query_queue_counter_ != 0
           or tasklet_counters_.crawls_queue_counter_ != 0
           or tasklet_counters_.rss_feeds_queue_counter_ != 0
           or tasklet_counters_.apiquery_tasklet_execution_counter_ != 0
           or tasklet_counters_.apiquery_queue_counter_ != 0
           or tasklet_counters_.emailcrawl_tasklet_execution_counter_ != 0
           or tasklet_counters_.emailcrawl_queue_counter_ != 0;
}


} // end namespace Download


} // end namespace ZoteroHarvester
