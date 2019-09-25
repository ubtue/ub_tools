/** \brief Interaction with Zotero Translation Server
 *         public functions are named like endpoints
 *         see https://github.com/zotero/translation-server
 *  \author Dr. Johannes Ruscheinski
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
#include "Zotero.h"
#include <chrono>
#include <ctime>
#include <uuid/uuid.h>
#include "DbConnection.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "LobidUtil.h"
#include "MiscUtil.h"
#include "NGram.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "TimeUtil.h"
#include "util.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroTransformation.h"


// Forward declaration:
class File;


namespace Zotero {


const std::map<HarvesterType, std::string> HARVESTER_TYPE_TO_STRING_MAP{
    { HarvesterType::RSS, "RSS" },
    { HarvesterType::CRAWL, "CRAWL" },
    { HarvesterType::DIRECT, "DIRECT" }
};


const std::vector<std::string> EXPORT_FORMATS{
    "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei",
    "json", "marc21", "marcxml"
};


namespace TranslationServer {


const Url GetUrl() {
    const IniFile ini(UBTools::GetTuelibPath() + "zotero.conf");
    return Url(ini.getString("Server", "url"));
}


bool ResponseCodeIndicatesSuccess(unsigned response_code, const std::string &response_body, std::string * const error_message) {
    const std::string response_code_string(StringUtil::ToString(response_code));
    const char response_code_category(response_code_string[0]);
    if (response_code_category == '4' or response_code_category == '5' or response_code_category == '9') {
        *error_message = "HTTP response " + response_code_string;
        if (not response_body.empty())
            *error_message += " (" + response_body + ")";
        return false;
    }
    return true;
}


bool Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &format, const std::string &json, std::string * const response_body, std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/export?format=" + format));
    downloader_params.additional_headers_ = { "Content-Type: application/json" };
    downloader_params.post_data_ = json;

    Downloader downloader(endpoint_url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_body = downloader.getMessageBody();
        return ResponseCodeIndicatesSuccess(downloader.getResponseCode(), *response_body, error_message);
    }
}


bool Import(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
            const std::string &input_content, std::string * const output_json, std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/import"));
    downloader_params.post_data_ = input_content;

    Downloader downloader(endpoint_url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *output_json = downloader.getMessageBody();
        return ResponseCodeIndicatesSuccess(downloader.getResponseCode(), *output_json, error_message);
    }
}


bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
         const Url &harvest_url, std::string * const response_body, unsigned * response_code,
         std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/web"));
    downloader_params.additional_headers_ = { "Accept: application/json", "Content-Type: text/plain" };
    downloader_params.post_data_ = harvest_url;

    Downloader downloader(endpoint_url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_code = downloader.getResponseCode();
        *response_body = downloader.getMessageBody();
        return ResponseCodeIndicatesSuccess(*response_code, *response_body, error_message);
    }
}


bool Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
         const std::string &request_body, std::string * const response_body, unsigned * response_code,
         std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/web"));
    downloader_params.additional_headers_ = { "Accept: application/json", "Content-Type: application/json" };
    downloader_params.post_data_ = request_body;

    Downloader downloader(endpoint_url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_code = downloader.getResponseCode();
        *response_body = downloader.getMessageBody();
        return ResponseCodeIndicatesSuccess(*response_code, *response_body, error_message);
    }
}


} // namespace TranslationServer


void LoadGroup(const IniFile::Section &section, std::unordered_map<std::string, GroupParams> * const group_name_to_params_map) {
    GroupParams new_group_params;
    new_group_params.name_                           = section.getSectionName();
    new_group_params.user_agent_                     = section.getString("user_agent");
    new_group_params.isil_                           = section.getString("isil");
    new_group_params.bsz_upload_group_               = section.getString("bsz_upload_group");
    new_group_params.author_ppn_lookup_url_          = section.getString("author_ppn_lookup_url");
    new_group_params.author_gnd_lookup_query_params_ = section.getString("author_gnd_lookup_query_params", "");
    for (const auto &entry : section) {
        if (StringUtil::StartsWith(entry.name_, "add_field"))
            new_group_params.additional_fields_.emplace_back(entry.value_);
    }

    group_name_to_params_map->emplace(section.getSectionName(), new_group_params);
}


std::unique_ptr<FormatHandler> FormatHandler::Factory(DbConnection * const db_connection, const std::string &output_format,
                                                      const std::string &output_file,
                                                      const std::shared_ptr<const HarvestParams> &harvest_params)
{
    if (output_format == "marc-xml" or output_format == "marc-21")
        return std::unique_ptr<FormatHandler>(new MarcFormatHandler(db_connection, output_file, harvest_params, output_format));
    else if (output_format == "json")
        return std::unique_ptr<FormatHandler>(new JsonFormatHandler(db_connection, output_format, output_file, harvest_params));
    else if (std::find(EXPORT_FORMATS.begin(), EXPORT_FORMATS.end(), output_format) != EXPORT_FORMATS.end())
        return std::unique_ptr<FormatHandler>(new ZoteroFormatHandler(db_connection, output_format, output_file, harvest_params));
    else
        LOG_ERROR("invalid output-format: " + output_format);
}


JsonFormatHandler::JsonFormatHandler(DbConnection * const db_connection, const std::string &output_format,
                                     const std::string &output_file, const std::shared_ptr<const HarvestParams> &harvest_params)
    : FormatHandler(db_connection, output_format, output_file, harvest_params), record_count_(0),
      output_file_object_(new File(output_file_, "w"))
{
    output_file_object_->write("[");
}


JsonFormatHandler::~JsonFormatHandler() {
    output_file_object_->write("]");
    delete output_file_object_;
}


std::pair<unsigned, unsigned> JsonFormatHandler::processRecord(const std::shared_ptr<JSON::ObjectNode> &object_node) {
    if (record_count_ > 0)
        output_file_object_->write(",");
    output_file_object_->write(object_node->toString());
    ++record_count_;
    return std::make_pair(1, 0);
}


ZoteroFormatHandler::ZoteroFormatHandler(DbConnection * const db_connection, const std::string &output_format,
                                         const std::string &output_file, const std::shared_ptr<const HarvestParams> &harvest_params)
    : FormatHandler(db_connection, output_format, output_file, harvest_params), record_count_(0),
      json_buffer_("[")
{
}


ZoteroFormatHandler::~ZoteroFormatHandler() {
    json_buffer_ += "]";

    Downloader::Params downloader_params;
    std::string response_body;
    std::string error_message;
    if (not TranslationServer::Export(harvest_params_->zts_server_url_, DEFAULT_CONVERSION_TIMEOUT, downloader_params,
                                      output_format_, json_buffer_, &response_body, &error_message))
        LOG_ERROR("converting to target format failed: " + error_message);
    else
        FileUtil::WriteString(output_file_, response_body);
}


std::pair<unsigned, unsigned> ZoteroFormatHandler::processRecord(const std::shared_ptr<JSON::ObjectNode> &object_node) {
    if (record_count_ > 0)
        json_buffer_ += ',';
    json_buffer_ += object_node->toString();
    ++record_count_;
    return std::make_pair(1, 0);
}


std::string GuessOutputFormat(const std::string &output_file) {
    switch (MARC::GuessFileType(output_file)) {
    case MARC::FileType::BINARY:
        return "marc-21";
    case MARC::FileType::XML:
        return "marc-xml";
    default:
        LOG_ERROR("we should *never* get here!");
    }
}


MARC::FileType GetOutputMarcFileType(const std::string &output_format) {
    if (output_format == "marc-21")
        return MARC::FileType::BINARY;
    else if (output_format == "marc-xml")
        return MARC::FileType::XML;

    LOG_ERROR("Unknown MARC file type '" + output_format + "'");
}


MarcFormatHandler::MarcFormatHandler(DbConnection * const db_connection, const std::string &output_file,
                                     const std::shared_ptr<const HarvestParams> &harvest_params, const std::string &output_format)
    : FormatHandler(db_connection, output_format.empty() ? GuessOutputFormat(output_file) : output_format, output_file, harvest_params),
      marc_writer_(MARC::Writer::Factory(output_file_, output_format.empty() ? MARC::FileType::AUTO : GetOutputMarcFileType(output_format)))
{
}


void MarcFormatHandler::identifyMissingLanguage(ItemParameters * const node_parameters) {
    static const unsigned MINIMUM_TOKEN_COUNT = 5;

    if (site_params_->expected_languages_.size() == 1) {
        node_parameters->language_ = *site_params_->expected_languages_.begin();
        LOG_DEBUG("language set to default language '" + node_parameters->language_ + "'");
    } else if (not site_params_->expected_languages_.empty()) {
        // attempt to automatically detect the language
        std::vector<std::string> top_languages;
        std::string record_text;

        if (site_params_->expected_languages_text_fields_.empty() or site_params_->expected_languages_text_fields_ == "title") {
            record_text = node_parameters->title_;
            // use naive tokenization to count tokens in the title
            // additionally use abstract if we have too few tokens in the title
            if (StringUtil::CharCount(record_text, ' ') < MINIMUM_TOKEN_COUNT) {
                record_text += " " + node_parameters->abstract_note_;
                LOG_DEBUG("too few tokens in title. applying heuristic on the abstract as well");
            }
        } else if (site_params_->expected_languages_text_fields_ == "abstract")
            record_text = node_parameters->abstract_note_;
        else if (site_params_->expected_languages_text_fields_ == "title+abstract")
            record_text = node_parameters->title_ + " " + node_parameters->abstract_note_;
        else
            LOG_ERROR("unknown text field '" + site_params_->expected_languages_text_fields_ + "' for language detection");

        NGram::ClassifyLanguage(record_text, &top_languages, site_params_->expected_languages_, NGram::DEFAULT_NGRAM_NUMBER_THRESHOLD);
        node_parameters->language_ = top_languages.front();
        LOG_INFO("automatically detected language to be '" + node_parameters->language_ + "'");
    }
}


void MarcFormatHandler::extractItemParameters(std::shared_ptr<const JSON::ObjectNode> object_node, ItemParameters * const node_parameters) {
    // Item Type
    node_parameters->item_type_ = object_node->getStringValue("itemType");

    // Title
    node_parameters->title_ = object_node->getOptionalStringValue("title");
    if (site_params_->review_regex_ != nullptr and site_params_->review_regex_->matched(node_parameters->title_)) {
        LOG_DEBUG("title matched review pattern");
        node_parameters->item_type_ = "review";
    }

    // Short Title
    node_parameters->short_title_ = object_node->getOptionalStringValue("shortTitle");
    if (site_params_->review_regex_ != nullptr and site_params_->review_regex_->matched(node_parameters->short_title_)) {
        LOG_DEBUG("short title matched review pattern");
        node_parameters->item_type_ = "review";
    }

    // Creators
    const auto creator_nodes(object_node->getOptionalArrayNode("creators"));
    if (creator_nodes != nullptr) {
        for (const auto creator_node : *creator_nodes) {
            Creator creator;
            auto creator_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, creator_node));
            creator.first_name_ = creator_object_node->getOptionalStringValue("firstName");
            creator.last_name_ = creator_object_node->getOptionalStringValue("lastName");
            creator.affix_ = creator_object_node->getOptionalStringValue("affix");
            creator.title_ = creator_object_node->getOptionalStringValue("title");
            creator.type_ = creator_object_node->getOptionalStringValue("creatorType");
            creator.ppn_ = creator_object_node->getOptionalStringValue("ppn");
            creator.gnd_number_ = creator_object_node->getOptionalStringValue("gnd_number");
            node_parameters->creators_.emplace_back(creator);
        }
    }

    // Publication Title
    node_parameters->publication_title_ = object_node->getOptionalStringValue("publicationTitle");

    // Serial Short Title
    node_parameters->abbreviated_publication_title_ = object_node->getOptionalStringValue("journalAbbreviation");

    // DOI
    node_parameters->doi_ = object_node->getOptionalStringValue("DOI");
    if (node_parameters->doi_.empty()) {
        const std::string extra(object_node->getOptionalStringValue("extra"));
        if (not extra.empty()) {
            static RegexMatcher * const doi_matcher(RegexMatcher::RegexMatcherFactory("^DOI:\\s*([0-9a-zA-Z./]+)$"));
            if (doi_matcher->matched(extra))
                node_parameters->doi_ = (*doi_matcher)[1];
        }
    }

    // Abstract Note
    node_parameters->abstract_note_ = object_node->getOptionalStringValue("abstractNote");
    if (site_params_->review_regex_ != nullptr and site_params_->review_regex_->matched(node_parameters->abstract_note_)) {
        LOG_DEBUG("abstract matched review pattern");
        node_parameters->item_type_ = "review";
    }

    // Keywords
    const std::shared_ptr<const JSON::JSONNode>tags_node(object_node->getNode("tags"));
    if (tags_node != nullptr) {
        const std::shared_ptr<const JSON::ArrayNode> tags(JSON::JSONNode::CastToArrayNodeOrDie("tags", tags_node));
        for (const auto &tag : *tags) {
            const std::shared_ptr<const JSON::ObjectNode> tag_object(JSON::JSONNode::CastToObjectNodeOrDie("tag", tag));
            const std::shared_ptr<const JSON::JSONNode> tag_node(tag_object->getNode("tag"));
            if (tag_node == nullptr)
                LOG_ERROR("unexpected: tag object does not contain a \"tag\" entry!");
            else if (tag_node->getType() != JSON::JSONNode::STRING_NODE)
                LOG_ERROR("unexpected: tag object's \"tag\" entry is not a string node!");
            else {
                const std::shared_ptr<const JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie("tag", tag_node));
                const std::string value(string_node->getValue());
                node_parameters->keywords_.emplace_back(value);

                if (site_params_->review_regex_ != nullptr and site_params_->review_regex_->matched(value)) {
                    LOG_DEBUG("keyword '" + value + "' matched review pattern");
                    node_parameters->item_type_ = "review";
                }
            }
        }
    }

    // Language
    node_parameters->language_ = object_node->getOptionalStringValue("language");
    if (node_parameters->language_.empty() or site_params_->force_automatic_language_detection_) {
        if (site_params_->force_automatic_language_detection_)
            LOG_DEBUG("forcing automatic language detection");
        identifyMissingLanguage(node_parameters);
    } else if (site_params_->expected_languages_.size() == 1 and *site_params_->expected_languages_.begin() != node_parameters->language_) {
        LOG_WARNING("expected language '" + *site_params_->expected_languages_.begin() + "' but found '"
                    + node_parameters->language_ + "'");
    }

    // Copyright
    node_parameters->copyright_ = object_node->getOptionalStringValue("rights");

    // Date
    node_parameters->date_ = object_node->getOptionalStringValue("date");

    // Volume
    node_parameters->volume_ = object_node->getOptionalStringValue("volume");

    // Issue
    node_parameters->issue_ = object_node->getOptionalStringValue("issue");

    // Pages
    node_parameters->pages_ = object_node->getOptionalStringValue("pages");

    // URL
    node_parameters->url_ = object_node->getOptionalStringValue("url");

    // Non-standard metadata:
    const auto notes_nodes(object_node->getOptionalArrayNode("notes"));
    if (notes_nodes != nullptr) {
        for (const auto note_node : *notes_nodes) {
            auto note_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, note_node));
            const std::string key_value_pair(note_object_node->getStringValue("note"));
            const auto first_colon_pos(key_value_pair.find(':'));
            if (unlikely(first_colon_pos == std::string::npos)) {
                LOG_WARNING("additional metadata in \"notes\" is missing a colon! data: '" + key_value_pair + "'");
                continue;   // could be a valid note added by the translator
            }
            node_parameters->notes_key_value_pairs_[key_value_pair.substr(0, first_colon_pos)] = key_value_pair.substr(first_colon_pos + 1);
        }
    }
}


static const size_t MIN_CONTROl_FIELD_LENGTH(1);
static const size_t MIN_DATA_FIELD_LENGTH(2 /*indicators*/ + 1 /*subfield separator*/ + 1 /*subfield code*/ + 1 /*subfield value*/);


static bool InsertAdditionalField(MARC::Record * const record, const std::string &additional_field) {
    if (unlikely(additional_field.length() < MARC::Record::TAG_LENGTH))
        return false;
    const MARC::Tag tag(additional_field.substr(0, MARC::Record::TAG_LENGTH));
    if ((tag.isTagOfControlField() and additional_field.length() < MARC::Record::TAG_LENGTH + MIN_CONTROl_FIELD_LENGTH)
        or (not tag.isTagOfControlField() and additional_field.length() < MARC::Record::TAG_LENGTH + MIN_DATA_FIELD_LENGTH))
        return false;
    record->insertField(tag, additional_field.substr(MARC::Record::TAG_LENGTH));

    return true;
}


static void InsertAdditionalFields(const std::string &parameter_source, MARC::Record * const record,
                                   const std::vector<std::string> &additional_fields)
{
    for (const auto &additional_field : additional_fields) {
        if (not InsertAdditionalField(record, additional_field))
            LOG_ERROR("bad additional field \"" + StringUtil::CStyleEscape(additional_field) +"\" in \"" + parameter_source + "\"!");
    }
}


static void ProcessNonStandardMetadata(MARC::Record * const record, const std::map<std::string, std::string> &notes_key_value_pairs,
                                       const std::vector<std::string> &non_standard_metadata_fields)
{
    static auto placeholder_matcher(RegexMatcher::RegexMatcherFactoryOrDie("%(.+)%"));

    for (auto non_standard_metadata_field : non_standard_metadata_fields) {
        if (not placeholder_matcher->matched(non_standard_metadata_field))
            LOG_WARNING("non-standard metadata field '" + non_standard_metadata_field + "' has no placeholders");
        else {
            std::string first_missing_placeholder;
            for (unsigned i(1); i < placeholder_matcher->getLastMatchCount(); ++i) {
                const auto placeholder((*placeholder_matcher)[i]);
                const auto note_match(notes_key_value_pairs.find(placeholder));
                if (note_match == notes_key_value_pairs.end()) {
                    first_missing_placeholder = placeholder;
                    break;
                }

                non_standard_metadata_field = StringUtil::ReplaceString((*placeholder_matcher)[0], note_match->second,
                                                                        non_standard_metadata_field);
            }

            if (not first_missing_placeholder.empty()) {
                LOG_DEBUG("non-standard metadata field '" + non_standard_metadata_field + "' has missing placeholder(s) '" +
                            first_missing_placeholder + "'");
                break;
            }

            if (InsertAdditionalField(record, non_standard_metadata_field))
                LOG_DEBUG("inserted non-standard metadata field '" + non_standard_metadata_field + "'");
            else
                LOG_ERROR("failed to add non-standard metadata field! (Content was \"" + non_standard_metadata_field + "\")");
        }
    }
}


void SelectIssnAndPpn(const std::string &issn_zotero, const std::string &issn_online, const std::string &issn_print,
                      const std::string &ppn_online, const std::string &ppn_print,
                      std::string * const issn_selected, std::string * const ppn_selected)
{
    if (not issn_online.empty() and (issn_zotero.empty() or issn_zotero == issn_online)) {
        *issn_selected = issn_online;
        *ppn_selected = ppn_online;
        if (ppn_online.empty())
            LOG_ERROR("cannot use online ISSN \"" + issn_online + "\" because no online PPN is given!");
        LOG_DEBUG("use online ISSN \"" + issn_online + "\" with online PPN \"" + ppn_online + "\"");
    } else if (not issn_print.empty() and (issn_zotero.empty() or issn_zotero == issn_print)) {
        *issn_selected = issn_print;
        *ppn_selected = ppn_print;
        if (ppn_print.empty())
            LOG_ERROR("cannot use print ISSN \"" + issn_print + "\" because no print PPN is given!");
        LOG_DEBUG("use print ISSN \"" + issn_print + "\" with print PPN \"" + ppn_print + "\"");
    } else
        LOG_ERROR("ISSN and PPN could not be chosen! ISSN online: \"" + issn_online + "\""
                  + ", ISSN print: \"" + issn_print + "\", ISSN zotero: \"" + issn_zotero + "\""
                  + ", PPN online: \"" + ppn_online + "\", PPN print: \"" + ppn_print + "\"");
}


void MarcFormatHandler::generateMarcRecord(MARC::Record * const record, const struct ItemParameters &node_parameters) {
    const std::string item_type(node_parameters.item_type_);
    *record = MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, Transformation::MapBiblioLevel(item_type));

    // Control Fields

    // Handle 001 only at the end since we need a proper hash value
    // -> c.f. last line of this function

    const std::string isil(node_parameters.isil_);
    record->insertField("003", isil);

    // ISSN and physical description
    std::string superior_ppn, issn;
    SelectIssnAndPpn(node_parameters.issn_zotero_, node_parameters.issn_online_, node_parameters.issn_print_,
                     node_parameters.superior_ppn_online_, node_parameters.superior_ppn_print_, &issn, &superior_ppn);
    if (issn == node_parameters.issn_print_ and node_parameters.issn_online_.empty())
        record->insertField("007", "tu");
    else
        record->insertField("007", "cr|||||");

    // Authors/Creators (use reverse iterator to keep order, because "insertField" inserts at first possible position)
    // The first creator is always saved in the "100" field, all following creators go into the 700 field
    unsigned num_creators_left(node_parameters.creators_.size());
    for (auto creator(node_parameters.creators_.rbegin()); creator != node_parameters.creators_.rend(); ++creator) {
        MARC::Subfields subfields;
        if (not creator->ppn_.empty())
            subfields.appendSubfield('0', "(DE-627)" + creator->ppn_);
        if (not creator->gnd_number_.empty())
            subfields.appendSubfield('0', "(DE-588)" + creator->gnd_number_);
        if (not creator->type_.empty())
            subfields.appendSubfield('4', Transformation::GetCreatorTypeForMarc21(creator->type_));

        subfields.appendSubfield('a', StringUtil::Join(std::vector<std::string>({ creator->last_name_, creator->first_name_ }), ", "));
        if (not creator->affix_.empty())
            subfields.appendSubfield('b', creator->affix_ + ".");
        if (not creator->title_.empty())
            subfields.appendSubfield('c', creator->title_);

        if (num_creators_left == 1)
            record->insertField("100", subfields, /* indicator 1 = */'1');
        else
            record->insertField("700", subfields, /* indicator 1 = */'1');

        const std::string _887_data("Autor in der Zoterovorlage [" + creator->last_name_ + ", " + creator->first_name_ + "] maschinell zugeordnet");
        record->insertField("887", { { 'a', _887_data }, { '2', "ixzom" } });

        --num_creators_left;
    }

    // Titles
    std::string title(node_parameters.title_);
    if (title.empty())
        title = node_parameters.website_title_;
    if (not title.empty())
        record->insertField("245", { { 'a', title } }, /* indicator 1 = */'0', /* indicator 2 = */'0');
    else
        LOG_ERROR("No title found");

    // Language
    if (not node_parameters.language_.empty())
        record->insertField("041", { { 'a', node_parameters.language_ } });

    // Abstract Note
    if (not node_parameters.abstract_note_.empty())
        record->insertField("520", { { 'a', node_parameters.abstract_note_ } });

    // Date & Year
    std::string year(node_parameters.year_);
    if (year.empty())
        year = TimeUtil::GetCurrentYear();
    record->insertField("264", { { 'c', year } });

    const std::string date(node_parameters.date_);
    if (not date.empty() and item_type != "journalArticle" and item_type != "review")
        record->insertField("362", { { 'a', date } });

    // URL
    const std::string url(node_parameters.url_);
    if (not url.empty())
        record->insertField("856", { { 'u', url } }, /* indicator1 = */'4', /* indicator2 = */'0');

    // DOI
    const std::string doi(node_parameters.doi_);
    if (not doi.empty()) {
        record->insertField("024", { { 'a', doi }, { '2', "doi" } }, '7');
        const std::string doi_url("https://doi.org/" + doi);
        if (doi_url != url)
            record->insertField("856", { { 'u', doi_url } }, /* indicator1 = */'4', /* indicator2 = */'0');
    }

    // Review-specific modifications
    if (item_type == "review")
        record->insertField("655", { { 'a', "!106186019!" }, { '0', "(DE-588)" } }, /* indicator1 = */' ', /* indicator2 = */'7');

    // License data
    const std::string license(node_parameters.license_);
    if (license == "l")
        record->insertField("856", { { 'z', "Kostenfrei" } }, /* indicator1 = */'4', /* indicator2 = */'0');
    else if (license == "kw")
        record->insertField("856", { { 'z', "Teilw. kostenfrei" } }, /* indicator1 = */'4', /* indicator2 = */'0');

    // Differentiating information about source (see BSZ Konkordanz MARC 936)
    MARC::Subfields _936_subfields;
    const std::string volume(node_parameters.volume_);
    const std::string issue(node_parameters.issue_);
    if (not volume.empty()) {
        _936_subfields.appendSubfield('d', volume);
        if (not issue.empty())
            _936_subfields.appendSubfield('e', issue);
    } else if (not issue.empty())
        _936_subfields.appendSubfield('d', issue);

    const std::string pages(node_parameters.pages_);
    if (not pages.empty()) {
        if (pages.find('-') == std::string::npos)
            _936_subfields.appendSubfield('g', pages);
        else
            _936_subfields.appendSubfield('h', pages);
    }

    _936_subfields.appendSubfield('j', year);
    if (not _936_subfields.empty())
        record->insertField("936", _936_subfields, 'u', 'w');

    // Information about superior work (See BSZ Konkordanz MARC 773)
    MARC::Subfields _773_subfields;
    const std::string publication_title(node_parameters.publication_title_);
    if (not publication_title.empty()) {
        _773_subfields.appendSubfield('i', "In: ");
        _773_subfields.appendSubfield('t', publication_title);
    }
    if (not issn.empty())
        _773_subfields.appendSubfield('x', issn);
    if (not superior_ppn.empty())
        _773_subfields.appendSubfield('w', "(DE-627)" + superior_ppn);

    // 773g, example: "52 (2018), 1, Seite 1-40" => <volume>(<year>), <issue>, S. <pages>
    const bool _773_subfields_iaxw_present(not _773_subfields.empty());
    bool _773_subfield_g_present(false);
    std::string g_content;
    if (not volume.empty()) {
        g_content += volume + " (" + year + ")";
        if (not issue.empty())
            g_content += ", " + issue;

        if (not pages.empty())
            g_content += ", Seite " + pages;

        _773_subfields.appendSubfield('g', g_content);
        _773_subfield_g_present = true;
    }

    if (_773_subfields_iaxw_present and _773_subfield_g_present)
        record->insertField("773", _773_subfields, '0', '8');
    else
        record->insertField("773", _773_subfields);

    // Keywords
    for (const auto &keyword : node_parameters.keywords_)
        record->insertField(MARC::GetIndexField(TextUtil::CollapseAndTrimWhitespace(keyword)));

    // SSG numbers
    if (node_parameters.ssgn_ != BSZTransform::SSGNType::INVALID) {
        MARC::Subfields _084_subfields;
        switch(node_parameters.ssgn_) {
        case BSZTransform::SSGNType::FG_0:
            _084_subfields.appendSubfield('a', "0");
            break;
        case BSZTransform::SSGNType::FG_1:
            _084_subfields.appendSubfield('a', "1");
            break;
        case BSZTransform::SSGNType::FG_01:
            _084_subfields.appendSubfield('a', "0");
            _084_subfields.appendSubfield('a', "1");
            break;
        default:
            break;
        }
        _084_subfields.appendSubfield('2', "ssgn");
    }

    record->insertField("001", site_params_->group_params_->name_ + "#" + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d")
                        + "#" + StringUtil::ToHexString(MARC::CalcChecksum(*record)));

    InsertAdditionalFields("site params (" + site_params_->journal_name_ + ")", record, site_params_->additional_fields_);
    InsertAdditionalFields("group params (" + site_params_->group_params_->name_ + ")", record,
                           site_params_->group_params_->additional_fields_);

    ProcessNonStandardMetadata(record, node_parameters.notes_key_value_pairs_, site_params_->non_standard_metadata_fields_);

    if (not site_params_->zeder_id_.empty())
        record->insertField("ZID", { { 'a', site_params_->zeder_id_ } });

    if (not node_parameters.harvest_url_.empty())
        record->insertField("URL", { { 'a', node_parameters.harvest_url_ } });

    if (not node_parameters.journal_name_.empty())
        record->insertField("JOU", { { 'a', node_parameters.journal_name_ } });

    // remove any fields that match removal patterns
    for (const auto &filter : site_params_->field_removal_filters_) {
        auto tag_and_subfield_code(filter.first);
        const auto matched_fields(record->getMatchedFields(filter.first, filter.second.get()));
        for (const auto &matched_field : matched_fields) {
            record->erase(matched_field);
            LOG_DEBUG("erased field '" + tag_and_subfield_code + "' due to removal filter '" + filter.second->getPattern() + "'");
        }
    }
}


// Extracts information from the ubtue node
void MarcFormatHandler::extractCustomNodeParameters(std::shared_ptr<const JSON::JSONNode> custom_node,
                                                    CustomNodeParameters * const custom_node_params)
{
    const std::shared_ptr<const JSON::ObjectNode>custom_object(JSON::JSONNode::CastToObjectNodeOrDie("ubtue", custom_node));

    const auto creator_nodes(custom_object->getOptionalArrayNode("creators"));
    if (creator_nodes != nullptr) {
        for (const auto &creator_node : *creator_nodes) {
            Creator creator;
            const auto creator_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, creator_node));
            creator.first_name_ = creator_object_node->getOptionalStringValue("firstName");
            creator.last_name_ = creator_object_node->getOptionalStringValue("lastName");
            creator.affix_ = creator_object_node->getOptionalStringValue("affix");
            creator.type_ = creator_object_node->getOptionalStringValue("creatorType");
            creator.ppn_ = creator_object_node->getOptionalStringValue("ppn");
            creator.gnd_number_ = creator_object_node->getOptionalStringValue("gnd_number");
            custom_node_params->creators_.emplace_back(creator);
        }
    }

    custom_node_params->issn_zotero_ = custom_object->getOptionalStringValue("issn_zotero");
    custom_node_params->issn_online_ = custom_object->getOptionalStringValue("issn_online");
    custom_node_params->issn_print_ = custom_object->getOptionalStringValue("issn_print");
    custom_node_params->journal_name_ = custom_object->getOptionalStringValue("journal_name");
    custom_node_params->harvest_url_ = custom_object->getOptionalStringValue("harvest_url");
    custom_node_params->volume_ = custom_object->getOptionalStringValue("volume");
    custom_node_params->license_ = custom_object->getOptionalStringValue("licenseCode");
    custom_node_params->ssg_number_ = custom_object->getOptionalStringValue("ssgNumbers");
    custom_node_params->date_normalized_ = custom_object->getOptionalStringValue("date_normalized");
    custom_node_params->superior_ppn_online_ = custom_object->getOptionalStringValue("ppn_online");
    custom_node_params->superior_ppn_print_ = custom_object->getOptionalStringValue("ppn_print");
    custom_node_params->isil_ = custom_object->getOptionalStringValue("isil");
    custom_node_params->issn_mapped_language_ = custom_object->getOptionalStringValue("issn_language");
}


std::string GetCustomValueIfNotEmpty(const std::string &custom_value, const std::string &item_value) {
    return (not custom_value.empty()) ? custom_value : item_value;
}


void MarcFormatHandler::mergeCustomParametersToItemParameters(struct ItemParameters * const item_parameters,
                                                              struct CustomNodeParameters &custom_node_params)
{
    item_parameters->issn_zotero_ = custom_node_params.issn_zotero_;
    item_parameters->issn_online_ = custom_node_params.issn_online_;
    item_parameters->issn_print_ = custom_node_params.issn_print_;
    item_parameters->superior_ppn_online_ = custom_node_params.superior_ppn_online_;
    item_parameters->superior_ppn_print_ = custom_node_params.superior_ppn_print_;
    item_parameters->journal_name_ = GetCustomValueIfNotEmpty(custom_node_params.journal_name_, item_parameters->journal_name_);
    item_parameters->harvest_url_ = GetCustomValueIfNotEmpty(custom_node_params.harvest_url_, item_parameters->harvest_url_);
    item_parameters->license_ = GetCustomValueIfNotEmpty(custom_node_params.license_, item_parameters->license_);

    item_parameters->ssgn_ = BSZTransform::SSGNType::INVALID;
    if (site_params_->ssgn_ != BSZTransform::SSGNType::INVALID)
        item_parameters->ssgn_ = site_params_->ssgn_;
    else
        item_parameters->ssgn_ = BSZTransform::GetSSGNTypeFromString(custom_node_params.ssg_number_);

    // Use the custom creator version if present since it may contain additional information such as a PPN
    if (custom_node_params.creators_.size())
        item_parameters->creators_ = custom_node_params.creators_;
    item_parameters->date_ = GetCustomValueIfNotEmpty(custom_node_params.date_normalized_, item_parameters->date_);
    item_parameters->isil_ = GetCustomValueIfNotEmpty(custom_node_params.isil_, item_parameters->isil_);
    if (item_parameters->year_.empty() and not custom_node_params.date_normalized_.empty()) {
        unsigned year;
        if (TimeUtil::StringToYear(custom_node_params.date_normalized_, &year))
            item_parameters->year_ = StringUtil::ToString(year);
    }
    if (item_parameters->language_.empty()) {
        item_parameters->language_ = custom_node_params.issn_mapped_language_;
        LOG_INFO("set language to ISSN-mapped value '" + item_parameters->language_ + "'");
    }
}


void MarcFormatHandler::handleTrackingAndWriteRecord(const MARC::Record &new_record, const bool keep_delivered_records,
                                                     struct ItemParameters &item_params, unsigned * const previously_downloaded_count)
{
    const std::string record_url(item_params.url_);
    const std::string checksum(StringUtil::ToHexString(MARC::CalcChecksum(new_record)));
    if (record_url.empty())
        LOG_ERROR("\"record_url\" has not been set!");

    if (keep_delivered_records or not delivery_tracker_.hasAlreadyBeenDelivered(record_url, checksum))
        marc_writer_->write(new_record);
    else {
        ++(*previously_downloaded_count);
        LOG_INFO("skipping URL '" + record_url + "' - already delivered");
    }
}


std::pair<unsigned, unsigned> MarcFormatHandler::processRecord(const std::shared_ptr<JSON::ObjectNode> &object_node) {
    if (not JSON::IsValidUTF8(*object_node))
        LOG_ERROR("bad UTF8 in JSON node!");

    std::shared_ptr<const JSON::JSONNode> custom_node(object_node->getNode("ubtue"));
    CustomNodeParameters custom_node_params;
    if (custom_node != nullptr)
        extractCustomNodeParameters(custom_node, &custom_node_params);

    struct ItemParameters item_parameters;
    extractItemParameters(object_node, &item_parameters);
    mergeCustomParametersToItemParameters(&item_parameters, custom_node_params);

    MARC::Record new_record(std::string(MARC::Record::LEADER_LENGTH, ' ') /*empty dummy leader*/);
    generateMarcRecord(&new_record, item_parameters);

    std::string exclusion_string;
    if (recordMatchesExclusionFilters(new_record, object_node, &exclusion_string)) {
        LOG_INFO("skipping URL '" + item_parameters.harvest_url_ + " - excluded due to filter (" + exclusion_string + ")");
        return std::make_pair(0, 0);
    }

    unsigned previously_downloaded_count(0);
    handleTrackingAndWriteRecord(new_record, harvest_params_->force_downloads_, item_parameters, &previously_downloaded_count);
    return std::make_pair(/* record count */1, previously_downloaded_count);
}


bool MarcFormatHandler::recordMatchesExclusionFilters(const MARC::Record &new_record,
                                                      const std::shared_ptr<JSON::ObjectNode> &object_node,
                                                      std::string * const exclusion_string) const
{
    bool found_match(false);

    for (const auto &filter : site_params_->field_exclusion_filters_) {
        if (new_record.fieldOrSubfieldMatched(filter.first, filter.second.get())) {
            *exclusion_string = filter.first + "/" + filter.second->getPattern() + "/";
            found_match = true;
            break;
        }
    }

    auto metadata_exclusion_predicate = [&found_match, &exclusion_string]
                                        (const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                                         const SiteParams &site_params) -> void
    {
        const auto filter_regex(site_params.metadata_exclusion_filters_.find(node_name));
        if (filter_regex != site_params.metadata_exclusion_filters_.end()) {
            if (node->getType() != JSON::JSONNode::STRING_NODE)
                LOG_ERROR("metadata exclusion filter has invalid node type '" + node_name + "'");

            const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
            if (filter_regex->second->matched(string_node->getValue())) {
                found_match = true;
                * exclusion_string = node_name + "/" + filter_regex->second->getPattern() + "/";
            }
        }
    };

    if (not site_params_->metadata_exclusion_filters_.empty())
        JSON::VisitLeafNodes("root", object_node, metadata_exclusion_predicate, std::ref(*site_params_));

    return found_match;
}


// If "key" is in "map", then return the mapped value, o/w return "key".
inline std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map) {
    const auto &key_and_value(map.find(key));
    return (key_and_value == map.cend()) ? key : key_and_value->second;
}


void AugmentJsonCreators(const std::shared_ptr<JSON::ArrayNode> creators_array, const SiteParams &site_params,
                         std::vector<std::string> * const comments)
{
    for (const auto &array_element : *creators_array) {
        const auto creator_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", array_element));

        auto first_name_node(creator_object->getNode("firstName"));
        auto last_name_node(creator_object->getNode("lastName"));
        auto first_name(creator_object->getOptionalStringValue("firstName"));
        auto last_name(creator_object->getOptionalStringValue("lastName"));
        std::string name_title, name_affix;
        BSZTransform::PostProcessAuthorName(&first_name, &last_name, &name_title, &name_affix);
        if (not name_title.empty()) {
            const std::shared_ptr<JSON::StringNode> title_node(new JSON::StringNode(name_title));
            creator_object->insert("title", title_node);
        }
        if (not name_affix.empty()) {
            const std::shared_ptr<JSON::StringNode> affix_node(new JSON::StringNode(name_affix));
            creator_object->insert("affix", affix_node);
        }

        if (not last_name.empty()) {
            std::string name(last_name);
            if (not first_name.empty())
                name += ", " + first_name;

            const std::string PPN(BSZTransform::DownloadAuthorPPN(name, site_params.group_params_->author_ppn_lookup_url_));
            if (not PPN.empty()) {
                comments->emplace_back("Added author PPN " + PPN + " for author " + name);
                creator_object->insert("ppn", std::make_shared<JSON::StringNode>(PPN));
            }

            const std::string gnd_number(LobidUtil::GetAuthorGNDNumber(name, site_params.group_params_->author_gnd_lookup_query_params_));
            if (not gnd_number.empty()) {
                comments->emplace_back("Added author GND number " + gnd_number + " for author " + name);
                creator_object->insert("gnd_number", std::make_shared<JSON::StringNode>(gnd_number));
            }
        }

        if (first_name_node != nullptr)
            JSON::JSONNode::CastToStringNodeOrDie("firstName", first_name_node)->setValue(first_name);
        if (last_name_node != nullptr)
            JSON::JSONNode::CastToStringNodeOrDie("lastName", last_name_node)->setValue(last_name);
    }
}


void SuppressJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const SiteParams &site_params)
{
    const auto suppression_regex(site_params.metadata_suppression_filters_.find(node_name));
    if (suppression_regex != site_params.metadata_suppression_filters_.end()) {
        if (node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("metadata suppression filter has invalid node type '" + node_name + "'");

        const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
        if (suppression_regex->second->matched(string_node->getValue())) {
            LOG_DEBUG("suppression regex '" + suppression_regex->second->getPattern() +
                        "' matched metadata field '" + node_name + "' value '" + string_node->getValue() + "'");
            string_node->setValue("");
        }
    }
}


void OverrideJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const SiteParams &site_params)
{
    static const std::string ORIGINAL_VALUE_SPECIFIER("%org%");

    const auto override_pattern(site_params.metadata_overrides_.find(node_name));
    if (override_pattern != site_params.metadata_overrides_.end()) {
        if (node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("metadata override has invalid node type '" + node_name + "'");

        const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
        const auto string_value(string_node->getValue());
        const auto override_string(StringUtil::ReplaceString(ORIGINAL_VALUE_SPECIFIER, string_value,override_pattern->second));

        LOG_DEBUG("metadata field '" + node_name + "' value changed from '" + string_value + "' to '" + override_string + "'");
        string_node->setValue(override_string);
    }
}


void AugmentJson(const std::string &harvest_url, const std::shared_ptr<JSON::ObjectNode> &object_node, const SiteParams &site_params) {
    static std::unique_ptr<RegexMatcher> page_range_matcher(RegexMatcher::RegexMatcherFactoryOrDie("^(.+)-(.+)$"));
    static std::unique_ptr<RegexMatcher> page_range_digit_matcher(RegexMatcher::RegexMatcherFactoryOrDie("^(\\d+)-(\\d+)$"));

    LOG_DEBUG("Augmenting JSON...");
    std::map<std::string, std::string> custom_fields;
    std::vector<std::string> comments;
    std::string issn_raw, issn_zotero;
    std::shared_ptr<JSON::StringNode> language_node(nullptr);
    Transformation::TestForUnknownZoteroKey(object_node);

    JSON::VisitLeafNodes("root", object_node, SuppressJsonMetadata, std::ref(site_params));
    JSON::VisitLeafNodes("root", object_node, OverrideJsonMetadata, std::ref(site_params));

    for (const auto &key_and_node : *object_node) {
        if (key_and_node.first == "language") {
            language_node = JSON::JSONNode::CastToStringNodeOrDie("language", key_and_node.second);
            const std::string language_json(language_node->getValue());
            const std::string language_mapped(OptionalMap(language_json,
                                                          site_params.global_params_->maps_->language_to_language_code_map_));
            if (language_json != language_mapped) {
                language_node->setValue(language_mapped);
                comments.emplace_back("changed \"language\" from \"" + language_json + "\" to \"" + language_mapped + "\"");
            }
        } else if (key_and_node.first == "creators") {
            std::shared_ptr<JSON::ArrayNode> creators_array(JSON::JSONNode::CastToArrayNodeOrDie("creators", key_and_node.second));
            AugmentJsonCreators(creators_array, site_params, &comments);
        } else if (key_and_node.first == "ISSN") {
            if (not site_params.ISSN_online_.empty() or
                not site_params.ISSN_print_.empty()) {
                continue;   // we'll just use the override
            }
            issn_raw = JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue();
            if (unlikely(not MiscUtil::NormaliseISSN(issn_raw, &issn_zotero))) {
                // the raw ISSN string probably contains multiple ISSN's that can't be distinguished
                throw std::runtime_error("\"" + issn_raw + "\" is invalid (multiple ISSN's?)!");
            } else
                custom_fields.emplace(std::pair<std::string, std::string>("issn_zotero", issn_zotero));
        } else if (key_and_node.first == "date") {
            const std::string date_raw(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue());
            custom_fields.emplace(std::pair<std::string, std::string>("date_raw", date_raw));
            const std::string date_normalized(Transformation::NormalizeDate(date_raw, site_params.strptime_format_));
            custom_fields.emplace(std::pair<std::string, std::string>("date_normalized", date_normalized));
            comments.emplace_back("normalized date to: " + date_normalized);
        } else if (key_and_node.first == "volume" or key_and_node.first == "issue") {
            std::shared_ptr<JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second));
            if (string_node->getValue() == "0")
                string_node->setValue("");
        } else if (key_and_node.first == "pages") {
            auto pages_node(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second));
            const auto page_value(pages_node->getValue());

            // force uppercase for roman numeral detection
            if (page_range_matcher->matched(StringUtil::ToUpper(page_value))) {
                std::string converted_page_value;
                if (TextUtil::IsRomanNumeral((*page_range_matcher)[1]))
                    converted_page_value += std::to_string(StringUtil::RomanNumeralToDecimal((*page_range_matcher)[1]));
                else
                    converted_page_value += (*page_range_matcher)[1];

                converted_page_value += "-";

                if (TextUtil::IsRomanNumeral((*page_range_matcher)[2]))
                    converted_page_value += std::to_string(StringUtil::RomanNumeralToDecimal((*page_range_matcher)[2]));
                else
                    converted_page_value += (*page_range_matcher)[2];

                if (converted_page_value != page_value) {
                    LOG_DEBUG("converted roman numeral page range '" + page_value + "' to decimal page range '"
                              + converted_page_value + "'");
                    pages_node->setValue(converted_page_value);
                }
            }

            if (page_range_digit_matcher->matched(pages_node->getValue()) and (*page_range_digit_matcher)[1] == (*page_range_digit_matcher)[2])
                pages_node->setValue((*page_range_digit_matcher)[1]);
        }
    }

    // use ISSN/PPN specified in the config file if any
    if (not site_params.ISSN_print_.empty())
        custom_fields.emplace(std::pair<std::string, std::string>("issn_print", site_params.ISSN_print_));
    if (not site_params.ISSN_online_.empty())
        custom_fields.emplace(std::pair<std::string, std::string>("issn_online", site_params.ISSN_online_));
    if (not site_params.PPN_online_.empty())
        custom_fields.emplace(std::pair<std::string, std::string>("ppn_online", site_params.PPN_online_));
    if (not site_params.PPN_print_.empty())
        custom_fields.emplace(std::pair<std::string, std::string>("ppn_print", site_params.PPN_print_));

    std::string issn_selected, ppn_selected;
    SelectIssnAndPpn(issn_zotero, site_params.ISSN_online_, site_params.ISSN_print_, site_params.PPN_online_, site_params.PPN_print_,
                     &issn_selected, &ppn_selected);

    // ISSN specific overrides
    if (not issn_selected.empty()) {
        // language
        const auto ISSN_and_language(site_params.global_params_->maps_->ISSN_to_language_code_map_.find(issn_selected));
        if (ISSN_and_language != site_params.global_params_->maps_->ISSN_to_language_code_map_.cend()) {
            // this will be consumed in the later stages depending on the results of the language detection heuristic
            custom_fields.emplace(std::make_pair("issn_language", ISSN_and_language->second));
        }

        // volume
        const std::string volume(object_node->getOptionalStringValue("volume"));
        if (volume.empty()) {
            const auto ISSN_and_volume(site_params.global_params_->maps_->ISSN_to_volume_map_.find(issn_selected));
            if (ISSN_and_volume != site_params.global_params_->maps_->ISSN_to_volume_map_.cend()) {
                std::shared_ptr<JSON::StringNode> volume_node(new JSON::StringNode(ISSN_and_volume->second));
                object_node->insert("volume", volume_node);
            }
        }

        // license code
        const auto ISSN_and_license_code(site_params.global_params_->maps_->ISSN_to_licence_map_.find(issn_selected));
        if (ISSN_and_license_code != site_params.global_params_->maps_->ISSN_to_licence_map_.end()) {
            if (ISSN_and_license_code->second != "l" and ISSN_and_license_code->second != "kw")
                LOG_ERROR("ISSN_to_licence.map contains an ISSN that has an unknown code \"" + ISSN_and_license_code->second + "\"");
            else
                custom_fields.emplace(std::pair<std::string, std::string>("licenseCode", ISSN_and_license_code->second));
        }

        // SSG numbers:
        const auto ISSN_and_SSGN_numbers(site_params.global_params_->maps_->ISSN_to_SSG_map_.find(issn_selected));
        if (ISSN_and_SSGN_numbers != site_params.global_params_->maps_->ISSN_to_SSG_map_.end())
            custom_fields.emplace(std::pair<std::string, std::string>("ssgNumbers", ISSN_and_SSGN_numbers->second));


    } else
        LOG_WARNING("No suitable ISSN was found!");

    // Add the parent journal name for tracking changes to the harvested URL
    custom_fields.emplace(std::make_pair("journal_name", site_params.journal_name_));
    // save harvest URL in case of a faulty translator that doesn't correctly retrieve it
    custom_fields.emplace(std::make_pair("harvest_url", harvest_url));

    // Add ISIL for later use
    const std::string isil(site_params.group_params_->isil_);
    custom_fields.emplace(std::make_pair("isil", isil));

    // Insert custom node with fields and comments
    if (comments.size() > 0 or custom_fields.size() > 0) {
        std::shared_ptr<JSON::ObjectNode> custom_object(new JSON::ObjectNode);
        if (comments.size() > 0) {
            std::shared_ptr<JSON::ArrayNode> comments_node(new JSON::ArrayNode);
            for (const auto &comment : comments) {
                std::shared_ptr<JSON::StringNode> comment_node(new JSON::StringNode(comment));
                comments_node->push_back(comment_node);
            }
            custom_object->insert("comments", comments_node);
        }

        for (const auto &custom_field : custom_fields) {
            std::shared_ptr<JSON::StringNode> custom_field_node(new JSON::StringNode(custom_field.second));
            custom_object->insert(custom_field.first, custom_field_node);
        }

        object_node->insert("ubtue", custom_object);
    }

    LOG_DEBUG("Augmented JSON: " + object_node->toString());
}


const std::shared_ptr<RegexMatcher> LoadSupportedURLsRegex(const std::string &map_directory_path) {
    std::string combined_regex;
    for (const auto line : FileUtil::ReadLines(map_directory_path + "targets.regex")) {
        if (likely(not line.empty())) {
            if (likely(not combined_regex.empty()))
                combined_regex += '|';
            combined_regex += "(?:" + line + ")";
        }
    }

    std::string err_msg;
    std::shared_ptr<RegexMatcher> supported_urls_regex(RegexMatcher::RegexMatcherFactory(combined_regex, &err_msg));
    if (supported_urls_regex == nullptr)
        LOG_ERROR("compilation of the combined regex failed: " + err_msg);

    return supported_urls_regex;
}


static std::string GetProxyHostAndPort() {
    const std::string ENV_KEY("ZTS_PROXY");
    const std::string ENV_FILE("/usr/local/etc/zts_proxy.env");

    if (not MiscUtil::EnvironmentVariableExists(ENV_KEY) and FileUtil::Exists(ENV_FILE))
        MiscUtil::SetEnvFromFile(ENV_FILE);

    if (MiscUtil::EnvironmentVariableExists(ENV_KEY)) {
        const std::string PROXY(MiscUtil::GetEnv(ENV_KEY));
        LOG_DEBUG("using proxy: " + PROXY);
        return PROXY;
    }

    return "";
}


void PreprocessHarvesterResponse(std::shared_ptr<JSON::ArrayNode> * const response_object_array) {
    // notes returned by the translation server are encoded as separate objects in the response
    // we'll need to iterate through the entires and append individual notes to their parents
    std::shared_ptr<JSON::ArrayNode> augmented_array(new JSON::ArrayNode());
    JSON::ObjectNode *last_entry(nullptr);

    for (auto entry : **response_object_array) {
        const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        const auto item_type(json_object->getStringValue("itemType"));

        if (item_type == "note") {
            if (last_entry == nullptr)
                LOG_ERROR("unexpected note object in translation server response!");

            const std::shared_ptr<JSON::ObjectNode> new_note(new JSON::ObjectNode());
            new_note->insert("note", std::shared_ptr<JSON::JSONNode>(new JSON::StringNode(json_object->getStringValue("note"))));
            last_entry->getArrayNode("notes")->push_back(new_note);
            continue;
        }

        // add the main entry to our array
        auto main_entry_copy(JSON::JSONNode::CastToObjectNodeOrDie("entry", json_object->clone()));
        main_entry_copy->insert("notes", std::shared_ptr<JSON::JSONNode>(new JSON::ArrayNode()));
        augmented_array->push_back(main_entry_copy);
        last_entry = main_entry_copy.get();
    }

    *response_object_array = augmented_array;
}


bool ValidateAugmentedJSON(const std::shared_ptr<JSON::ObjectNode> &entry, const std::shared_ptr<HarvestParams> &harvest_params) {
    static const std::vector<std::string> valid_item_types_for_online_first{
        "journalArticle", "magazineArticle"
    };

    const auto item_type(entry->getStringValue("itemType"));
    const auto issue(entry->getOptionalStringValue("issue"));
    const auto volume(entry->getOptionalStringValue("volume"));
    const auto doi(entry->getOptionalStringValue("DOI"));

    if (std::find(valid_item_types_for_online_first.begin(),
                  valid_item_types_for_online_first.end(), item_type) != valid_item_types_for_online_first.end())
    {
        if (issue.empty() and volume.empty() and not harvest_params->force_downloads_) {
            if (harvest_params->skip_online_first_articles_unconditionally_) {
                LOG_DEBUG("Skipping: online-first article unconditionally");
                return false;
            } else if (doi.empty()) {
                LOG_DEBUG("Skipping: online-first article without a DOI");
                return false;
            }
        }
    }

    return true;
}


void ApplyCrawlDelay(const std::string &harvest_url, const std::shared_ptr<HarvestParams> &harvest_params) {
    struct CrawlDelayParams {
        RobotsDotTxt robots_dot_txt_;
        TimeLimit crawl_timeout_;
    public:
        CrawlDelayParams(const std::shared_ptr<HarvestParams> &harvest_parameters, const std::string &robots_dot_txt)
         : robots_dot_txt_(robots_dot_txt), crawl_timeout_(robots_dot_txt_.getCrawlDelay("*") * 1000)
        {
            if (crawl_timeout_.getLimit() < harvest_parameters->default_crawl_delay_time_)
                crawl_timeout_ = harvest_parameters->default_crawl_delay_time_;
        }
        CrawlDelayParams(const std::shared_ptr<HarvestParams> &harvest_parameters, const TimeLimit &crawl_timeout)
         : crawl_timeout_(crawl_timeout)
        {
            if (crawl_timeout_.getLimit() < harvest_parameters->default_crawl_delay_time_)
                crawl_timeout_ = harvest_parameters->default_crawl_delay_time_;
        }
    };

    static std::unordered_map<std::string, CrawlDelayParams> HOSTNAME_TO_DELAY_PARAMS_MAP;

    const Url parsed_url(harvest_url);
    const auto hostname(parsed_url.getAuthority());
    auto delay_params(HOSTNAME_TO_DELAY_PARAMS_MAP.find(hostname));

    if (delay_params == HOSTNAME_TO_DELAY_PARAMS_MAP.end()) {
        Downloader robots_txt_downloader(parsed_url.getRobotsDotTxtUrl());
        if (robots_txt_downloader.anErrorOccurred()) {
            CrawlDelayParams default_delay_params(harvest_params, harvest_params->default_crawl_delay_time_);
            HOSTNAME_TO_DELAY_PARAMS_MAP.insert(std::make_pair(hostname, default_delay_params));

            LOG_DEBUG("couldn't retrieve robots.txt for domain '" + hostname + "'");
            return;
        }

        CrawlDelayParams new_delay_params(harvest_params, robots_txt_downloader.getMessageBody());
        HOSTNAME_TO_DELAY_PARAMS_MAP.insert(std::make_pair(hostname, new_delay_params));
        delay_params = HOSTNAME_TO_DELAY_PARAMS_MAP.find(hostname);

        LOG_INFO("set crawl-delay for domain '" + hostname + "' to " +
                 std::to_string(new_delay_params.crawl_timeout_.getLimit()) + " ms");
    }

    auto &current_delay_params(delay_params->second);

    LOG_DEBUG("sleeping for " + std::to_string(current_delay_params.crawl_timeout_.getLimit()) + " ms...");
    current_delay_params.crawl_timeout_.restart();
    current_delay_params.crawl_timeout_.sleepUntilExpired();
}


std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> &harvest_params,
                                      const SiteParams &site_params, HarvesterErrorLogger * const error_logger)
{
    if (harvest_url.empty())
        LOG_ERROR("empty URL passed to Zotero::Harvest");

    std::pair<unsigned, unsigned> record_count_and_previously_downloaded_count;
    static std::unordered_set<std::string> already_skipped_urls;
    static std::unordered_set<std::string> already_harvested_urls;

    if (already_skipped_urls.find(harvest_url) != already_skipped_urls.end())
        return record_count_and_previously_downloaded_count;
    else if (harvest_params->harvest_url_regex_ != nullptr and not harvest_params->harvest_url_regex_->matched(harvest_url)) {
        LOG_DEBUG("Skipping URL (does not match harvest URL regex): " + harvest_url);
        already_skipped_urls.insert(harvest_url);
        return record_count_and_previously_downloaded_count;
    } else if (already_harvested_urls.find(harvest_url) != already_harvested_urls.end()) {
        LOG_DEBUG("Skipping URL (already harvested during this session): " + harvest_url);
        already_skipped_urls.insert(harvest_url);
        return record_count_and_previously_downloaded_count;
    } else if (site_params.extraction_regex_ and not site_params.extraction_regex_->matched(harvest_url)) {
        LOG_DEBUG("Skipping URL (does not match extraction regex): " + harvest_url);
        already_skipped_urls.insert(harvest_url);
        return record_count_and_previously_downloaded_count;
    } else if (not harvest_params->force_downloads_) {
        auto &delivery_tracker(harvest_params->format_handler_->getDeliveryTracker());
        if (delivery_tracker.hasAlreadyBeenDelivered(harvest_url)) {
            const auto delivery_mode(site_params.delivery_mode_);
            switch (delivery_mode) {
            case BSZUpload::DeliveryMode::TEST:
            case BSZUpload::DeliveryMode::LIVE:
                LOG_DEBUG("Skipping URL (already delivered to the BSZ " +
                          BSZUpload::DELIVERY_MODE_TO_STRING_MAP.at(delivery_mode) + " server): " + harvest_url);
                break;
            default:
                LOG_DEBUG("Skipping URL (delivery mode set to NONE but URL has already been delivered?!): " + harvest_url);
                break;
            }
            already_skipped_urls.insert(harvest_url);
            return record_count_and_previously_downloaded_count;
        }
    }

    ApplyCrawlDelay(harvest_url, harvest_params);
    auto error_logger_context(error_logger->newContext(site_params.journal_name_, harvest_url));

    LOG_INFO("\nHarvesting URL: " + harvest_url);

    std::string response_body, error_message;
    unsigned response_code;
    Downloader::Params downloader_params;
    downloader_params.user_agent_ = harvest_params->user_agent_;

    bool download_succeeded(TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ DEFAULT_TIMEOUT,
                                                   downloader_params, Url(harvest_url), &response_body, &response_code,
                                                   &error_message));

    if (not download_succeeded) {
        error_logger_context.log(HarvesterErrorLogger::ZTS_CONVERSION_FAILED, error_message);
        return record_count_and_previously_downloaded_count;
    }

    // 300 => multiple matches found, try to harvest children (send the response_body right back to the server, to get all of them)
    if (response_code == 300) {
        LOG_DEBUG("multiple articles found => trying to harvest children");
        download_succeeded = TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ DEFAULT_TIMEOUT,
                                                    downloader_params, response_body, &response_body, &response_code, &error_message);
        if (not download_succeeded) {
            error_logger_context.log(HarvesterErrorLogger::DOWNLOAD_MULTIPLE_FAILED, error_message);
            return record_count_and_previously_downloaded_count;
        }
    }

    // Process either single or multiple results (response_body is array by now)
    std::shared_ptr<JSON::JSONNode> tree_root(nullptr);
    JSON::Parser json_parser(response_body);
    if (not (json_parser.parse(&tree_root))) {
        error_logger_context.log(HarvesterErrorLogger::FAILED_TO_PARSE_JSON, json_parser.getErrorMessage());
        return record_count_and_previously_downloaded_count;
    }

    auto json_array(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));
    PreprocessHarvesterResponse(&json_array);

    int processed_json_entries(0);
    for (const auto entry : *json_array) {
        const std::shared_ptr<JSON::ObjectNode> json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        ++processed_json_entries;

        try {
            const auto url(json_object->getOptionalStringValue("url", ""));
            if (already_harvested_urls.find(url) != already_harvested_urls.end()) {
                LOG_DEBUG("Skipping URL (already harvested during this session): " + url);
                already_skipped_urls.insert(harvest_url);
                continue;
            }

            AugmentJson(harvest_url, json_object, site_params);
            if (not ValidateAugmentedJSON(json_object, harvest_params))
                continue;

            auto record_counts(harvest_params->format_handler_->processRecord(json_object));
            record_count_and_previously_downloaded_count.first += record_counts.first;
            record_count_and_previously_downloaded_count.second += record_counts.second;

            if (not url.empty())
                already_harvested_urls.insert(url);
        } catch (const std::exception &x) {
            error_logger_context.autoLog("Couldn't process record! Error: " + std::string(x.what()));
            return record_count_and_previously_downloaded_count;
        }
    }

    if (processed_json_entries == 0)
        error_logger_context.log(HarvesterErrorLogger::ZTS_EMPTY_RESPONSE, "Response code = " + std::to_string(response_code));

    ++harvest_params->harvested_url_count_;

    LOG_DEBUG("Harvested " + StringUtil::ToString(record_count_and_previously_downloaded_count.first) + " record(s) from "
              + harvest_url + " of which "
              + StringUtil::ToString(record_count_and_previously_downloaded_count.first - record_count_and_previously_downloaded_count.second)
              + " records were new records.");
    return record_count_and_previously_downloaded_count;
}


UnsignedPair HarvestSite(const SimpleCrawler::SiteDesc &site_desc, SimpleCrawler::Params crawler_params,
                         const std::shared_ptr<RegexMatcher> &supported_urls_regex,
                         const std::shared_ptr<HarvestParams> &harvest_params, const SiteParams &site_params,
                         HarvesterErrorLogger * const error_logger, File * const progress_file)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;
    LOG_DEBUG("\n\nStarting crawl at base URL: " +  site_desc.start_url_);
    crawler_params.proxy_host_and_port_ = GetProxyHostAndPort();
    if (not crawler_params.proxy_host_and_port_.empty())
        crawler_params.ignore_ssl_certificates_ = true;
    SimpleCrawler crawler(site_desc, crawler_params);
    SimpleCrawler::PageDetails page_details;
    unsigned processed_url_count(0);
    while (crawler.getNextPage(&page_details)) {
        if (not supported_urls_regex->matched(page_details.url_))
            LOG_DEBUG("Skipping unsupported URL: " + page_details.url_);
        else if (page_details.error_message_.empty()) {
            const auto record_count_and_previously_downloaded_count(
                Harvest(page_details.url_, harvest_params, site_params, error_logger));
            total_record_count_and_previously_downloaded_record_count.first  += record_count_and_previously_downloaded_count.first;
            total_record_count_and_previously_downloaded_record_count.second += record_count_and_previously_downloaded_count.second;
            if (progress_file != nullptr) {
                progress_file->rewind();
                if (unlikely(not progress_file->write(
                    std::to_string(processed_url_count) + ";" + std::to_string(crawler.getRemainingCallDepth()) + ";" + page_details.url_)))
                    LOG_ERROR("failed to write progress to \"" + progress_file->getPath());
            }
        }
    }

    return total_record_count_and_previously_downloaded_record_count;
}


UnsignedPair HarvestURL(const std::string &url, const std::shared_ptr<HarvestParams> &harvest_params,
                        const SiteParams &site_params, HarvesterErrorLogger * const error_logger)
{
    return Harvest(url, harvest_params, site_params, error_logger);
}


bool FeedNeedsToBeHarvested(const std::string &feed_contents, const std::shared_ptr<HarvestParams> &harvest_params,
                            const SiteParams &site_params, const SyndicationFormat::AugmentParams &syndication_format_site_params)
{
    if (harvest_params->force_downloads_)
        return true;

    const auto last_harvest_timestamp(harvest_params->format_handler_->getDeliveryTracker().getLastDeliveryTime(site_params.journal_name_));
    if (last_harvest_timestamp == TimeUtil::BAD_TIME_T) {
        LOG_DEBUG("feed will be harvested for the first time");
        return true;
    } else {
        const auto diff((time(nullptr) - last_harvest_timestamp) / 86400);
        if (unlikely(diff < 0))
            LOG_ERROR("unexpected negative time difference '" + std::to_string(diff) + "'");

        const auto harvest_threshold(site_params.journal_update_window_ > 0 ? site_params.journal_update_window_ : harvest_params->journal_harvest_interval_);
        LOG_DEBUG("feed last harvest timestamp: " + TimeUtil::TimeTToString(last_harvest_timestamp));
        LOG_DEBUG("feed harvest threshold: " + std::to_string(harvest_threshold) + " days | diff: " + std::to_string(diff) + " days");

        if (diff >= harvest_threshold) {
            LOG_DEBUG("feed older than " + std::to_string(harvest_threshold) +
                      " days. flagging for mandatory harvesting");
            return true;
        }
    }

    // needs to be parsed again as iterating over a SyndicationFormat instance will consume its items
    std::string err_msg;
    const auto syndication_format(SyndicationFormat::Factory(feed_contents, syndication_format_site_params, &err_msg));
    for (const auto &item : *syndication_format) {
        const auto pub_date(item.getPubDate());
        if (harvest_params->force_process_feeds_with_no_pub_dates_ and pub_date == TimeUtil::BAD_TIME_T) {
            LOG_DEBUG("URL '" + item.getLink() + "' has no publication timestamp. flagging for harvesting");
            return true;
        } else if (pub_date != TimeUtil::BAD_TIME_T and std::difftime(item.getPubDate(), last_harvest_timestamp) > 0) {
            LOG_DEBUG("URL '" + item.getLink() + "' was added/updated since the last harvest of this RSS feed. flagging for harvesting");
            return true;
        }
    }

    LOG_INFO("no new, harvestable entries in feed. skipping...");
    return false;
}


UnsignedPair HarvestSyndicationURL(const std::string &feed_url, const std::shared_ptr<HarvestParams> &harvest_params,
                                   const SiteParams &site_params, HarvesterErrorLogger * const error_logger)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;
    auto error_logger_context(error_logger->newContext(site_params.journal_name_, feed_url));

    LOG_INFO("\n\nProcessing feed URL: " + feed_url);

    Downloader::Params downloader_params;
    downloader_params.proxy_host_and_port_ = GetProxyHostAndPort();
    if (not downloader_params.proxy_host_and_port_.empty())
        downloader_params.ignore_ssl_certificates_ = true;
    downloader_params.user_agent_ = harvest_params->user_agent_;
    Downloader downloader(feed_url, downloader_params);
    if (downloader.anErrorOccurred()) {
        error_logger_context.autoLog("Download problem for \"" + feed_url + "\": " + downloader.getLastErrorMessage());
        return total_record_count_and_previously_downloaded_record_count;
    }

    SyndicationFormat::AugmentParams syndication_format_site_params;
    syndication_format_site_params.strptime_format_ = site_params.strptime_format_;
    std::string err_msg;
    std::unique_ptr<SyndicationFormat> syndication_format(
        SyndicationFormat::Factory(downloader.getMessageBody(), syndication_format_site_params, &err_msg));
    if (syndication_format == nullptr) {
        error_logger_context.autoLog("Problem parsing XML document for \"" + feed_url + "\": " + err_msg);
        return total_record_count_and_previously_downloaded_record_count;
    }

    if (not FeedNeedsToBeHarvested(downloader.getMessageBody(), harvest_params, site_params, syndication_format_site_params))
        return total_record_count_and_previously_downloaded_record_count;

    const time_t last_build_date(syndication_format->getLastBuildDate());
    LOG_DEBUG(feed_url + " (" + syndication_format->getFormatName() + "):");
    LOG_DEBUG("\tTitle: " + syndication_format->getTitle());
    if (last_build_date != TimeUtil::BAD_TIME_T)
        LOG_DEBUG("\tLast build date: " + TimeUtil::TimeTToUtcString(last_build_date));
    LOG_DEBUG("\tLink: " + syndication_format->getLink());
    LOG_DEBUG("\tDescription: " + syndication_format->getDescription());

    for (const auto &item : *syndication_format) {
        const auto item_id(item.getId());
        const std::string title(item.getTitle());
        if (not title.empty())
            LOG_DEBUG("\n\nFeed Item: " + title);

        const auto record_count_and_previously_downloaded_count(Harvest(item.getLink(), harvest_params, site_params, error_logger));
        total_record_count_and_previously_downloaded_record_count += record_count_and_previously_downloaded_count;
    }

    return total_record_count_and_previously_downloaded_record_count;
}


const std::unordered_map<HarvesterErrorLogger::ErrorType, std::string> HarvesterErrorLogger::ERROR_KIND_TO_STRING_MAP{
    { UNKNOWN,                  "ERROR-UNKNOWN"  },
    { ZTS_CONVERSION_FAILED,    "ERROR-ZTS_CONVERSION_FAILED"  },
    { DOWNLOAD_MULTIPLE_FAILED, "ERROR-DOWNLOAD_MULTIPLE_FAILED"  },
    { FAILED_TO_PARSE_JSON,     "ERROR-FAILED_TO_PARSE_JSON"  },
    { ZTS_EMPTY_RESPONSE,       "ERROR-ZTS_EMPTY_RESPONSE"  },
    { BAD_STRPTIME_FORMAT,      "ERROR-BAD_STRPTIME_FORMAT"  },
};


void HarvesterErrorLogger::log(ErrorType error, const std::string &journal_name, const std::string &harvest_url, const std::string &message,
                               const bool write_to_stderr)
{
    JournalErrors *current_journal_errors(nullptr);
    auto match(journal_errors_.find(journal_name));
    if (match == journal_errors_.end()) {
        journal_errors_.insert(std::make_pair(journal_name, JournalErrors()));
        current_journal_errors = &journal_errors_[journal_name];
    } else
        current_journal_errors = &match->second;


    if (not harvest_url.empty())
        current_journal_errors->url_errors_[harvest_url] = HarvesterError{ error, message };
    else
        current_journal_errors->non_url_errors_.emplace_back(HarvesterError{ error, message });

    if (write_to_stderr)
        LOG_WARNING("[" + ERROR_KIND_TO_STRING_MAP.at(error) + "] for '" + harvest_url + "': " + message);
}


void HarvesterErrorLogger::autoLog(const std::string &journal_name, const std::string &harvest_url, const std::string &message,
                                   const bool write_to_std_error)
{
    static const std::unordered_multimap<ErrorType, RegexMatcher *> error_regexp_map{
        { BAD_STRPTIME_FORMAT,      RegexMatcher::RegexMatcherFactoryOrDie("StringToStructTm\\: don't know how to convert \\\"(.+?)\\\"") },
        { BAD_STRPTIME_FORMAT,      RegexMatcher::RegexMatcherFactoryOrDie("StringToStructTm\\: gmtime\\(3\\) failed to convert a time_t! \\((.+?)\\)") },
    };

    HarvesterError error{ UNKNOWN, "" };
    for (const auto &error_regexp : error_regexp_map) {
        if (error_regexp.second->matched(message)) {
            error.type_ = error_regexp.first;
            error.message_ = (*error_regexp.second)[1];
            break;
        }
    }

    log(error.type_, journal_name, harvest_url, error.type_ == UNKNOWN ? message : error.message_, write_to_std_error);
}


void HarvesterErrorLogger::writeReport(const std::string &report_file_path) const {
    IniFile report("", true, true);
    report.appendSection("");
    report.getSection("")->insert("has_errors", not journal_errors_.empty() ? "true" : "false");

    std::string journal_names;
    for (const auto &journal_error : journal_errors_) {
        const auto journal_name(journal_error.first);
        if (journal_name.find('|') != std::string::npos)
            LOG_ERROR("Invalid character '|' in journal name '" + journal_name + "'");

        journal_names += journal_name + "|";
        report.appendSection(journal_name);

        for (const auto &url_error : journal_error.second.url_errors_) {
            const auto error_string(ERROR_KIND_TO_STRING_MAP.at(url_error.second.type_));
            // we cannot cache the section pointer as it can get invalidated after appending a new section
            report.getSection(journal_name)->insert(url_error.first, error_string);
            report.appendSection(error_string);
            report.getSection(error_string)->insert(url_error.first, url_error.second.message_);
        }

        int i(1);
        for (const auto &non_url_error : journal_error.second.non_url_errors_) {
            const auto error_string(ERROR_KIND_TO_STRING_MAP.at(non_url_error.type_));
            const auto error_key(journal_name + "-non_url_error-" + std::to_string(i));

            report.getSection(journal_name)->insert(error_key, error_string);
            report.appendSection(error_string);
            report.getSection(error_string)->insert(error_key, non_url_error.message_);
            ++i;
        }
    }

    report.getSection("")->insert("journal_names", journal_names);
    report.write(report_file_path);
}


} // namespace Zotero
