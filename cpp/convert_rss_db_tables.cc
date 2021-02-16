/** \file    convert_rss_db_tables.cc
 *  \brief   Transfers data from ub_tools.rss_aggregator to vufind.tuefind_rss_items.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2021 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <set>
#include <unordered_map>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


struct FeedInfo {
    std::string id_;
    std::set<std::string> subsystem_types_;
public:
    FeedInfo() = default;
    FeedInfo(const FeedInfo &rhs) = default;
    FeedInfo(const std::string &id, const std::set<std::string> &subsystem_types)
        : id_(id), subsystem_types_(subsystem_types) { }
    inline bool isCompatibleWith(const std::string &subsystem_type) const
        { return subsystem_types_.find(subsystem_type) != subsystem_types_.cend(); }
};


const FeedInfo &GetRSSFeedsID(DbConnection * vufind_connection, const std::string &url) {
    static std::unordered_map<std::string, FeedInfo> urls_to_feed_infos_map;
    const auto url_and_feed_info(urls_to_feed_infos_map.find(url));
    if (url_and_feed_info != urls_to_feed_infos_map.end())
        return url_and_feed_info->second;

    vufind_connection->queryOrDie("SELECT id,subsystem_types FROM tuefind_rss_feeds WHERE feed_url="
                                  + vufind_connection->escapeAndQuoteString(url));
    auto result_set(vufind_connection->getLastResultSet());
    if (result_set.empty())
        LOG_ERROR("found no tuefind_rss_feeds.id for \"" + url + "\"!");

    const auto row(result_set.getNextRow());
    std::vector<std::string> subsystem_types;
    StringUtil::Split(row["subsystem_types"], ',', &subsystem_types);
    const FeedInfo new_feed_info(row["id"], StlHelpers::VectorToSet(subsystem_types));
    urls_to_feed_infos_map[url] = new_feed_info;
    return urls_to_feed_infos_map.find(url)->second;
}


void CopyItem(DbConnection * const db_writer, const std::string &feed_id, const std::string &item_id,
              const std::string &item_url, const std::string &item_title, const std::string &item_description,
              const std::string &pub_date, const std::string &insertion_time)
{
    db_writer->queryOrDie("INSERT INTO tuefind_rss_items SET rss_feeds_id=" + feed_id + ",item_id='" + item_id
                          + "',item_url=" + db_writer->escapeAndQuoteString(item_url) + ",item_title="
                          + db_writer->escapeAndQuoteString(item_title) + ",item_description="
                          + db_writer->escapeAndQuoteString(item_description) + ",pub_date='"
                          + pub_date + "',insertion_time='" + insertion_time + "'");
}


} // unnamed namespace


int Main(int /*argc*/, char */*argv*/[]) {
    auto db_reader((DbConnection()));
    auto db_writer(VuFind::GetDbConnection());

    db_reader.queryOrDie("SELECT * FROM rss_aggregator");
    auto result_set(db_reader.getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        const auto &feed_info(GetRSSFeedsID(db_writer.get(), row["feed_url"]));
        if (unlikely(not feed_info.isCompatibleWith(row["flavour"])))
            LOG_ERROR("Item w/ item_id \"" + row["item_id"] + " has a flavour \"" + row["flavour"] +
                      "\" which is incompatible with the subsystem_types \""
                      + StlHelpers::ContainerToString(feed_info.subsystem_types_.cbegin(),
                                                      feed_info.subsystem_types_.cend(), ","));
        CopyItem(db_writer.get(), feed_info.id_, row["item_id"], row["item_url"], row["item_title"],
                 row["item_description"], row["pub_date"], row["insertion_time"]);
    }

    return EXIT_SUCCESS;
}
