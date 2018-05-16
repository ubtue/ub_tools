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
#include "Downloader.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "TimeLimit.h"
#include "TimeUtil.h"
#include "Url.h"


namespace Zotero {


// native supported formats, see https://github.com/zotero/translation-server/blob/master/src/server_translation.js#L31-43
// also allowed: json, marc21 and marcxml
extern const std::vector<std::string> ExportFormats;


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


struct HarvestMaps {
    std::unordered_map<std::string, std::string> ISSN_to_SSG_map_;
    std::unordered_map<std::string, std::string> ISSN_to_keyword_field_map_;
    std::unordered_map<std::string, std::string> ISSN_to_language_code_map_;
    std::unordered_map<std::string, std::string> ISSN_to_licence_map_;
    std::unordered_map<std::string, std::string> ISSN_to_physical_form_map_;
    std::unordered_map<std::string, std::string> ISSN_to_superior_ppn_map_;
    std::unordered_map<std::string, std::string> ISSN_to_volume_map_;
    std::unordered_map<std::string, std::string> language_to_language_code_map_;
    std::unordered_set<std::string> previously_downloaded_;
};


/** \brief  This function can be used to augment  Zotero JSON structure with information from HarvestMaps.
 *  \param  object_node     The JSON ObjectNode with Zotero JSON structure of a single dataset
 *  \param  harvest_maps    The map files to apply.
 */
void AugmentJson(const std::shared_ptr<JSON::ObjectNode> object_node, const std::shared_ptr<const HarvestMaps> harvest_maps);


// forward declaration
class FormatHandler;


struct HarvestParams {
    Url zts_server_url_;
    TimeLimit min_url_processing_time_ = DEFAULT_MIN_URL_PROCESSING_TIME;
    unsigned harvested_url_count_ = 0;
    std::string optional_strptime_format_;
    std::unique_ptr<FormatHandler> format_handler_;
};


class FormatHandler {
protected:
    std::string output_format_;
    std::string output_file_;
    std::shared_ptr<HarvestMaps> harvest_maps_;
    std::shared_ptr<HarvestParams> harvest_params_;

    FormatHandler(const std::string &output_format, const std::string &output_file, std::shared_ptr<HarvestMaps> harvest_maps,
                  std::shared_ptr<HarvestParams> harvest_params)
        : output_format_(output_format), output_file_(output_file), harvest_maps_(harvest_maps), harvest_params_(harvest_params)
        { }
public:
    virtual ~FormatHandler() = default;

    /** \brief Convert & write single record to output file */
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) = 0;

    // The output format must be one of "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    // "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei", "json", "marc21", or "marcxml".
    static std::unique_ptr<FormatHandler> Factory(const std::string &output_format, const std::string &output_file,
                                                  std::shared_ptr<HarvestMaps> harvest_maps,
                                                  std::shared_ptr<HarvestParams> harvest_params);
};


class JsonFormatHandler final : public FormatHandler {
    unsigned record_count_;
    File *output_file_object_;
public:
    JsonFormatHandler(const std::string &output_format, const std::string &output_file, std::shared_ptr<HarvestMaps> harvest_maps,
                      std::shared_ptr<HarvestParams> harvest_params);
    virtual ~JsonFormatHandler();
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
};


class ZoteroFormatHandler final : public FormatHandler {
    unsigned record_count_;
    std::string json_buffer_;
public:
    ZoteroFormatHandler(const std::string &output_format, const std::string &output_file,
                        std::shared_ptr<HarvestMaps> harvest_maps, std::shared_ptr<HarvestParams> harvest_params);
    virtual ~ZoteroFormatHandler();
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
};


class MarcFormatHandler final : public FormatHandler {
    std::unique_ptr<MARC::Writer> marc_writer_;
public:
    MarcFormatHandler(const std::string &output_file, std::shared_ptr<HarvestMaps> harvest_maps,
                      std::shared_ptr<HarvestParams> harvest_params);
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

    void ExtractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node, const std::string &optional_strptime_format,
                                        MARC::Record * const new_record);
    void CreateCreatorFields(const std::shared_ptr<const JSON::JSONNode> creators_node, MARC::Record * const marc_record);
};


const std::shared_ptr<RegexMatcher> LoadSupportedURLsRegex(const std::string &map_directory_path);

std::shared_ptr<HarvestMaps> LoadMapFilesFromDirectory(const std::string &map_directory_path);


/** \brief  Harvest a single URL.
 *  \param  harvest_url     The URL to harvest.
 *  \param  harvest_params  The parameters for downloading.
 *  \param  harvest_maps    The map files to use after harvesting.
 *  \param  harvested_html  If not empty, the HTML will be used for harvesting
 *                          instead of downloading the URL again.
 *                          However, if the page contains a list of multiple
 *                          items (e.g. HTML page with a search result),
 *                          all results will be downloaded.
 *  \param  log             If true, additional statistics will be logged.
 *  \return count of all records / previously downloaded records => The number of newly downloaded records is the
 *          difference (first - second).
 */
std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> harvest_params,
                                      const std::shared_ptr<const HarvestMaps> harvest_maps,
                                      const std::string &harvested_html = "", const bool log = true);


// \brief Loads and stores the hashes of previously downloaded metadata records.
class PreviouslyDownloadedHashesManager {
    std::string hashes_path_;
    std::unordered_set<std::string> &previously_downloaded_;
public:
    PreviouslyDownloadedHashesManager(const std::string &hashes_path,
                                      std::unordered_set<std::string> * const previously_downloaded);
    ~PreviouslyDownloadedHashesManager();
};


/** \class DownloadTracker
 *  \brief Keeps track of already downloaded/processed URL's.
 */
class DownloadTracker {
public:
    class Entry {
        std::string url_;
        time_t recording_time_;
        std::string optional_message_;
    public:
        Entry(const std::string &url, const time_t recording_time, const std::string &optional_message)
            : url_(url), recording_time_(recording_time), optional_message_(optional_message) { }
        Entry() = default;
        Entry(const Entry &rhs) = default;

        inline const std::string &getURL() const { return url_; }
        inline time_t getRecodingTime() const { return recording_time_; }
        inline const std::string &getOptionalMessage() const { return optional_message_; }
        inline bool operator!=(const Entry &rhs) const { return url_ != rhs.url_; }
    };

    class const_iterator {
        friend class DownloadTracker;
        Entry current_entry_;
        void *cursor_;
    private:
        const_iterator(void *cursor);
    public:
        ~const_iterator();
        inline const Entry *operator->() const { return &current_entry_; }
        inline const Entry &operator*() const { return current_entry_; }
        void operator++();
        inline bool operator!=(const const_iterator &rhs) const { return current_entry_ != rhs.current_entry_; }
    private:
        void readEntry();
    };
private:
    void *db_;
public:
    DownloadTracker();
    ~DownloadTracker();

    /** \brief Used to determine if we already downloaded a URL in the past or not.
     *  \param url            The URL to check.
     *  \param download_time  If we already downloaded the URL, this will be set to the time of when we recorded it.
     */
    bool alreadyDownloaded(const std::string &url, time_t * const download_time) const;

    /** \brief Records that we downloaded a URL.
     *  \param url               The relevant URL.
     *  \param optional_message  Auxillary information like, for example, a downlod error message.
     *  \note Uses the current time as the recording time.
     */
    void recordDownload(const std::string &url, const std::string &optional_message = "");

    /** \brief Delete the entry for a given URL.
     *  \param url  The URL whose entry we want to erase.
     *  \return True if we succeded in removing the entry and false if the entry didn't exist.
     */
    bool clearEntry(const std::string &url);

    /** \return True if an entry w/ key "url" was found, o/w false. */
    bool lookup(const std::string &url, time_t * const timestamp, std::string * const optional_message) const;

    /** \brief Deletes all entries older than "cutoff".
     *  \return The number of deleted entries.
     */
    size_t clear(const time_t cutoff = TimeUtil::MAX_TIME_T);

    const_iterator begin() const;
    const_iterator end() const;
};


} // namespace Zotero
