/** \brief Interaction with Zotero Translation Server
 *         - For a list of Zotero field types ("itemFields") in JSON, see
 *           https://github.com/zotero/zotero/blob/master/chrome/locale/de/zotero/zotero.properties#L409
 *  \author Mario Trojan
 *
 *  \copyright 2018, 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "BSZTransform.h"
#include "BSZUpload.h"
#include "DbConnection.h"
#include "Downloader.h"
#include "IniFile.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "SimpleCrawler.h"
#include "TimeLimit.h"
#include "UnsignedPair.h"
#include "Url.h"


namespace BSZTransform { struct AugmentMaps; } // forward declaration


namespace Zotero {


enum HarvesterType { RSS, CRAWL, DIRECT };


extern const std::map<HarvesterType, std::string> HARVESTER_TYPE_TO_STRING_MAP;


const std::map<std::string, int> STRING_TO_HARVEST_TYPE_MAP { { "RSS", static_cast<int>(Zotero::HarvesterType::RSS) },
                                                              { "DIRECT", static_cast<int>(Zotero::HarvesterType::DIRECT) },
                                                              { "CRAWL", static_cast<int>(Zotero::HarvesterType::CRAWL) } };


struct Creator {
    std::string first_name_;
    std::string last_name_;
    std::string affix_;
    std::string title_;
    std::string type_;
    std::string ppn_;
    std::string gnd_number_;
};


struct CustomNodeParameters {
    std::string issn_zotero_;
    std::string issn_online_;
    std::string issn_print_;
    std::string superior_ppn_online_;
    std::string superior_ppn_print_;
    std::string journal_name_;
    std::string harvest_url_;
    std::string year_;
    std::string pages_;
    std::string volume_;
    std::string license_;
    std::string ssg_numbers_;
    std::vector<Creator> creators_;
    std::string comment_;
    std::string date_normalized_;
    std::string isil_;
    std::string issn_mapped_language_;
};


struct ItemParameters {
    std::string item_type_;
    std::string publication_title_;
    std::string abbreviated_publication_title_;
    std::string language_;
    std::string abstract_note_;
    std::string website_title_;
    std::string doi_;
    std::string copyright_;
    std::vector<Creator> creators_;
    std::string url_;
    std::string year_;
    std::string pages_;
    std::string volume_;
    std::string date_;
    std::string title_;
    std::string short_title_;
    std::string issue_;
    std::string isil_;
    // Additional item parameters
    std::string superior_ppn_online_;
    std::string superior_ppn_print_;
    std::string issn_zotero_;
    std::string issn_online_;
    std::string issn_print_;
    std::string license_;
    std::vector<std::string> keywords_;
    std::vector<std::string> ssg_numbers_;
    std::string journal_name_;
    std::string harvest_url_;
    std::map<std::string, std::string> notes_key_value_pairs_; // Abuse of the "notes" field to pass thru non-standard values
};


// native supported formats, see https://github.com/zotero/translation-server/blob/master/src/server_translation.js#L31-43
// also allowed: json, marc21 and marcxml
extern const std::vector<std::string> EXPORT_FORMATS;


const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type);


/**
 * Functions are named like endpoints, see
 * https://github.com/zotero/translation-server
 */
namespace TranslationServer {


/** \brief get url for zotero translation server based on local machine configuration */
const Url GetUrl();


/** \brief Use builtin translator to convert JSON to output format. */
bool Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &format, const std::string &json, std::string * const response_body,
            std::string * const error_message);

/** \brief Use builtin translator to convert input format to JSON. */
bool Import(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &input_content, std::string * const output_json, std::string * const error_message);

/** \brief Download single URL and return as JSON. (If harvested_html is not empty, URL is not downloaded again.) */
bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
         const Url &harvest_url, std::string * const response_body, unsigned * response_code,
         std::string * const error_message);

/** \brief This function is used if we get a "300 - multiple" response, to paste the response body back to the server.
 *         This way we get a JSON array with all downloaded results.
 */
bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
         const std::string &request_body, std::string * const response_body, unsigned * response_code,
         std::string * const error_message);


} // namespace TranslationServer


// Default timeout values in milliseconds
constexpr unsigned DEFAULT_CONVERSION_TIMEOUT = 60000;
constexpr unsigned DEFAULT_TIMEOUT = 30000;
constexpr unsigned DEFAULT_MIN_URL_PROCESSING_TIME = 200;


struct GroupParams {
    std::string name_;
    std::string user_agent_;
    std::string isil_;
    std::string bsz_upload_group_;
    std::string author_ppn_lookup_url_;
    std::string author_gnd_lookup_query_params_;
    std::vector<std::string> additional_fields_;
};


void LoadGroup(const IniFile::Section &section, std::unordered_map<std::string, GroupParams> * const group_name_to_params_map);


/** \brief Parameters that apply to all sites equally. */
struct GlobalAugmentParams {
    BSZTransform::AugmentMaps * const maps_;
public:
    explicit GlobalAugmentParams(BSZTransform::AugmentMaps * const maps): maps_(maps) { }
};


/** \brief Parameters that apply to single sites only. */
struct SiteParams {
    // So that we don't have to pass through two arguments everywhere.
    GlobalAugmentParams *global_params_;
    GroupParams *group_params_;

    std::string zeder_id_;
    std::string journal_name_;
    std::string ISSN_print_;
    std::string ISSN_online_;
    std::string PPN_print_;
    std::string PPN_online_;
    std::string strptime_format_;
    std::unique_ptr<RegexMatcher> extraction_regex_;
    std::unique_ptr<RegexMatcher> review_regex_;
    BSZUpload::DeliveryMode delivery_mode_;
    std::set<std::string> expected_languages_;
    std::string expected_languages_text_fields_;
    bool force_automatic_language_detection_;
    std::map<std::string, std::string> metadata_overrides_;
    std::map<std::string, std::unique_ptr<RegexMatcher>> metadata_suppression_filters_;
    std::vector<std::string> additional_fields_;
    std::vector<std::string> non_standard_metadata_fields_;
    std::map<std::string, std::unique_ptr<RegexMatcher>> field_exclusion_filters_;
    std::map<std::string, std::unique_ptr<RegexMatcher>> field_removal_filters_;
    unsigned journal_update_window_;
public:
    SiteParams()
        : global_params_(nullptr), group_params_(nullptr), delivery_mode_(BSZUpload::DeliveryMode::NONE),
          force_automatic_language_detection_(false), journal_update_window_(0) {}
};


/** \brief  This function can be used to augment  Zotero JSON structure with information from AugmentParams.
 *  \param  object_node     The JSON ObjectNode with Zotero JSON structure of a single dataset
 *  \param  harvest_maps    The map files to apply.
 */
void AugmentJson(const std::shared_ptr<JSON::ObjectNode> &object_node, const SiteParams &site_params);


// forward declaration
class FormatHandler;
class HarvesterErrorLogger;


struct HarvestParams {
    Url zts_server_url_;
    unsigned harvested_url_count_;
    std::string user_agent_;
    FormatHandler *format_handler_;
    bool force_downloads_;
    std::unique_ptr<RegexMatcher> harvest_url_regex_;
    unsigned journal_harvest_interval_;
    bool force_process_feeds_with_no_pub_dates_;
    unsigned default_crawl_delay_time_;
    bool skip_online_first_articles_unconditionally_;
public:
    HarvestParams()
        : harvested_url_count_(0), format_handler_(nullptr), force_downloads_(false), journal_harvest_interval_(0),
          force_process_feeds_with_no_pub_dates_(false), default_crawl_delay_time_(0),
          skip_online_first_articles_unconditionally_(false) {}
};


class FormatHandler {
protected:
    BSZUpload::DeliveryTracker delivery_tracker_;
    std::string output_format_;
    std::string output_file_;
    SiteParams *site_params_;
    const std::shared_ptr<const HarvestParams> harvest_params_;
protected:
    FormatHandler(DbConnection * const db_connection, const std::string &output_format, const std::string &output_file,
                  const std::shared_ptr<const HarvestParams> &harvest_params)
        : delivery_tracker_(db_connection), output_format_(output_format), output_file_(output_file),
          site_params_(nullptr), harvest_params_(harvest_params)
        { }
public:
    virtual ~FormatHandler() = default;

    inline void setAugmentParams(SiteParams * const new_site_params) { site_params_ = new_site_params; }
    inline BSZUpload::DeliveryTracker &getDeliveryTracker() { return delivery_tracker_; }

    /** \brief Convert & write single record to output file */
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) = 0;

    // The output format must be one of "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    // "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei", "json", "marc-21", or "marc-xml".
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
                      const std::shared_ptr<const HarvestParams> &harvest_params, const std::string &output_format = "");
    virtual ~MarcFormatHandler() = default;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
    MARC::Writer *getWriter() { return marc_writer_.get(); }
private:
    inline std::string createSubfieldFromStringNode(const std::string &key, const std::shared_ptr<const JSON::JSONNode> node,
                                                    const std::string &tag, const char subfield_code,
                                                    MARC::Record * const marc_record, const char indicator1 = ' ',
                                                    const char indicator2 = ' ')
    {
        const std::shared_ptr<const JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie(key, node));
        const std::string value(string_node->getValue());
        marc_record->insertField(tag, { { subfield_code, value } }, indicator1, indicator2);
        return value;
    }

    inline std::string createSubfieldFromStringNode(const std::pair<std::string, std::shared_ptr<JSON::JSONNode>> &key_and_node,
                                                    const std::string &tag, const char subfield_code,
                                                    MARC::Record * const marc_record, const char indicator1 = ' ',
                                                    const char indicator2 = ' ')
    {
        return createSubfieldFromStringNode(key_and_node.first, key_and_node.second, tag, subfield_code, marc_record,
                                            indicator1, indicator2);
    }

    void extractKeywords(std::shared_ptr<const JSON::JSONNode> tags_node, const std::string &issn,
                         const std::unordered_map<std::string, std::string> &ISSN_to_keyword_field_map,
                         MARC::Record * const new_record);

    void extractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node,
                                        MARC::Record * const new_record);

    MARC::Record processJSON(const std::shared_ptr<const JSON::ObjectNode> &object_node, std::string * const url,
                             std::string * const publication_title, std::string * const abbreviated_publication_title,
                             std::string * const website_title);

    void identifyMissingLanguage(ItemParameters * const node_parameters);

    // Extracts information from the ubtue node
    void extractCustomNodeParameters(std::shared_ptr<const JSON::JSONNode> custom_node,
                                     struct CustomNodeParameters * const custom_node_parameters);

    void extractItemParameters(std::shared_ptr<const JSON::ObjectNode> object_node,
                               struct ItemParameters * const item_parameters);

    void generateMarcRecord(MARC::Record * const record, const struct ItemParameters &item_parameters);

    void mergeCustomParametersToItemParameters(struct ItemParameters * const item_parameters,
                                               struct CustomNodeParameters &custom_node_params);

    void handleTrackingAndWriteRecord(const MARC::Record &new_record, const bool keep_delivered_records,
                                      struct ItemParameters &item_params, unsigned * const previously_downloaded_count);

    bool recordMatchesExclusionFilters(const MARC::Record &new_record, std::string * const exclusion_string) const;
};


const std::shared_ptr<RegexMatcher> LoadSupportedURLsRegex(const std::string &map_directory_path);


/** \brief  Harvest a single URL.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> &harvest_params,
                                      const SiteParams &site_params, HarvesterErrorLogger * const error_logger);


/** \brief Harvest metadate from a single site.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestSite(const SimpleCrawler::SiteDesc &site_desc, SimpleCrawler::Params crawler_params,
                         const std::shared_ptr<RegexMatcher> &supported_urls_regex, const std::shared_ptr<HarvestParams> &harvest_params,
                         const SiteParams &site_params, HarvesterErrorLogger * const error_logger, File * const progress_file = nullptr);


/** \brief Harvest metadate from a single Web page.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestURL(const std::string &url, const std::shared_ptr<HarvestParams> &harvest_params,
                        const SiteParams &site_params, HarvesterErrorLogger * const error_logger);


/** \brief Harvest metadata from URL's referenced in an RSS or Atom feed.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
UnsignedPair HarvestSyndicationURL(const std::string &feed_url, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                                   const SiteParams &site_params, HarvesterErrorLogger * const error_logger);


class HarvesterErrorLogger {
public:
    enum ErrorType {
        UNKNOWN,
        ZTS_CONVERSION_FAILED,
        DOWNLOAD_MULTIPLE_FAILED,
        FAILED_TO_PARSE_JSON,
        ZTS_EMPTY_RESPONSE,
        BAD_STRPTIME_FORMAT
    };

    friend class Context;

    class Context {
        friend class HarvesterErrorLogger;

        HarvesterErrorLogger &parent_;
        std::string journal_name_;
        std::string harvest_url_;
    private:
        Context(HarvesterErrorLogger * const parent, const std::string &journal_name, const std::string &harvest_url)
         : parent_(*parent), journal_name_(journal_name), harvest_url_(harvest_url) {}
    public:
        void log(HarvesterErrorLogger::ErrorType error, const std::string &message) {
            parent_.log(error, journal_name_, harvest_url_, message);
        }
        void autoLog(const std::string &message) {
            parent_.autoLog(journal_name_, harvest_url_, message);
        }
    };
private:
    static const std::unordered_map<ErrorType, std::string> ERROR_KIND_TO_STRING_MAP;

    struct HarvesterError {
        ErrorType type_;
        std::string message_;
    };

    struct JournalErrors {
        std::unordered_map<std::string, HarvesterError> url_errors_;
        std::vector<HarvesterError> non_url_errors_;
    };

    std::unordered_map<std::string, JournalErrors> journal_errors_;
public:
    HarvesterErrorLogger() = default;
public:
    Context newContext(const std::string &journal_name, const std::string &harvest_url) {
        return Context(this, journal_name, harvest_url);
    }
    void log(ErrorType error, const std::string &journal_name, const std::string &harvest_url, const std::string &message,
             const bool write_to_stderr = true);

    // Used when the error message crosses API boundaries and cannot be logged at the point of inception
    void autoLog(const std::string &journal_name, const std::string &harvest_url, const std::string &message,
                 const bool write_to_stderr = true);
    void writeReport(const std::string &report_file_path) const;
    inline bool hasErrors() const { return not journal_errors_.empty(); }
};


} // namespace Zotero


namespace std {
    template <>
    struct hash<Zotero::HarvesterErrorLogger::ErrorType> {
        size_t operator()(const Zotero::HarvesterErrorLogger::ErrorType &harvester_error_type) const {
            // hash method here.
            return hash<int>()(harvester_error_type);
        }
    };
} // namespace std
