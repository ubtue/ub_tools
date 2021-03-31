/** \file    import_rss_aggregator_conf.cc
 *  \brief   Imports an existing ini file into the new SQL table replacing it.
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

#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


// Returns true if we inserted or updated an entry in vufind.tuefind_rss_feeds else false.
bool ProcessSection(const std::string &subsystem_type, const unsigned default_downloader_time_limit,
                    DbConnection * const db_connection, const IniFile::Section &section)
{
    db_connection->queryOrDie("SELECT subsystem_types FROM vufind.tuefind_rss_feeds WHERE "
                              "feed_name='" + section.getSectionName() + "'");

    auto result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        const auto feed_url(section.getString("feed_url"));
        const auto blog_url(section.getString("blog_url"));
        const auto title_suppression_regex(section.getString("title_suppression_regex", ""));
        const auto strptime_format(section.getString("strptime_format", ""));
        const auto downloader_time_limit(section.getUnsigned("downloader_time_limit", default_downloader_time_limit));
        std::string QUERY("INSERT INTO vufind.tuefind_rss_feeds SET feed_name='" + section.getSectionName()
                          + "',subsystem_types='" + subsystem_type
                          + "',feed_url='" + db_connection->escapeString(feed_url)
                          + "',website_url='" + db_connection->escapeString(blog_url)
                          + "',downloader_time_limit=" + StringUtil::ToString(downloader_time_limit));
        if (not title_suppression_regex.empty())
            QUERY += ",title_suppression_regex='" + db_connection->escapeString(title_suppression_regex) + "'";
        if (not strptime_format.empty())
            QUERY += ",strptime_format='" + db_connection->escapeString(strptime_format) + "'";
        db_connection->queryOrDie(QUERY);
    } else {
        auto subsystem_types(result_set.getNextRow()["subsystem_types"]);
        if (subsystem_types.find(subsystem_type) != std::string::npos)
            return false; // Nothing to do!
        subsystem_types += "," + subsystem_type;
        db_connection->queryOrDie("UPDATE vufind.tuefind_rss_feeds SET subsystem_types='" + subsystem_types
                                  + "' WHERE feed_name='" + section.getSectionName() + "'");
    }

    return true;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("subsystem_type ini_file_path");

    const std::string subsystem_type(argv[1]);
    if (subsystem_type != "krimdok" and subsystem_type != "ixtheo" and subsystem_type != "relbib")
        LOG_ERROR("subsystem_type must be one of {krimdok,ixtheo,relbib}!");

    const IniFile ini_file(argv[2]);
    const auto db_connection(VuFind::GetDbConnection());

    unsigned default_downloader_time_limit(30), updated_or_inserted(0), feed_section_count(0);
    for (const auto &section : ini_file) {
        if (section.getSectionName() == "")
            continue; // Skip global section.

        if (section.getSectionName() == "Channel") {
            default_downloader_time_limit = section.getUnsigned("default_downloader_time_limit", 30);
            continue;
        }

        ++feed_section_count;
        if (ProcessSection(subsystem_type, default_downloader_time_limit, db_connection.get(), section))
            ++updated_or_inserted;
    }

    LOG_INFO("Processed " + std::to_string(feed_section_count) + " feed(s).");
    LOG_INFO("Updated or inserted " + std::to_string(updated_or_inserted)
             + " entry/entries in/into vufind.tuefind_rss_feeds.");

    return EXIT_SUCCESS;
}
