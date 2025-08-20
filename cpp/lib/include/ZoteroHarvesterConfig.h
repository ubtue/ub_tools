/** \brief Classes related to the Zotero Harvester's configuration data
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
#pragma once


#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include "IniFile.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "Zeder.h"


#ifdef LICENSE
#undef LICENSE
#endif


namespace ZoteroHarvester {


// This namespace contains classes that represent the (immutable) configuration
// data of the Zotero Harvester program. Global, group and journal parameters
// read in from an INI file. Refer to the documentation in the default configuration
// INI file for details about individual configuration fields/keys.
namespace Config {


static constexpr unsigned DEFAULT_ZEDER_ID(0);


struct DownloadDelayParams {
    unsigned default_delay_in_ms_;
    unsigned max_delay_in_ms_;
    std::unordered_map<std::string, unsigned> domain_to_default_delay_map_;
    std::unordered_map<std::string, unsigned> domain_to_max_delay_map_;

public:
    DownloadDelayParams(): default_delay_in_ms_(0), max_delay_in_ms_(0) { }
    DownloadDelayParams(const IniFile::Section &config_section);

    unsigned getDefaultDelayForDomainOrDefault(const std::string &domain, bool * const default_returned) {
        return MiscUtil::GetContainerValueOrDefault(domain_to_default_delay_map_, domain, default_delay_in_ms_, default_returned);
    }
    unsigned getMaxDelayForDomainOrDefault(const std::string &domain, bool * const default_returned) {
        return MiscUtil::GetContainerValueOrDefault(domain_to_max_delay_map_, domain, max_delay_in_ms_, default_returned);
    }

    static bool IsValidIniEntry(const IniFile::Entry &entry);
};


// Metadata parameters related to Zotero that will be re-used in global as well as journal params later
struct ZoteroMetadataParams {
    std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> fields_to_suppress_;
    std::map<std::string, std::string> fields_to_override_;
    std::map<std::string, std::unique_ptr<ThreadSafeRegexMatcher>> exclusion_filters_;
    std::map<std::string, std::pair<std::unique_ptr<ThreadSafeRegexMatcher>, std::string>> rewrite_filters_;

public:
    ZoteroMetadataParams() = default;
    ZoteroMetadataParams(const IniFile::Section &config_section);

    static bool IsValidIniEntry(const IniFile::Entry &entry);
};

struct MarcMetadataFieldIfParams {
    std::string test_field_name_;
    std::shared_ptr<ThreadSafeRegexMatcher> if_matcher_;
    std::string replace_term_;
};


// Metadata parameters related to MARC that will be re-used in global as well as journal params later
struct MarcMetadataParams {
    std::vector<std::string> fields_to_add_;
    std::map<std::string, std::shared_ptr<ThreadSafeRegexMatcher>> fields_to_remove_;
    std::map<std::string, std::shared_ptr<ThreadSafeRegexMatcher>> subfields_to_remove_;
    std::map<std::string, std::shared_ptr<ThreadSafeRegexMatcher>> exclusion_filters_;
    std::map<std::string, std::pair<std::shared_ptr<ThreadSafeRegexMatcher>, std::string>> rewrite_filters_;
    std::map<std::string, MarcMetadataFieldIfParams> fields_to_add_if_;
    std::map<std::string, MarcMetadataFieldIfParams> fields_to_rewrite_if_;

public:
    MarcMetadataParams() = default;
    MarcMetadataParams(const IniFile::Section &config_section);

    static bool IsValidIniEntry(const IniFile::Entry &entry);
};


typedef bool (*ValidationCallback)(const IniFile::Entry &entry);
template <typename EnumType>
void CheckIniSection(const IniFile::Section &section, const std::map<EnumType, std::string> &allowed_values,
                     const std::vector<ValidationCallback> &callbacks = {}) {
    for (const auto &entry : section) {
        if (entry.name_.empty())
            continue;

        bool valid(false);
        for (const auto &allowed_value : allowed_values) {
            if (entry.name_ == allowed_value.second) {
                valid = true;
                break;
            }
        }

        if (not valid) {
            for (const auto &callback : callbacks) {
                if (callback(entry)) {
                    valid = true;
                    break;
                }
            }

            if (not valid) {
                std::string message("Invalid ini entry \"" + entry.name_ + "\"");
                if (not section.getSectionName().empty())
                    message += " in section \"" + section.getSectionName() + "\"";
                LOG_WARNING(message);
            }
        }
    }
}


// Parameters that pertain to all harvestable journals/groups.
struct GlobalParams {
    enum IniKey : unsigned {
        ENHANCEMENT_MAPS_DIRECTORY,
        GROUP_NAMES,
        SUBGROUP_NAMES,
        STRPTIME_FORMAT_STRING,
        SKIP_ONLINE_FIRST_ARTICLES_UNCONDITIONALLY,
        DOWNLOAD_DELAY_DEFAULT,
        DOWNLOAD_DELAY_MAX,
        REVIEW_REGEX,
        NOTES_REGEX,
        TIMEOUT_CRAWL_OPERATION,
        TIMEOUT_DOWNLOAD_REQUEST
    };

    std::string translation_server_url_;
    std::vector<std::string> emailcrawl_mboxes_;
    std::string enhancement_maps_directory_;
    std::string group_names_;
    std::string subgroup_names_;
    std::string strptime_format_string_;
    bool skip_online_first_articles_unconditionally_;
    DownloadDelayParams download_delay_params_;
    unsigned timeout_crawl_operation_;
    unsigned timeout_download_request_;
    std::shared_ptr<ThreadSafeRegexMatcher> review_regex_;
    std::shared_ptr<ThreadSafeRegexMatcher> notes_regex_;
    ZoteroMetadataParams zotero_metadata_params_;
    MarcMetadataParams marc_metadata_params_;

public:
    GlobalParams(const IniFile::Section &config_section);
    GlobalParams(const GlobalParams &rhs) = default;
    GlobalParams &operator=(const GlobalParams &rhs) = default;

    static std::string GetIniKeyString(const IniKey ini_key);

private:
    static const std::map<IniKey, std::string> KEY_TO_STRING_MAP;
};


enum HarvesterOperation : unsigned { RSS, CRAWL, DIRECT, APIQUERY, EMAIL };


extern const std::map<int, std::string> HARVESTER_OPERATION_TO_STRING_MAP;
extern const std::map<std::string, int> STRING_TO_HARVEST_OPERATION_MAP;


enum UploadOperation : unsigned { NONE, TEST, LIVE };


extern const std::map<std::string, int> STRING_TO_UPLOAD_OPERATION_MAP;
extern const std::map<int, std::string> UPLOAD_OPERATION_TO_STRING_MAP;


// Parameters that pertain to a specific group. Every journal has an associated group.
struct GroupParams {
    enum IniKey : unsigned {
        USER_AGENT,
        ISIL,
        OUTPUT_FOLDER,
        AUTHOR_SWB_LOOKUP_URL,
        AUTHOR_LOBID_LOOKUP_QUERY_PARAMS,
    };

    std::string name_;
    std::string user_agent_;
    std::string isil_;
    std::string output_folder_;
    std::string author_swb_lookup_url_;
    std::string author_lobid_lookup_query_params_;
    MarcMetadataParams marc_metadata_params_;

public:
    GroupParams(){};
    GroupParams(const IniFile::Section &group_section);
    GroupParams(const GroupParams &rhs) = default;
    GroupParams &operator=(const GroupParams &rhs) = default;

    static std::string GetIniKeyString(const IniKey ini_key);

protected:
    static const std::map<IniKey, std::string> KEY_TO_STRING_MAP;
};


struct SubgroupParams : public GroupParams {
    std::string reference_group_;

public:
    SubgroupParams() = default;
    SubgroupParams(const IniFile::Section &subgroup_section);
    SubgroupParams(const SubgroupParams &rhs) = default;
    SubgroupParams &operator=(const SubgroupParams &rhs) = default;
    std::string getReferenceGroup() { return reference_group_; }
};


struct LanguageParams {
    enum Mode : unsigned { DEFAULT, FORCE_LANGUAGES, FORCE_DETECTION, FORCE_FROM_TRANSLATOR };
    std::set<std::string> expected_languages_;
    std::string source_text_fields_ = "title";
    Mode mode_;

public:
    void reset() {
        expected_languages_.clear();
        source_text_fields_ = "title";
        mode_ = DEFAULT;
    }
};


// Parameters that pertain to a specific journal.
struct JournalParams {
    enum IniKey : unsigned {
        NAME, // not an actual INI key; placeholder for the journal name (name of the INI section)
        ZEDER_ID,
        ZEDER_MODIFIED_TIME,
        ZEDER_NEWLY_SYNCED_ENTRY,
        GROUP,
        SUBGROUP,
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
        NOTES_REGEX,
        EXPECTED_LANGUAGES,
        SSGN,
        LICENSE,
        SELECTIVE_EVALUATION,
        FORCE_LANGUAGE_DETECTION,
        CRAWL_MAX_DEPTH,
        CRAWL_EXTRACTION_REGEX,
        CRAWL_URL_REGEX,
        PERSONALIZED_AUTHORS,
        EMAILCRAWL_SUBJECT_REGEX,
        ADDITIONAL_SELECTORS,
        PAGES_NOT_ONLINE_FIRST,
        PAGED_RSS,
        PAGED_RSS_SIZE,
        PAGED_RSS_RANGE,
        PAGED_RSS_DELAY_TIME
    };

    unsigned zeder_id_;
    std::string name_;
    std::string group_;
    std::string subgroup_;
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
    std::string ssgn_;
    std::string license_;
    std::shared_ptr<ThreadSafeRegexMatcher> review_regex_;
    std::shared_ptr<ThreadSafeRegexMatcher> notes_regex_;
    LanguageParams language_params_;
    struct {
        unsigned max_crawl_depth_;
        std::unique_ptr<ThreadSafeRegexMatcher> extraction_regex_;
        std::unique_ptr<ThreadSafeRegexMatcher> crawl_url_regex_;
    } crawl_params_;
    std::string personalized_authors_;
    ZoteroMetadataParams zotero_metadata_params_;
    MarcMetadataParams marc_metadata_params_;
    bool zeder_newly_synced_entry_;
    bool selective_evaluation_;
    std::shared_ptr<ThreadSafeRegexMatcher> emailcrawl_subject_regex_;
    std::string additional_selectors_;
    bool pages_not_online_first_;
    bool paged_rss_;
    unsigned paged_rss_size_;
    std::vector<unsigned> paged_rss_range_;
    unsigned paged_rss_delay_time_;

public:
    JournalParams(const GlobalParams &global_params);
    JournalParams(const IniFile::Section &journal_section, const GlobalParams &global_params);
    JournalParams(const JournalParams &rhs) = delete;
    JournalParams &operator=(const JournalParams &rhs) = delete;

    static std::string GetIniKeyString(const IniKey ini_key);
    static IniKey GetIniKey(const std::string &ini_key_string);
    void SetEntryUrl(const std::string &entry_url);

private:
    static const std::map<IniKey, std::string> KEY_TO_STRING_MAP;
    static const std::map<std::string, IniKey> STRING_TO_KEY_MAP;
};


void LoadHarvesterConfigFile(const std::string &config_filepath, std::unique_ptr<GlobalParams> * const global_params,
                             std::vector<std::unique_ptr<GroupParams>> * const group_params,
                             std::vector<std::unique_ptr<SubgroupParams>> * const subgroup_params,
                             std::vector<std::unique_ptr<JournalParams>> * const journal_params,
                             std::unique_ptr<IniFile> * const config_file = nullptr,
                             const IniFile::Section config_overrides = IniFile::Section());


/** \brief Checks if the given language is an allowed language.
 *  \note  Possible allowed languages:
 *         - 3-letter english language code (see IsNormalizedLanguage)
 *         - 2-letter english language code (see TranslationUtil)
 *         - 3 or 4-letter german language code (see TranslationUtil)
 */
bool IsAllowedLanguage(const std::string &language);


/** \brief Checks if the given language is a normalized language.
 *  \note  Normalized Language = 3-letter english languages code (see TranslationUtil)
 */
bool IsNormalizedLanguage(const std::string &language);


/** \brief  Converts an allowed language to a normalized language.
 *  \throws std::runtime_error if a non-allowed language is given.
 */
std::string GetNormalizedLanguage(const std::string &language);


/** \brief Check if INI string for expected languages is valid & return struct if possible.
 *  \note  Can be used e.g. when parsing the INI file or trying to import values from Zeder to INI.
 *  \note  Empty string is allowed (returns defaults).
 */
bool ParseExpectedLanguages(const std::string &expected_languages_string, LanguageParams * const language_params);


} // end namespace Config


} // end namespace ZoteroHarvester
