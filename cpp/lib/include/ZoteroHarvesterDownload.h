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
#include "SimpleCrawler.h"
#include "ThreadUtil.h"
#include "TimeLimit.h"
#include "Url.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace ZoteroHarvester {


namespace Download {


namespace DirectDownload {


struct Params {
    Util::Harvestable download_item_;
    Url translation_server_url_;
    std::string user_agent_;
    TimeLimit time_limit_;
public:
    explicit Params(const Util::Harvestable &download_item, const std::string &translation_server_url,
                    const std::string user_agent, const TimeLimit time_limit)
     : download_item_(download_item), translation_server_url_(translation_server_url), user_agent_(user_agent),
       time_limit_(time_limit) {}
};


struct Result {
    std::string response_body_;
    unsigned response_code_;
    std::string error_message_;
public:
    Result() = default;
    explicit Result(const std::string &response_body, unsigned response_code, const std::string error_message = "")
     : response_body_(response_body), response_code_(response_code), error_message_(error_message) {}

    inline bool isValid() const { return response_code_ == 200 and not error_message_.empty(); }
};


class Tasklet : public Util::Tasklet<Params, Result> {
    static void Run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned long> * const instance_counter, std::unique_ptr<Params> parameters);
    virtual ~Tasklet() override = default;
};


} // end namespace DirectDownload


class DownloadManager;
class DownloadResult;


namespace Crawling {


struct Params {
    Util::Harvestable download_item_;
    std::string user_agent_;
    TimeLimit time_limit_;
    bool ignore_robots_dot_txt_;
    DownloadManager * const download_manager_;
public:
    explicit Params(const Util::Harvestable &download_item, const std::string user_agent, const TimeLimit time_limit,
                    const bool ignore_robots_dot_txt, DownloadManager * const download_manager)
     : download_item_(download_item), user_agent_(user_agent), time_limit_(time_limit), ignore_robots_dot_txt_(ignore_robots_dot_txt),
       download_manager_(download_manager) {}
};


struct Result {
    std::vector<std::unique_ptr<DownloadResult>> downloaded_urls_;
};


class Tasklet : public Util::Tasklet<Params, Result> {
    static void Run(const Params &parameters, Result * const result);
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned long> * const instance_counter, std::unique_ptr<Params> parameters);
    virtual ~Tasklet() override = default;
};


} // end namespace Crawling


/*
    2 threads - One to wait for the download delay and another to
    a cache for recently downloaded urls
*/
class DownloadManager {
public:
    struct Globals {
        Url translation_server_url_;
        unsigned default_download_delay_time_;
        unsigned max_download_delay_time_;
        bool ignore_robots_txt_;
    public:
        explicit Globals() : default_download_delay_time_(0), max_download_delay_time_(0), ignore_robots_txt_(false) {}
    };





    struct CrawlParams {
        Util::Harvestable download_item_;
    public:
        explicit CrawlParams(const Util::Harvestable &download_item) : download_item_(download_item) {}
    };
private:

};




} // end namespace Download


} // end namespace ZoteroHarvester
