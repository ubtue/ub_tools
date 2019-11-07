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

#include "MapUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "ZoteroHarvesterConfig.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Config {


const std::map<int, std::string> HARVESTER_OPERATION_TO_STRING_MAP{
    { HarvesterOperation::RSS,    "RSS"    },
    { HarvesterOperation::CRAWL,  "CRAWL"  },
    { HarvesterOperation::DIRECT, "DIRECT" }
};
const std::map<std::string, int> STRING_TO_HARVEST_OPERATION_MAP {
    { "RSS",    HarvesterOperation::RSS    },
    { "DIRECT", HarvesterOperation::DIRECT },
    { "CRAWL",  HarvesterOperation::CRAWL  }
};


const std::map<std::string, int> STRING_TO_UPLOAD_OPERATION_MAP {
    { "NONE", UploadOperation::NONE },
    { "TEST", UploadOperation::TEST },
    { "LIVE", UploadOperation::LIVE }
};
const std::map<int, std::string> UPLOAD_OPERATION_TO_STRING_MAP {
    { UploadOperation::NONE, "NONE" },
    { UploadOperation::TEST, "TEST" },
    { UploadOperation::LIVE, "LIVE" }
};


GlobalParams::GlobalParams(const IniFile::Section &config_section) {
    skip_online_first_articles_unconditonally_ = false;
    download_delay_params_.default_delay_ = 0;
    download_delay_params_.max_delay_ = 0;
    rss_harvester_operation_params_.harvest_interval_ = 0;
    rss_harvester_operation_params_.force_process_feeds_with_no_pub_dates_ = false;

    translation_server_url_ = config_section.getString(GetINIKeyString(TRANSLATION_SERVER_URL));
    enhancement_maps_directory_ = config_section.getString(GetINIKeyString(ENHANCEMENT_MAPS_DIRECTORY));
    group_names_ = config_section.getString(GetINIKeyString(GROUP_NAMES));
    strptime_format_string_ = config_section.getString(GetINIKeyString(STRPTIME_FORMAT_STRING));
    skip_online_first_articles_unconditonally_ = config_section.getBool(GetINIKeyString(SKIP_ONLINE_FIRST_ARTICLES_UNCONDITIONALLY));
    download_delay_params_.default_delay_ = config_section.getUnsigned(GetINIKeyString(DOWNLOAD_DELAY_DEFAULT));
    download_delay_params_.max_delay_ = config_section.getUnsigned(GetINIKeyString(DOWNLOAD_DELAY_MAX));
    rss_harvester_operation_params_.harvest_interval_ = config_section.getUnsigned(GetINIKeyString(RSS_HARVEST_INTERVAL));
    rss_harvester_operation_params_.force_process_feeds_with_no_pub_dates_ = config_section.getBool(GetINIKeyString(RSS_FORCE_PROCESS_FEEDS_WITH_NO_PUB_DATES));

    if (not strptime_format_string_.empty()) {
        if (strptime_format_string_[0] == '(')
            LOG_ERROR("Cannot specify locale in global strptime_format");
    }
}


const std::map<GlobalParams::INIKey, std::string> GlobalParams::KEY_TO_STRING_MAP {
    { TRANSLATION_SERVER_URL,                     "translation_server_url" },
    { ENHANCEMENT_MAPS_DIRECTORY,                 "enhancement_maps_directory" },
    { GROUP_NAMES,                                "groups" },
    { STRPTIME_FORMAT_STRING,                     "common_strptime_format" },
    { SKIP_ONLINE_FIRST_ARTICLES_UNCONDITIONALLY, "skip_online_first_articles_unconditionally" },
    { DOWNLOAD_DELAY_DEFAULT,                     "default_download_delay_time" },
    { DOWNLOAD_DELAY_MAX,                         "max_download_delay_time" },
    { RSS_HARVEST_INTERVAL,                       "journal_rss_harvest_interval" },
    { RSS_FORCE_PROCESS_FEEDS_WITH_NO_PUB_DATES,  "force_process_feeds_with_no_pub_dates" },
};


std::string GlobalParams::GetINIKeyString(const INIKey ini_key) {
    const auto match(KEY_TO_STRING_MAP.find(ini_key));
    if (match == KEY_TO_STRING_MAP.end())
        LOG_ERROR("invalid GlobalParams INI key '" + std::to_string(ini_key) + "'");
    return match->second;
}


GroupParams::GroupParams(const IniFile::Section &group_section) {
    name_ = group_section.getSectionName();
    user_agent_ = group_section.getString(GetINIKeyString(USER_AGENT));
    isil_ = group_section.getString(GetINIKeyString(ISIL));
    bsz_upload_group_ = group_section.getString(GetINIKeyString(BSZ_UPLOAD_GROUP));
    author_ppn_lookup_url_ = group_section.getString(GetINIKeyString(AUTHOR_PPN_LOOKUP_URL));
    author_gnd_lookup_query_params_ = group_section.getString(GetINIKeyString(AUTHOR_GND_LOOKUP_QUERY_PARAMS), "");
}


const std::map<GroupParams::INIKey, std::string> GroupParams::KEY_TO_STRING_MAP {
    { USER_AGENT,                     "user_agent" },
    { ISIL,                           "isil" },
    { BSZ_UPLOAD_GROUP,               "bsz_upload_group" },
    { AUTHOR_PPN_LOOKUP_URL,          "author_ppn_lookup_url" },
    { AUTHOR_GND_LOOKUP_QUERY_PARAMS, "author_gnd_lookup_query_params" },
};


std::string GroupParams::GetINIKeyString(const INIKey ini_key) {
    const auto match(KEY_TO_STRING_MAP.find(ini_key));
    if (match == KEY_TO_STRING_MAP.end())
        LOG_ERROR("invalid GroupParams INI key '" + std::to_string(ini_key) + "'");
    return match->second;
}


JournalParams::JournalParams(const IniFile::Section &journal_section, const GlobalParams &global_params) {
    zeder_id_ = journal_section.getUnsigned(GetINIKeyString(ZEDER_ID));
    name_ = journal_section.getSectionName();
    group_ = journal_section.getString(GetINIKeyString(GROUP));
    entry_point_url_ = journal_section.getString(GetINIKeyString(ENTRY_POINT_URL));
    harvester_operation_ = static_cast<HarvesterOperation>(journal_section.getEnum(GetINIKeyString(HARVESTER_OPERATION),
                                                           STRING_TO_HARVEST_OPERATION_MAP));
    ppn_.online_ = journal_section.getString(GetINIKeyString(ONLINE_PPN), "");
    ppn_.print_ = journal_section.getString(GetINIKeyString(PRINT_PPN), "");
    issn_.online_ = journal_section.getString(GetINIKeyString(ONLINE_ISSN), "");
    issn_.print_ = journal_section.getString(GetINIKeyString(PRINT_ISSN), "");
    strptime_format_string_ = journal_section.getString(GetINIKeyString(STRPTIME_FORMAT_STRING), "");
    if (not global_params.strptime_format_string_.empty()) {
        if (not strptime_format_string_.empty())
            strptime_format_string_ += '|';

        strptime_format_string_ += global_params.strptime_format_string_;
    }
    update_window_ = journal_section.getUnsigned(GetINIKeyString(UPDATE_WINDOW), 0);

    const auto review_regex(journal_section.getString(GetINIKeyString(REVIEW_REGEX), ""));
    if (not review_regex.empty())
        review_regex_.reset(new ThreadSafeRegexMatcher(review_regex));

    language_params_.force_automatic_language_detection_ = false;
    auto expected_languages(journal_section.getString(GetINIKeyString(EXPECTED_LANGUAGES), ""));
    if (not expected_languages.empty() and expected_languages[0] == '*') {
        language_params_.force_automatic_language_detection_ = true;
        expected_languages = expected_languages.substr(1);
    }
    const auto field_separator_pos(expected_languages.find(':'));
    if (field_separator_pos != std::string::npos) {
        language_params_.source_text_fields_ = expected_languages.substr(0, field_separator_pos);
        expected_languages = expected_languages.substr(field_separator_pos + 1);
    }
    StringUtil::Split(expected_languages, ',', &language_params_.expected_languages_, /* suppress_empty_components = */true);

    crawl_params_.max_crawl_depth_ = journal_section.getUnsigned(GetINIKeyString(CRAWL_MAX_DEPTH), 1);
    const auto extraction_regex(journal_section.getString(GetINIKeyString(CRAWL_EXTRACTION_REGEX), ""));
    if (not extraction_regex.empty())
        crawl_params_.extraction_regex_.reset(new ThreadSafeRegexMatcher(extraction_regex));

    const auto crawl_regex(journal_section.getString(GetINIKeyString(CRAWL_URL_REGEX), ""));
    if (not crawl_regex.empty())
        crawl_params_.crawl_url_regex_.reset(new ThreadSafeRegexMatcher(crawl_regex));

    // repeatable fields
    for (const auto &entry : journal_section) {
        if (StringUtil::StartsWith(entry.name_, "override_metadata_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("override_metadata_")));
            zotero_metadata_params_.fields_to_override_.insert(std::make_pair(field_name, entry.value_));
        } else if (StringUtil::StartsWith(entry.name_, "suppress_metadata_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("suppress_metadata_")));
            zotero_metadata_params_.fields_to_suppress_.insert(std::make_pair(field_name,
                                                               std::unique_ptr<ThreadSafeRegexMatcher>(new ThreadSafeRegexMatcher(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "add_field"))
            marc_metadata_params_.fields_to_add_.emplace_back(entry.value_);
        else if (StringUtil::StartsWith(entry.name_, "exclude_if_field_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("exclude_if_field_")));
            if (field_name.length() != MARC::Record::TAG_LENGTH and field_name.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("invalid exclusion field name '" + field_name + "'! expected format: <tag> or <tag><subfield_code>");

            marc_metadata_params_.exclusion_filters_.insert(std::make_pair(field_name,
                                                            std::unique_ptr<ThreadSafeRegexMatcher>(new ThreadSafeRegexMatcher(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "exclude_if_metadata_")) {
            const auto metadata_name(entry.name_.substr(__builtin_strlen("exclude_if_metadata_")));
            zotero_metadata_params_.exclusion_filters_.insert(std::make_pair(metadata_name,
                                                              std::unique_ptr<ThreadSafeRegexMatcher>(new ThreadSafeRegexMatcher(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "remove_field_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("remove_field_")));
            if (field_name.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("invalid removal filter name '" + field_name + "'! expected format: <tag><subfield_code>");

            marc_metadata_params_.fields_to_remove_.insert(std::make_pair(field_name,
                                                           std::unique_ptr<ThreadSafeRegexMatcher>(new ThreadSafeRegexMatcher(entry.value_))));
        }
    }
}


const std::map<JournalParams::INIKey, std::string> JournalParams::KEY_TO_STRING_MAP {
    { ZEDER_ID,                "zeder_id" },
    { ZEDER_MODIFIED_TIME,     "zeder_modified_time" },
    { GROUP,                   "zotero_group" },
    { ENTRY_POINT_URL,         "zotero_url" },
    { HARVESTER_OPERATION,     "zotero_type" },
    { UPLOAD_OPERATION,        "zotero_delivery_mode" },
    { ONLINE_PPN,              "online_ppn" },
    { PRINT_PPN,               "print_ppn" },
    { ONLINE_ISSN,             "online_issn" },
    { PRINT_ISSN,              "print_issn" },
    { STRPTIME_FORMAT_STRING,  "zotero_strptime_format" },
    { UPDATE_WINDOW,           "zotero_update_window" },
    { REVIEW_REGEX,            "zotero_review_regex" },
    { EXPECTED_LANGUAGES,      "zotero_expected_languages" },
    { CRAWL_MAX_DEPTH,         "zotero_max_crawl_depth" },
    { CRAWL_EXTRACTION_REGEX,  "zotero_extraction_regex" },
    { CRAWL_URL_REGEX,         "zotero_crawl_url_regex" },
};


std::string JournalParams::GetINIKeyString(const INIKey ini_key) {
    const auto match(KEY_TO_STRING_MAP.find(ini_key));
    if (match == KEY_TO_STRING_MAP.end())
        LOG_ERROR("invalid GroupParams INI key '" + std::to_string(ini_key) + "'");
    return match->second;
}


EnhancementMaps::EnhancementMaps(const std::string &enhancement_map_directory) {
    MapUtil::DeserialiseMap(enhancement_map_directory + "/language_to_language_code.map", &ISSN_to_language_);
    MapUtil::DeserialiseMap(enhancement_map_directory + "/ISSN_to_licence.map", &ISSN_to_license_);
    MapUtil::DeserialiseMap(enhancement_map_directory + "/ISSN_to_SSG.map", &ISSN_to_SSG_);
    MapUtil::DeserialiseMap(enhancement_map_directory + "/ISSN_to_volume.map", &ISSN_to_volume_);
}


std::string EnhancementMaps::lookup(const std::string &issn, const std::unordered_map<std::string, std::string> &map) const {
    const auto match(map.find(issn));
    if (match == map.end())
        return "";
    return match->second;
}


std::string EnhancementMaps::lookupLanguage(const std::string &issn) const {
    return lookup(issn, ISSN_to_language_);
}


std::string EnhancementMaps::lookupLicense(const std::string &issn) const {
    return lookup(issn, ISSN_to_license_);
}


std::string EnhancementMaps::lookupSSG(const std::string &issn) const {
    return lookup(issn, ISSN_to_SSG_);
}


std::string EnhancementMaps::lookupVolume(const std::string &issn) const {
    return lookup(issn, ISSN_to_volume_);
}


} // end namespace Config


} // end namespace ZoteroHarvester
