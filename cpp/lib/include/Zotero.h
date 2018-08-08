/** \brief Interaction with Zotero Translation Server
 *         - For a list of Zotero field types ("itemFields") in JSON, see
 *           https://github.com/zotero/zotero/blob/master/chrome/locale/de/zotero/zotero.properties#L409
 *  \author Mario Trojan
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <ctime>
#include <kchashdb.h>
#include "DbConnection.h"
#include "Downloader.h"
#include "IniFile.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "SimpleCrawler.h"
#include "TimeLimit.h"
#include "TimeUtil.h"
#include "UnsignedPair.h"
#include "Url.h"


namespace Zotero {


enum DeliveryMode { NONE, TEST, LIVE };
const std::map<std::string, int> STRING_TO_DELIVERY_MODE_MAP { { "NONE", static_cast<int>(Zotero::DeliveryMode::NONE) },
                                                              { "TEST", static_cast<int>(Zotero::DeliveryMode::TEST) },
                                                              { "LIVE", static_cast<int>(Zotero::DeliveryMode::LIVE) } };

enum HarvesterType { RSS, CRAWL, DIRECT };
const std::map<std::string, int> STRING_TO_HARVEST_TYPE_MAP { { "RSS", static_cast<int>(Zotero::HarvesterType::RSS) },
                                                              { "DIRECT", static_cast<int>(Zotero::HarvesterType::DIRECT) },
                                                              { "CRAWL", static_cast<int>(Zotero::HarvesterType::CRAWL) } };

enum HarvesterConfigEntry {
    TYPE, GROUP,
    PARENT_PPN, PARENT_ISSN_PRINT, PARENT_ISSN_ONLINE, STRPTIME_FORMAT,
    FEED, URL, BASE_URL,
    EXTRACTION_REGEX, MAX_CRAWL_DEPTH
};


extern const std::map<HarvesterType, std::string> HARVESTER_TYPE_TO_STRING_MAP;
extern const std::map<HarvesterConfigEntry, std::string> HARVESTER_CONFIG_ENTRY_TO_STRING_MAP;


extern const std::string DEFAULT_SIMPLE_CRAWLER_CONFIG_PATH;
extern const std::string ISSN_TO_MISC_BITS_MAP_PATH;


// native supported formats, see https://github.com/zotero/translation-server/blob/master/src/server_translation.js#L31-43
// also allowed: json, marc21 and marcxml
extern const std::vector<std::string> EXPORT_FORMATS;


const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type);


/**
 * Functions are named like endpoints, see
 * https://github.com/zotero/translation-server
 */
namespace TranslationServer {


/** \brief Use builtin translator to convert JSON to output format. */
bool Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &format, const std::string &json, std::string * const response_body,
            std::string * const error_message);

/** \brief Use builtin translator to convert input format to JSON. */
bool Import(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &input_content, std::string * const output_json, std::string * const error_message);

/** \brief Download URL and return as JSON. (If harvested_html is not empty, URL is not downloaded again.) */
bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
         const Url &harvest_url, std::string * const response_body, unsigned * response_code,
         std::string * const error_message, const std::string &harvested_html = "");


} // namespace TranslationServer


extern const std::string DEFAULT_SUBFIELD_CODE;


// Default timeout values in milliseconds
constexpr unsigned DEFAULT_CONVERSION_TIMEOUT = 60000;
constexpr unsigned DEFAULT_TIMEOUT = 10000;
constexpr unsigned DEFAULT_MIN_URL_PROCESSING_TIME = 200;


class PPNandTitle {
    std::string PPN_;
    std::string title_;
public:
    PPNandTitle(const std::string &PPN, const std::string &title): PPN_(PPN), title_(title) { }
    PPNandTitle() = default;
    PPNandTitle(const PPNandTitle &other) = default;

    inline const std::string &getPPN() const { return PPN_; }
    inline const std::string &getTitle() const { return title_; }
};


struct AugmentMaps {
    std::unordered_map<std::string, std::string> ISSN_to_SSG_map_;
    std::unordered_map<std::string, std::string> ISSN_to_keyword_field_map_;
    std::unordered_map<std::string, std::string> ISSN_to_language_code_map_;
    std::unordered_map<std::string, std::string> ISSN_to_licence_map_;
    std::unordered_map<std::string, std::string> ISSN_to_physical_form_map_;
    std::unordered_map<std::string, std::string> ISSN_to_volume_map_;
    std::unordered_map<std::string, std::string> language_to_language_code_map_;
    std::unordered_map<std::string, PPNandTitle> ISSN_to_superior_ppn_and_title_map_;
public:
    explicit AugmentMaps(const std::string &map_directory_path);
};


struct GroupParams {
    std::string name_;
    std::string user_agent_;
    std::string isil_;
    std::string author_lookup_url_;
};


void LoadGroup(const IniFile::Section &section, std::map<std::string, GroupParams> * const group_name_to_params_map);


/** \brief Parameters that apply to all sites equally. */
struct GobalAugmentParams {
    AugmentMaps * const maps_;
public:
    explicit GobalAugmentParams(AugmentMaps * const maps): maps_(maps) { }
};


/** \brief Parameters that apply to single sites only. */
struct SiteParams {
    // So that we don't have to pass through two arguments everywhere.
    GobalAugmentParams *global_params_;
    GroupParams *group_params_;

    std::string parent_journal_name_;
    std::string parent_ISSN_print_;
    std::string parent_ISSN_online_;
    std::string parent_PPN_;
    std::string strptime_format_;
    std::vector<MARC::EditInstruction> marc_edit_instructions_;
    std::unique_ptr<RegexMatcher> extraction_regex_;
public:
};


/** \brief  This function can be used to augment  Zotero JSON structure with information from AugmentParams.
 *  \param  object_node     The JSON ObjectNode with Zotero JSON structure of a single dataset
 *  \param  harvest_maps    The map files to apply.
 */
void AugmentJson(const std::shared_ptr<JSON::ObjectNode> &object_node, const SiteParams &site_params);


// forward declaration
class FormatHandler;


struct HarvestParams {
    Url zts_server_url_;
    TimeLimit min_url_processing_time_ = DEFAULT_MIN_URL_PROCESSING_TIME;
    unsigned harvested_url_count_ = 0;
    std::string user_agent_;
    std::unique_ptr<FormatHandler> format_handler_;
};


/** \class DownloadTracker
 *  \brief Loads, manages and stores the timestamps, hashes of previously downloaded metadata records.
 */
class DownloadTracker {
    DbConnection * db_connection_;
public:
    struct Entry {
        std::string url_;
        std::string journal_name_;
        time_t last_harvest_time_;
        std::string error_message_;
        std::string hash_;
    };
public:
    explicit DownloadTracker(DbConnection * const db): db_connection_(db) {}
    ~DownloadTracker() = default;

    /** \brief Checks if "url" or ("url", "hash") have already been downloaded.
     *  \return True if we have find an entry for "url" or ("url", "hash"), else false.
     */
    bool hasAlreadyBeenDownloaded(const std::string &url, const std::string &hash = "", Entry * const entry = nullptr) const;

    void addOrReplace(const std::string &url, const std::string &journal_name, const std::string &hash, const std::string &error_message);
    size_t listMatches(const std::string &url_regex, std::vector<Entry> * const entries) const;
    size_t deleteMatches(const std::string &url_regex);

    /** \return 0 if no matching entry was found, o/w 1. */
    size_t deleteSingleEntry(const std::string &url);

    /** \deletes all entries that have timestamps <= "cutoff_timestamp".
     *  \return  The number of deleted entries.
     */
    size_t deleteOldEntries(const time_t cutoff_timestamp);

    /** \brief Deletes all entries in the database.
     *  \return The number of deleted entries.
     */
    size_t clear();

    size_t size() const;
};


class FormatHandler {
protected:
    DownloadTracker download_tracker_;
    std::string output_format_;
    std::string output_file_;
    SiteParams *site_params_;
    const std::shared_ptr<const HarvestParams> &harvest_params_;
protected:
    FormatHandler(DbConnection * const db_connection, const std::string &output_format, const std::string &output_file,
                  const std::shared_ptr<const HarvestParams> &harvest_params)
        : download_tracker_(db_connection), output_format_(output_format), output_file_(output_file),
          site_params_(nullptr), harvest_params_(harvest_params)
        { }
public:
    virtual ~FormatHandler() = default;

    inline void setAugmentParams(SiteParams * const new_site_params) { site_params_ = new_site_params; }
    inline DownloadTracker &getDownloadTracker() { return download_tracker_; }

    /** \brief Convert & write single record to output file */
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) = 0;

    // The output format must be one of "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    // "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei", "json", "marc21", or "marcxml".
    static std::unique_ptr<FormatHandler> Factory(DbConnection * const db_connection, const std::string &output_format,
                                                  const std::string &output_file,
                                                  const std::shared_ptr<const HarvestParams> &harvest_params);
};


class JsonFormatHandler final : public FormatHandler {
    unsigned record_count_;
    File *output_file_object_;
public:
    JsonFormatHandler(DbConnection * const db_connection, const std::string &output_format, const std::string &output_file,
                      const std::shared_ptr<const HarvestParams> &harvest_params);
    virtual ~JsonFormatHandler();
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
};


class ZoteroFormatHandler final : public FormatHandler {
    unsigned record_count_;
    std::string json_buffer_;
public:
    ZoteroFormatHandler(DbConnection * const db_connection, const std::string &output_format, const std::string &output_file,
                        const std::shared_ptr<const HarvestParams> &harvest_params);
    virtual ~ZoteroFormatHandler();
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
};


class MarcFormatHandler final : public FormatHandler {
    std::unique_ptr<MARC::Writer> marc_writer_;
public:
    MarcFormatHandler(DbConnection * const db_connection, const std::string &output_file,
                      const std::shared_ptr<const HarvestParams> &harvest_params);
    virtual ~MarcFormatHandler() = default;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
    MARC::Writer *getWriter() { return marc_writer_.get(); }
private:
    inline std::string CreateSubfieldFromStringNode(const std::string &key, const std::shared_ptr<const JSON::JSONNode> node,
                                                    const std::string &tag, const char subfield_code,
                                                    MARC::Record * const marc_record, const char indicator1 = ' ',
                                                    const char indicator2 = ' ')
    {
        const std::shared_ptr<const JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie(key, node));
        const std::string value(string_node->getValue());
        marc_record->insertField(tag, { { subfield_code, value } }, indicator1, indicator2);
        return value;
    }

    inline std::string CreateSubfieldFromStringNode(const std::pair<std::string, std::shared_ptr<JSON::JSONNode>> &key_and_node,
                                                    const std::string &tag, const char subfield_code,
                                                    MARC::Record * const marc_record, const char indicator1 = ' ',
                                                    const char indicator2 = ' ')
    {
        return CreateSubfieldFromStringNode(key_and_node.first, key_and_node.second, tag, subfield_code, marc_record,
                                            indicator1, indicator2);
    }

    void ExtractKeywords(std::shared_ptr<const JSON::JSONNode> tags_node, const std::string &issn,
                         const std::unordered_map<std::string, std::string> &ISSN_to_keyword_field_map,
                         MARC::Record * const new_record);

    void ExtractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node,
                                        MARC::Record * const new_record);
    void CreateCreatorFields(const std::shared_ptr<const JSON::JSONNode> creators_node, MARC::Record * const marc_record);

    MARC::Record processJSON(const std::shared_ptr<const JSON::ObjectNode> &object_node, std::string * const url,
                             std::string * const publication_title, std::string * const abbreviated_publication_title,
                             std::string * const website_title);

    void populateCustomNode(std::shared_ptr<const JSON::JSONNode> custom_node, std::string * const issn_normalized,
                            std::string * const parent_journal_name, MARC::Record * const new_record);
};


const std::shared_ptr<RegexMatcher> LoadSupportedURLsRegex(const std::string &map_directory_path);


/** \brief  Harvest a single URL.
 *  \param  harvest_url         The URL to harvest.
 *  \param  extraction_regex    Regex matcher for URLs that can be harvested.
 *  \param  harvest_params      The parameters for downloading.
 *  \param  site_params      Parameter for augmenting the Zotero JSON result.
 *  \param  harvested_html      If not empty, the HTML will be used for harvesting
 *                              instead of downloading the URL again.
 *                              However, if the page contains a list of multiple
 *                              items (e.g. HTML page with a search result),
 *                              all results will be downloaded.
 *  \param  log                 If true, additional statistics will be logged.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> harvest_params,
                                      const SiteParams &site_params, const std::string &harvested_html = "", const bool log = true);


/** \brief Harvest metadate from a single site.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestSite(const SimpleCrawler::SiteDesc &site_desc, const SimpleCrawler::Params &crawler_params,
                         const std::shared_ptr<RegexMatcher> &supported_urls_regex, const std::shared_ptr<HarvestParams> &harvest_params,
                         const SiteParams &site_params, File * const progress_file = nullptr);


/** \brief Harvest metadate from a single Web page.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestURL(const std::string &url, const std::shared_ptr<HarvestParams> &harvest_params,
                        const SiteParams &site_params);


enum class RSSHarvestMode { VERBOSE, TEST, NORMAL };


/** \brief Harvest metadata from URL's referenced in an RSS or Atom feed.
 *  \param feed_url       Where to download the RSS feed.
 *  \param db_connection  A connection to a database w/ the structure as specified by .../cpp/data/ub_tools.sql. Not used when "mode"
 *                        is set to TEST.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestSyndicationURL(const RSSHarvestMode mode, const std::string &feed_url,
                                   const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                                   const SiteParams &site_params, DbConnection * const db_connection);


} // namespace Zotero
