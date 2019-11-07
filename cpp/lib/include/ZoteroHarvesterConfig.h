/** \brief Classes related to the Zotero Harvester's configuration data
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
#include "IniFile.h"
#include "RegexMatcher.h"


namespace ZoteroHarvester {


namespace Config {


struct GlobalParams {
    enum INIKey : unsigned {
        TRANSLATION_SERVER_URL,
        ENHANCEMENT_MAPS_DIRECTORY,
        GROUP_NAMES,
        STRPTIME_FORMAT_STRING,
        SKIP_ONLINE_FIRST_ARTICLES_UNCONDITIONALLY,
        DOWNLOAD_DELAY_DEFAULT,
        DOWNLOAD_DELAY_MAX,
        RSS_HARVEST_INTERVAL,
        RSS_FORCE_PROCESS_FEEDS_WITH_NO_PUB_DATES,
    };

    std::string translation_server_url_;
    std::string enhancement_maps_directory_;
    std::string group_names_;
    std::string strptime_format_string_;
    bool skip_online_first_articles_unconditonally_;
    struct {
        unsigned default_delay_;
        unsigned max_delay_;
    } download_delay_params_;
    struct {
        unsigned harvest_interval_;
        bool force_process_feeds_with_no_pub_dates_;
    } rss_harvester_operation_params_;
public:
    explicit GlobalParams(const IniFile::Section &config_section);

    static std::string GetINIKeyString(const INIKey ini_key);
private:
    static const std::map<INIKey, std::string> KEY_TO_STRING_MAP;
};


enum HarvesterOperation : unsigned { RSS, CRAWL, DIRECT };


extern const std::map<int, std::string> HARVESTER_OPERATION_TO_STRING_MAP;
extern const std::map<std::string, int> STRING_TO_HARVEST_OPERATION_MAP;


enum UploadOperation : unsigned { NONE, TEST, LIVE };


extern const std::map<std::string, int> STRING_TO_UPLOAD_OPERATION_MAP;
extern const std::map<int, std::string> UPLOAD_OPERATION_TO_STRING_MAP;


struct GroupParams {
    enum INIKey : unsigned {
        USER_AGENT,
        ISIL,
        BSZ_UPLOAD_GROUP,
        AUTHOR_PPN_LOOKUP_URL,
        AUTHOR_GND_LOOKUP_QUERY_PARAMS,
    };

    std::string name_;
    std::string user_agent_;
    std::string isil_;
    std::string bsz_upload_group_;
    std::string author_ppn_lookup_url_;
    std::string author_gnd_lookup_query_params_;
public:
    explicit GroupParams(const IniFile::Section &group_section);

    static std::string GetINIKeyString(const INIKey ini_key);
private:
    static const std::map<INIKey, std::string> KEY_TO_STRING_MAP;
};


struct JournalParams {
    enum INIKey : unsigned {
        ZEDER_ID,
        ZEDER_MODIFIED_TIME,
        GROUP,
        ENTRY_POINT_URL,
        HARVESTER_OPERATION,
        UPLOAD_OPERATION,
        ONLINE_PPN,
        PRINT_PPN,
        ONLINE_ISSN,
        PRINT_ISSN,
        STRPTIME_FORMAT_STRING,
        UPDATE_WINDOW,
        REVIEW_REGEX,
        EXPECTED_LANGUAGES,
        CRAWL_MAX_DEPTH,
        CRAWL_EXTRACTION_REGEX,
        CRAWL_URL_REGEX,
    };

    unsigned zeder_id_;
    std::string name_;
    std::string group_;
    std::string entry_point_url_;
    HarvesterOperation harvester_operation_;
    UploadOperation upload_operation_;
    struct {
        std::string online_;
        std::string print_;
    } ppn_;
    struct {
        std::string online_;
        std::string print_;
    } issn_;
    std::string strptime_format_string_;
    unsigned update_window_;
    std::unique_ptr<ThreadSafeRegexMatcher> review_regex_;
    struct {
        std::set<std::string> expected_languages_;
        std::string source_text_fields_;
        bool force_automatic_language_detection_;
    } language_params_;
    struct {
        unsigned max_crawl_depth_;
        std::unique_ptr<ThreadSafeRegexMatcher> extraction_regex_;
        std::unique_ptr<ThreadSafeRegexMatcher> crawl_url_regex_;
    } crawl_params_;

    struct {
        std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> fields_to_suppress_;
        std::map<std::string, std::string> fields_to_override_;
        std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> exclusion_filters_;
    } zotero_metadata_params_;
    struct {
        std::vector<std::string> fields_to_add_;
        std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> fields_to_remove_;
        std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> exclusion_filters_;
    } marc_metadata_params_;
public:
    explicit JournalParams(const IniFile::Section &journal_section, const GlobalParams &global_params);

    static std::string GetINIKeyString(const INIKey ini_key);
private:
    static const std::map<INIKey, std::string> KEY_TO_STRING_MAP;
};


class EnhancementMaps {
    std::unordered_map<std::string, std::string> ISSN_to_language_;
    std::unordered_map<std::string, std::string> ISSN_to_license_;
    std::unordered_map<std::string, std::string> ISSN_to_SSG_;
    std::unordered_map<std::string, std::string> ISSN_to_volume_;

    std::string lookup(const std::string &issn, const std::unordered_map<std::string, std::string> &map) const;
public:
    explicit EnhancementMaps(const std::string &enhancement_map_directory);

    std::string lookupLanguage(const std::string &issn) const;
    std::string lookupLicense(const std::string &issn) const;
    std::string lookupSSG(const std::string &issn) const;
    std::string lookupVolume(const std::string &issn) const;
};


} // end namespace Config


} // end namespace ZoteroHarvester
