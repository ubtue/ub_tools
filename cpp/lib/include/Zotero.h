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
#ifndef ZOTERO_H
#define ZOTERO_H


#include <memory>
#include "Downloader.h"
#include "FileUtil.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "TimeLimit.h"
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


// forward declaration
class FormatHandler;


struct HarvestParams {
    Url zts_server_url_;
    TimeLimit min_url_processing_time_ = DEFAULT_MIN_URL_PROCESSING_TIME;
    unsigned harvested_url_count_ = 0;
    std::unique_ptr<FormatHandler> format_handler_;
};


class FormatHandler {
protected:
    std::string output_format_;
    std::string output_file_;
    std::shared_ptr<HarvestMaps> harvest_maps_;
    std::shared_ptr<HarvestParams> harvest_params_;
public:
    FormatHandler(const std::string &output_format, const std::string &output_file, std::shared_ptr<HarvestMaps> harvest_maps,
                  std::shared_ptr<HarvestParams> harvest_params)
        : output_format_(output_format), output_file_(output_file), harvest_maps_(harvest_maps), harvest_params_(harvest_params) {}
    virtual ~FormatHandler() {}

    virtual void prepareProcessing() = 0;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) = 0;
    virtual void finishProcessing() = 0;

    static std::unique_ptr<FormatHandler> Factory(const std::string &output_format, const std::string &output_file,
                                                  std::shared_ptr<HarvestMaps> harvest_maps,
                                                  std::shared_ptr<HarvestParams> harvest_params);
};


class JsonFormatHandler final : public FormatHandler {
    unsigned record_count_ = 0;
    File *output_file_object_ = nullptr;
public:
    using FormatHandler::FormatHandler;
    virtual void prepareProcessing() override;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
    virtual void finishProcessing() override;
};


class ZoteroFormatHandler final : public FormatHandler {
    unsigned record_count_ = 0;
    std::string json_buffer_;
public:
    using FormatHandler::FormatHandler;
    virtual void prepareProcessing() override;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
    virtual void finishProcessing() override;
};


class MarcFormatHandler final : public FormatHandler {
    std::unique_ptr<MARC::Writer> marc_writer_;
public:
    using FormatHandler::FormatHandler;
    virtual void prepareProcessing() override;
    virtual std::pair<unsigned, unsigned> processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) override;
    virtual void finishProcessing() override;
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

    void ExtractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node, MARC::Record * const new_record);
    void CreateCreatorFields(const std::shared_ptr<const JSON::JSONNode> creators_node, MARC::Record * const marc_record);
};


const std::shared_ptr<RegexMatcher> LoadSupportedURLsRegex(const std::string &map_directory_path);

std::shared_ptr<HarvestMaps> LoadMapFilesFromDirectory(const std::string &map_directory_path);

/** \brief  Harvest a single URL.
 *  \param  harvest_url     The URL to harvest.
 *  \param  harvest_params  The parameters for downloading.
 *  \param  harvest_maps    The map files to use after harvesting.
 *  \param  harvested_html  If not empty, the html will be used for harvesting
 *                          instead of downloading the URL again.
 *                          However, if the page contains a list of multiple
 *                          items (e.g. HTML page with a search result),
 *                          all results will be downloaded.
 *  \param  log             If true, additional statistics will be logged.
 *  \return count of all records / previously downloaded records
 */
std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url,
                                      const std::shared_ptr<HarvestParams> harvest_params,
                                      const std::shared_ptr<const HarvestMaps> harvest_maps,
                                      const std::string &harvested_html = "",
                                      const bool log = true);


// \brief Loads and stores the hashes of previously downloaded metadata records.
class PreviouslyDownloadedHashesManager {
    std::string hashes_path_;
    std::unordered_set<std::string> &previously_downloaded_;
public:
    PreviouslyDownloadedHashesManager(const std::string &hashes_path,
                                      std::unordered_set<std::string> * const previously_downloaded);
    ~PreviouslyDownloadedHashesManager();
};


} // namespace Zotero


#endif // ifndef ZOTERO_H
