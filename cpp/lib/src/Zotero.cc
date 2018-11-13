/** \brief Interaction with Zotero Translation Server
 *         public functions are named like endpoints
 *         see https://github.com/zotero/translation-server
 *  \author Dr. Johannes Ruscheinski
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
#include "Zotero.h"
#include <chrono>
#include <ctime>
#include <uuid/uuid.h>
#include "BSZTransform.h"
#include "DbConnection.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "LobidUtil.h"
#include "MiscUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "TimeUtil.h"
#include "UrlUtil.h"
#include "ZoteroTransformation.h"
#include "util.h"


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

const std::string DEFAULT_SUBFIELD_CODE("eng");
const std::string DEFAULT_LANGUAGE_CODE("eng");


namespace TranslationServer {


const Url GetUrl() {
    const IniFile ini("/usr/local/var/lib/tuelib/zotero.conf");
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
            const std::string &format, const std::string &json,
            std::string * const response_body, std::string * const error_message)
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
    group_name_to_params_map->emplace(section.getSectionName(), new_group_params);

    for (const auto &entry : section) {
        if (StringUtil::StartsWith(entry.name_, "add_field"))
            new_group_params.additional_fields_.emplace_back(entry.value_);
    }
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


std::pair<unsigned, unsigned> JsonFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
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


std::pair<unsigned, unsigned> ZoteroFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    if (record_count_ > 0)
        json_buffer_ += ",";
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


void MarcFormatHandler::ExtractItemParameters(std::shared_ptr<const JSON::ObjectNode> object_node, ItemParameters * const node_parameters) {
    // Item Type
    node_parameters->item_type = object_node->getStringValue("itemType");

    // Title
    node_parameters->title = object_node->getOptionalStringValue("title");

    // Short Title
    node_parameters->short_title = object_node->getOptionalStringValue("shortTitle");

    // Creators
    const auto creator_nodes(object_node->getOptionalArrayNode("creators"));
    if (creator_nodes != nullptr) {
        for (const auto creator_node : *creator_nodes) {
            Creator creator;
            auto creator_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, creator_node));
            creator.first_name = creator_object_node->getOptionalStringValue("firstName");
            creator.last_name = creator_object_node->getOptionalStringValue("lastName");
            creator.type = creator_object_node->getOptionalStringValue("creatorType");
            creator.ppn = creator_object_node->getOptionalStringValue("ppn");
            creator.gnd_number = creator_object_node->getOptionalStringValue("gnd_number");
            node_parameters->creators.emplace_back(creator);
        }
    }

    // Publication Title
    node_parameters->publication_title = object_node->getOptionalStringValue("publicationTitle");

    // Serial Short Title
    node_parameters->abbreviated_publication_title = object_node->getOptionalStringValue("journalAbbreviation");

    // DOI
    node_parameters->doi = object_node->getOptionalStringValue("DOI");
    if (node_parameters->doi.empty()) {
        const std::string extra(object_node->getOptionalStringValue("extra"));
        if (not extra.empty()) {
            static RegexMatcher * const doi_matcher(RegexMatcher::RegexMatcherFactory("^DOI:\\s*([0-9a-zA-Z./]+)$"));
            if (doi_matcher->matched(extra))
                node_parameters->doi = (*doi_matcher)[1];
        }
    }
    // Language
    node_parameters->language = object_node->getOptionalStringValue("language");

    // Copyright
    node_parameters->copyright = object_node->getOptionalStringValue("rights");

    // Date
    node_parameters->date = object_node->getOptionalStringValue("date");

    // Volume
    node_parameters->volume = object_node->getOptionalStringValue("volume");

    // Issue
    node_parameters->issue = object_node->getOptionalStringValue("issue");

    // Pages
    node_parameters->pages = object_node->getOptionalStringValue("pages");

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
                node_parameters->keywords.emplace_back(value);
            }
        }
    }

    // Abstract Note
    node_parameters->abstract_note = object_node->getOptionalStringValue("abstractNote");

    // URL
    node_parameters->url = object_node->getOptionalStringValue("url");

    // Non-standard metadata:
    const auto notes_nodes(object_node->getOptionalArrayNode("notes"));
    if (notes_nodes != nullptr) {
        for (const auto note_node : *notes_nodes) {
            auto note_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, note_node));
            const std::string key_value_pair(note_object_node->getStringValue("note"));
            const auto first_colon_pos(key_value_pair.find(':'));
            if (unlikely(first_colon_pos == std::string::npos))
                LOG_ERROR("additional metadata in \"notes\" is missing a colon!");
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
    for (const auto &key_and_value : notes_key_value_pairs) {
        const std::string key("%" + key_and_value.first + "%");
        for (const auto &non_standard_metadata_field : non_standard_metadata_fields) {
            if (non_standard_metadata_field.find(key) != std::string::npos) {
                if (not InsertAdditionalField(record, StringUtil::ReplaceString(key, key_and_value.second, non_standard_metadata_field)))
                    LOG_ERROR("failed to add non-standard metadata field! (Pattern was \"" + non_standard_metadata_field + "\")");
            }
        }
    }
}


void MarcFormatHandler::GenerateMarcRecord(MARC::Record * const record, const struct ItemParameters &node_parameters) {
    const std::string item_type(node_parameters.item_type);
    *record = MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, Transformation::MapBiblioLevel(item_type));

    // Control Fields

    // Handle 001 only at the end since we need a proper hash value
    // -> c.f. last line of this function

    const std::string isil(node_parameters.isil);
    record->insertField("003", isil); // Isil

    const std::string physical_form(node_parameters.physical_form); // Physical Form
    if (not physical_form.empty()) {
        if (physical_form == "A")
            record->insertField("007", "tu");
        else if (physical_form == "O")
            record->insertField("007", "cr uuu---uuuuu");
        else
            LOG_ERROR("unhandled value of physical form: \"" + physical_form + "\"!");
    }

    // Authors/Creators (use reverse iterator to keep order, because "insertField" inserts at first possible position)
    const std::string creator_tag((node_parameters.creators.size() == 1) ? "100" : "700");
    for (auto creator(node_parameters.creators.rbegin()); creator != node_parameters.creators.rend(); ++creator) {
        MARC::Subfields subfields;
        if (not creator->ppn.empty())
            subfields.appendSubfield('0', "(DE-576)" + creator->ppn);
        if (not creator->gnd_number.empty())
            subfields.appendSubfield('0', "(DE-588)" + creator->gnd_number);
        if (not creator->type.empty())
            subfields.appendSubfield('4', Transformation::GetCreatorTypeForMarc21(creator->type));
        subfields.appendSubfield('a', StringUtil::Join(std::vector<std::string>({creator->last_name, creator->first_name}), ", "));
        record->insertField(creator_tag, subfields, /* indicator 1*/'1');
    }

    // Titles
    std::string title(node_parameters.title);
    if (title.empty())
        title = node_parameters.website_title;
    if (not title.empty())
        record->insertField("245", { { 'a', title } }, /* indicator 1*/'0', /* indicator 2*/'0');
    else
        LOG_ERROR("No title found");

    // Language (inserted uncoditionally)
    std::string language(node_parameters.language);
    if (language.empty())
        language = DEFAULT_LANGUAGE_CODE;
    record->insertField("041", { {'a', language } });

    // Abstract Note
    const std::string abstract_note(node_parameters.abstract_note);
    if (not abstract_note.empty())
        record->insertField("520", { {'a', abstract_note } }, '3' /* indicator 1*/);

    // Date
    const std::string date(node_parameters.date);
    if (not date.empty() and item_type != "journalArticle")
        record->insertField("362", { {'a', date} });

    // URL
    const std::string url(node_parameters.url);
    if (not url.empty())
        record->insertField("856", { {'u', url} });

    // DOI
    const std::string doi(node_parameters.doi);
    if (not doi.empty()) {
        record->insertField("024", { { 'a', doi }, { '2', "doi" } }, '7');
        const std::string doi_url("https://doi.org/" + doi);
        if (doi_url != url)
            record->insertField("856", { { 'u', doi_url } });
    }

    // Differentiating information about source (see BSZ Konkordanz MARC 936)
    if (item_type == "journalArticle" or item_type == "magazineArticle" or item_type == "newspaperArticle") {
        MARC::Subfields _936_subfields;
        const std::string volume(node_parameters.volume);
        if (not volume.empty())
            _936_subfields.appendSubfield('d', volume);
        const std::string issue(node_parameters.issue);
        if (not issue.empty())
            _936_subfields.appendSubfield('e', issue);
        const std::string pages(node_parameters.pages);
        if (not pages.empty())
            _936_subfields.appendSubfield('h', pages);
        const std::string year(node_parameters.year);
        if (not year.empty())
            _936_subfields.appendSubfield('j', year);
        const std::string license(node_parameters.license);
        if (license == "l")
            _936_subfields.appendSubfield('z', "Kostenfrei");
        if (not _936_subfields.empty())
            record->insertField("936", _936_subfields);
    }

    // Information about superior work (See BSZ Konkordanz MARC 773)
    MARC::Subfields _773_subfields;
    const std::string publication_title(node_parameters.publication_title);
    if (not publication_title.empty()) {
        _773_subfields.appendSubfield('i', "In: ");
        _773_subfields.appendSubfield('a', publication_title);
    }
    const std::string issn(node_parameters.issn);
    if (not issn.empty())
        _773_subfields.appendSubfield('x', issn);
    const std::string superior_ppn(node_parameters.superior_ppn);
    if (not superior_ppn.empty())
        _773_subfields.appendSubfield('w', "(DE-576)" + superior_ppn);
    const std::string volume(node_parameters.volume);

    // 773g, example: "52(2018), 1, S. 1-40" => <volume>(<year>), <issue>, S. <pages>
    std::string g_content;
    if (not volume.empty()) {
        g_content += volume;

        const std::string year(node_parameters.year);
        if (not year.empty())
            g_content += "(" + year + ")";

        const std::string issue(node_parameters.issue);
        if (not issue.empty())
            g_content += ", " + issue;

        const std::string pages(node_parameters.pages);
        if (not pages.empty())
            g_content += ", S. " + pages;

        _773_subfields.appendSubfield('g', g_content);
    }
    record->insertField("773", _773_subfields);

    // Keywords
    BSZTransform::BSZTransform bsz_transform(*(site_params_->global_params_->maps_));
    for (const auto keyword : node_parameters.keywords) {
        std::string tag;
        char subfield, indicator2(' ');
        bsz_transform.DetermineKeywordOutputFieldFromISSN(issn, &tag, &subfield);
        if (tag == "650")
            indicator2 = '4';
        record->insertField(tag, { {subfield, keyword } }, ' ', indicator2);
    }

    // SSG numbers
    const auto ssg_numbers(node_parameters.ssg_numbers);
    if (not ssg_numbers.empty()) {
        MARC::Subfields _084_subfields;
        for (const auto ssg_number : ssg_numbers)
            _084_subfields.appendSubfield('a', ssg_number);
        _084_subfields.appendSubfield('0', "ssgn");
    }

    record->insertField("001", site_params_->group_params_->name_ + "#" + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d")
                        + "#" + StringUtil::ToHexString(MARC::CalcChecksum(*record)));

    InsertAdditionalFields("site params (" + site_params_->parent_journal_name_ + ")", record, site_params_->additional_fields_);
    InsertAdditionalFields("group params (" + site_params_->group_params_->name_ + ")", record,
                           site_params_->group_params_->additional_fields_);

    ProcessNonStandardMetadata(record, node_parameters.notes_key_value_pairs_, site_params_->non_standard_metadata_fields_);

    record->insertField("ZID", { { 'a', site_params_->zeder_id_} });
}


// Extracts information from the ubtue node
void MarcFormatHandler::ExtractCustomNodeParameters(std::shared_ptr<const JSON::JSONNode> custom_node, CustomNodeParameters * const
                                                        custom_node_params)
{
    const std::shared_ptr<const JSON::ObjectNode>custom_object(JSON::JSONNode::CastToObjectNodeOrDie("ubtue", custom_node));
    if (custom_object->getOptionalStringNode("ISSN_untagged"))
        custom_node_params->issn_normalized = custom_object->getOptionalStringValue("ISSN_untagged");
    else if (custom_object->getOptionalStringNode("ISSN_online"))
        custom_node_params->issn_normalized = custom_object->getOptionalStringValue("ISSN_online");
    else if (custom_object->getOptionalStringNode("ISSN_print"))
        custom_node_params->issn_normalized = custom_object->getOptionalStringValue("ISSN_print");
    else
        LOG_WARNING("No ISSN found for item.");


    const auto creator_nodes(custom_object->getOptionalArrayNode("creators"));
    if (creator_nodes != nullptr) {
        for (const auto creator_node : *creator_nodes) {
            Creator creator;
            auto creator_object_node(JSON::JSONNode::CastToObjectNodeOrDie(""/* intentionally empty */, creator_node));
            creator.first_name = creator_object_node->getOptionalStringValue("firstName");
            creator.last_name = creator_object_node->getOptionalStringValue("lastName");
            creator.type = creator_object_node->getOptionalStringValue("creatorType");
            creator.ppn = creator_object_node->getOptionalStringValue("ppn");
            creator.gnd_number = creator_object_node->getOptionalStringValue("gnd_number");
            custom_node_params->creators.emplace_back(creator);
        }
    }

    custom_node_params->parent_journal_name = custom_object->getOptionalStringValue("parent_journal_name");
    custom_node_params->harvest_url = custom_object->getOptionalStringValue("harvest_url");
    custom_node_params->physical_form = custom_object->getOptionalStringValue("physicalForm");
    custom_node_params->volume = custom_object->getOptionalStringValue("volume");
    custom_node_params->license = custom_object->getOptionalStringValue("licenseCode");
    custom_node_params->ssg_numbers = custom_object->getOptionalStringValue("ssgNumbers");
    custom_node_params->date_normalized = custom_object->getOptionalStringValue("date_normalized");
    custom_node_params->journal_ppn = custom_object->getOptionalStringValue("ppn");
    custom_node_params->isil = custom_object->getOptionalStringValue("isil");
}


std::string GetCustomValueIfNotEmpty(const std::string &custom_value, const std::string item_value) {
    return (not custom_value.empty()) ? custom_value : item_value;
}


void MarcFormatHandler::MergeCustomParametersToItemParameters(struct ItemParameters * const item_parameters, struct CustomNodeParameters &custom_node_params){
    item_parameters->issn = GetCustomValueIfNotEmpty(custom_node_params.issn_normalized, item_parameters->issn);
    item_parameters->parent_journal_name = GetCustomValueIfNotEmpty(item_parameters->parent_journal_name , item_parameters->parent_journal_name);
    item_parameters->harvest_url = GetCustomValueIfNotEmpty(custom_node_params.harvest_url, item_parameters->harvest_url);
    item_parameters->physical_form =GetCustomValueIfNotEmpty(custom_node_params.physical_form, item_parameters->physical_form);
    item_parameters->license = GetCustomValueIfNotEmpty(custom_node_params.license, item_parameters->license);
    item_parameters->superior_ppn = GetCustomValueIfNotEmpty(custom_node_params.journal_ppn, item_parameters->superior_ppn);
    item_parameters->ssg_numbers.emplace_back(custom_node_params.ssg_numbers);
    // Use the custom creator version if present since it may contain additional information such as a PPN
    if (custom_node_params.creators.size())
        item_parameters->creators = custom_node_params.creators;
    item_parameters->date = GetCustomValueIfNotEmpty(custom_node_params.date_normalized, item_parameters->date);
    item_parameters->isil = GetCustomValueIfNotEmpty(custom_node_params.isil, item_parameters->isil);
    if (item_parameters->year.empty() and not custom_node_params.date_normalized.empty()) {
        unsigned year;
        if (TimeUtil::StringToYear(custom_node_params.date_normalized, &year))
            item_parameters->year = StringUtil::ToString(year);
    }
}


void MarcFormatHandler::HandleTrackingAndWriteRecord(const MARC::Record &new_record, BSZUpload::DeliveryMode delivery_mode,
                                                     struct ItemParameters &item_params, unsigned * const previously_downloaded_count) {

    std::string url(item_params.url);
    const std::string parent_journal_name(item_params.parent_journal_name);
    const std::string harvest_url(item_params.harvest_url);
    const std::string checksum(StringUtil::ToHexString(MARC::CalcChecksum(new_record)));
    if (unlikely(url.empty())) {
        if (not harvest_url.empty())
            url = harvest_url;
        else
            LOG_ERROR("\"url\" has not been set!");
    }

    // only track downloads when the delivery mode is set to TEST or LIVE
    if (delivery_mode == BSZUpload::DeliveryMode::NONE)
        marc_writer_->write(new_record);
    else {
        DownloadTracker::Entry tracked_entry;
        if (not download_tracker_.hasAlreadyBeenDownloaded(delivery_mode, url, checksum, &tracked_entry) or
            not tracked_entry.error_message_.empty())
        {
            marc_writer_->write(new_record);
            download_tracker_.addOrReplace(delivery_mode, url, parent_journal_name, checksum, /* error_message = */"");
        } else
            ++(*previously_downloaded_count);
    }
}


std::pair<unsigned, unsigned> MarcFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    unsigned previously_downloaded_count(0);
    BSZUpload::DeliveryMode delivery_mode(MarcFormatHandler::getDeliveryMode());

    std::shared_ptr<const JSON::JSONNode> custom_node(object_node->getNode("ubtue"));
    CustomNodeParameters custom_node_params;
    if (custom_node != nullptr)
        ExtractCustomNodeParameters(custom_node, &custom_node_params);

    struct ItemParameters item_parameters;
    ExtractItemParameters(object_node, &item_parameters);
    MergeCustomParametersToItemParameters(&item_parameters, custom_node_params);

    MARC::Record new_record(std::string(MARC::Record::LEADER_LENGTH, ' ') /*empty dummy leader*/);
    GenerateMarcRecord(&new_record, item_parameters);

    HandleTrackingAndWriteRecord(new_record, delivery_mode, item_parameters, &previously_downloaded_count);
    return std::make_pair(/* record count */1, previously_downloaded_count);
}


// If "key" is in "map", then return the mapped value, o/w return "key".
inline std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map) {
    const auto &key_and_value(map.find(key));
    return (key_and_value == map.cend()) ? key : key_and_value->second;
}


void AugmentJsonCreators(const std::shared_ptr<JSON::ArrayNode> creators_array, const SiteParams &site_params,
                         std::vector<std::string> * const comments)
{
    for (size_t i(0); i < creators_array->size(); ++i) {
        const std::shared_ptr<JSON::ObjectNode> creator_object(creators_array->getObjectNode(i));

        const std::shared_ptr<const JSON::JSONNode> last_name_node(creator_object->getNode("lastName"));
        if (last_name_node != nullptr) {
            std::string name(creator_object->getStringValue("lastName"));

            const std::shared_ptr<const JSON::JSONNode> first_name_node(creator_object->getNode("firstName"));
            if (first_name_node != nullptr)
                name += ", " + creator_object->getStringValue("firstName");

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
    }
}


/* Improve JSON result delivered by Zotero Translation Server
 * Note on ISSN's: Some pages might contain multiple ISSN's (for each publication medium and/or a linking ISSN).
 *                In such cases, the Zotero translator must return tags to distinguish between them.
 */
void AugmentJson(const std::string &harvest_url, const std::shared_ptr<JSON::ObjectNode> &object_node, const SiteParams &site_params) {
    LOG_DEBUG("Augmenting JSON...");
    std::map<std::string, std::string> custom_fields;
    std::vector<std::string> comments;
    std::string issn_raw, issn_normalized, parent_ppn;
    std::shared_ptr<JSON::StringNode> language_node(nullptr);
    Transformation::TestForUnknownZoteroKey(object_node);

    for (auto &key_and_node : *object_node) {
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
            if (not site_params.parent_ISSN_online_.empty() or
                not site_params.parent_ISSN_print_.empty()) {
                continue;   // we'll just use the override
            }
            issn_raw = JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue();
            if (unlikely(not MiscUtil::NormaliseISSN(issn_raw, &issn_normalized))) {
                // the raw ISSN string probably contains multiple ISSN's that can't be distinguished
                throw std::runtime_error("\"" + issn_raw + "\" is invalid (multiple ISSN's?)!");
            } else
                custom_fields.emplace(std::pair<std::string, std::string>("ISSN_untagged", issn_normalized));
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
        }
    }

    // use ISSN specified in the config file if any
    if (not site_params.parent_ISSN_online_.empty()) {
        issn_normalized = site_params.parent_ISSN_online_;
        custom_fields.emplace(std::pair<std::string, std::string>("ISSN_online", issn_normalized));
        LOG_DEBUG("Using default online ISSN \"" + issn_normalized + "\"");
    } else if (not site_params.parent_ISSN_print_.empty()) {
        issn_normalized = site_params.parent_ISSN_print_;
        custom_fields.emplace(std::pair<std::string, std::string>("ISSN_print", issn_normalized));
        LOG_DEBUG("Using default print ISSN \"" + issn_normalized + "\"");
    }

    if (not site_params.parent_PPN_online_.empty()) {
        parent_ppn = site_params.parent_PPN_online_;
        custom_fields.emplace(std::pair<std::string, std::string>("ppn", parent_ppn));
        LOG_DEBUG("Using default online PPN \"" + parent_ppn + "\"");
    } else if (not site_params.parent_PPN_print_.empty()) {
        parent_ppn = site_params.parent_PPN_print_;
        custom_fields.emplace(std::pair<std::string, std::string>("ppn", parent_ppn));
        LOG_DEBUG("Using default print PPN \"" + parent_ppn + "\"");
    }

    // ISSN specific overrides
    if (not issn_normalized.empty()) {

        // physical form
        const auto ISSN_and_physical_form(site_params.global_params_->maps_->ISSN_to_physical_form_map_.find(issn_normalized));
        if (ISSN_and_physical_form != site_params.global_params_->maps_->ISSN_to_physical_form_map_.cend()) {
            if (ISSN_and_physical_form->second == "A")
                custom_fields.emplace(std::pair<std::string, std::string>("physicalForm", "A"));
            else if (ISSN_and_physical_form->second == "O")
                custom_fields.emplace(std::pair<std::string, std::string>("physicalForm", "O"));
            else
                LOG_ERROR("unhandled entry in physical form map: \"" + ISSN_and_physical_form->second + "\"!");
        }

        // language
        const auto ISSN_and_language(site_params.global_params_->maps_->ISSN_to_language_code_map_.find(issn_normalized));
        if (ISSN_and_language != site_params.global_params_->maps_->ISSN_to_language_code_map_.cend()) {
            if (language_node != nullptr) {
                const std::string language_old(language_node->getValue());
                language_node->setValue(ISSN_and_language->second);
                comments.emplace_back("changed \"language\" from \"" + language_old + "\" to \"" + ISSN_and_language->second
                                      + "\" due to ISSN map");
            } else {
                language_node = std::make_shared<JSON::StringNode>(ISSN_and_language->second);
                object_node->insert("language", language_node);
                comments.emplace_back("added \"language\" \"" + ISSN_and_language->second + "\" due to ISSN map");
            }
        }

        // volume
        const std::string volume(object_node->getOptionalStringValue("volume"));
        if (volume.empty()) {
            const auto ISSN_and_volume(site_params.global_params_->maps_->ISSN_to_volume_map_.find(issn_normalized));
            if (ISSN_and_volume != site_params.global_params_->maps_->ISSN_to_volume_map_.cend()) {
                if (volume.empty()) {
                    const std::shared_ptr<JSON::JSONNode> volume_node(object_node->getNode("volume"));
                    JSON::JSONNode::CastToStringNodeOrDie("volume", volume_node)->setValue(ISSN_and_volume->second);
                } else {
                    std::shared_ptr<JSON::StringNode> volume_node(new JSON::StringNode(ISSN_and_volume->second));
                    object_node->insert("volume", volume_node);
                }
            }
        }

        // license code
        const auto ISSN_and_license_code(site_params.global_params_->maps_->ISSN_to_licence_map_.find(issn_normalized));
        if (ISSN_and_license_code != site_params.global_params_->maps_->ISSN_to_licence_map_.end()) {
            if (ISSN_and_license_code->second != "l")
                LOG_ERROR("ISSN_to_licence.map contains an ISSN that has not been mapped to an \"l\" but \""
                          + ISSN_and_license_code->second
                          + "\" instead and we don't know what to do with it!");
            else
                custom_fields.emplace(std::pair<std::string, std::string>("licenseCode", ISSN_and_license_code->second));
        }

        // SSG numbers:
        const auto ISSN_and_SSGN_numbers(site_params.global_params_->maps_->ISSN_to_SSG_map_.find(issn_normalized));
        if (ISSN_and_SSGN_numbers != site_params.global_params_->maps_->ISSN_to_SSG_map_.end())
            custom_fields.emplace(std::pair<std::string, std::string>("ssgNumbers", ISSN_and_SSGN_numbers->second));


    } else
        LOG_WARNING("No suitable ISSN was found!");

    // Add the parent journal name for tracking changes to the harvested URL
    custom_fields.emplace(std::make_pair("parent_journal_name", site_params.parent_journal_name_));
    // save harvest URL in case of a faulty translator that doesn't correctly retrieve it
    custom_fields.emplace(std::make_pair("harvest_url", harvest_url));
    // save delivery mode for URL download tracking
    const auto delivery_mode_buffer(site_params.delivery_mode_); // can't capture the member variable directly in the lambda when using C++11
    const auto delivery_mode_string(std::find_if(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.begin(), BSZUpload::STRING_TO_DELIVERY_MODE_MAP.end(),
                                           [delivery_mode_buffer](const std::pair<std::string, int> &entry) -> bool { return static_cast<int>
                                           (delivery_mode_buffer) == entry.second; })->first);
    custom_fields.emplace(std::make_pair("delivery_mode", delivery_mode_string));

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
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(map_directory_path + "targets.regex"));

    std::string combined_regex;
    while (not input->eof()) {
        std::string line(input->getline());
        StringUtil::Trim(&line);
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


std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> harvest_params,
                                      const SiteParams &site_params, HarvesterErrorLogger * const error_logger, bool verbose)
{
    if (harvest_url.empty())
        LOG_ERROR("empty URL passed to Zotero::Harvest");

    std::pair<unsigned, unsigned> record_count_and_previously_downloaded_count;
    static std::unordered_set<std::string> already_harvested_urls;
    if (already_harvested_urls.find(harvest_url) != already_harvested_urls.end()) {
        LOG_DEBUG("Skipping URL (already harvested): " + harvest_url);
        return record_count_and_previously_downloaded_count;
    } else if (site_params.extraction_regex_ and not site_params.extraction_regex_->matched(harvest_url)) {
        LOG_DEBUG("Skipping URL ('" + harvest_url + "' does not match extraction regex)");
        return record_count_and_previously_downloaded_count;
    }

    already_harvested_urls.emplace(harvest_url);
    auto error_logger_context(error_logger->newContext(site_params.parent_journal_name_, harvest_url));

    LOG_INFO("Harvesting URL: " + harvest_url);

    std::string response_body, error_message;
    unsigned response_code;
    harvest_params->min_url_processing_time_.sleepUntilExpired();
    Downloader::Params downloader_params;
    downloader_params.user_agent_ = harvest_params->user_agent_;
    bool download_succeeded(TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ DEFAULT_TIMEOUT,
                                                   downloader_params, Url(harvest_url), &response_body, &response_code,
                                                   &error_message));

    harvest_params->min_url_processing_time_.restart();
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
            AugmentJson(harvest_url, json_object, site_params);
            record_count_and_previously_downloaded_count = harvest_params->format_handler_->processRecord(json_object);
        } catch (const std::exception &x) {
            error_logger_context.autoLog("Couldn't process record! Error: " + std::string(x.what()));
            return record_count_and_previously_downloaded_count;
        }
    }

    if (processed_json_entries == 0)
        error_logger_context.log(HarvesterErrorLogger::ZTS_EMPTY_RESPONSE, "Response code = " + std::to_string(response_code));

    ++harvest_params->harvested_url_count_;

    if (verbose) {
        LOG_DEBUG("Harvested " + StringUtil::ToString(record_count_and_previously_downloaded_count.first) + " record(s) from "
                  + harvest_url + '\n' + "of which "
                  + StringUtil::ToString(record_count_and_previously_downloaded_count.first
                                        - record_count_and_previously_downloaded_count.second)
                  + " records were new records.");
    }
    return record_count_and_previously_downloaded_count;
}


UnsignedPair HarvestSite(const SimpleCrawler::SiteDesc &site_desc, const SimpleCrawler::Params &crawler_params,
                         const std::shared_ptr<RegexMatcher> &supported_urls_regex,
                         const std::shared_ptr<HarvestParams> &harvest_params, const SiteParams &site_params,
                         HarvesterErrorLogger * const error_logger, File * const progress_file)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;
    LOG_DEBUG("Starting crawl at base URL: " +  site_desc.start_url_);
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


namespace {


// Returns true if we can determine that the last_build_date column value stored in the rss_feeds table for the feed identified
// by "feed_url" is no older than the "last_build_date" time_t passed into this function.  (This is somewhat complicated by the
// fact that both, the column value as well as the time_t value may contain indeterminate values.)
bool FeedContainsNoNewItems(const RSSHarvestMode mode, DbConnection * const db_connection, const std::string &feed_url,
                            const time_t last_build_date)
{
    db_connection->queryOrDie("SELECT last_build_date FROM rss_feeds WHERE feed_url='" + db_connection->escapeString(feed_url)
                              + "'");
    DbResultSet result_set(db_connection->getLastResultSet());

    std::string date_string;
    if (result_set.empty()) {
        if (last_build_date == TimeUtil::BAD_TIME_T)
            date_string = SqlUtil::DATETIME_RANGE_MIN;
        else
            date_string = SqlUtil::TimeTToDatetime(last_build_date);

        if (mode == RSSHarvestMode::VERBOSE)
            LOG_DEBUG("Creating new feed entry in rss_feeds table for \"" + feed_url + "\".");
        if (mode != RSSHarvestMode::TEST)
            db_connection->queryOrDie("INSERT INTO rss_feeds SET feed_url='" + db_connection->escapeString(feed_url)
                                      + "',last_build_date='" + date_string + "'");
        return false;
    }

    const DbRow first_row(result_set.getNextRow());
    date_string = first_row["last_build_date"];
    if (date_string != SqlUtil::DATETIME_RANGE_MIN and last_build_date != TimeUtil::BAD_TIME_T
        and SqlUtil::DatetimeToTimeT(date_string) >= last_build_date)
        return true;

    return false;
}


// Returns the feed ID for the URL "feed_url".
std::string GetFeedID(const RSSHarvestMode mode, DbConnection * const db_connection, const std::string &feed_url) {
    db_connection->queryOrDie("SELECT id FROM rss_feeds WHERE feed_url='" + db_connection->escapeString(feed_url)
                              + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (unlikely(result_set.empty())) {
        if (mode == RSSHarvestMode::TEST)
            return "-1"; // Must be an INT.
        LOG_ERROR("unexpected missing feed for URL \"" + feed_url + "\".");
    }
    const DbRow first_row(result_set.getNextRow());
    return first_row["id"];
}


// Returns true if the item with item ID "item_id" and feed ID "feed_id" were found in the rss_items table, else
// returns false.
bool ItemAlreadyProcessed(DbConnection * const db_connection, const std::string &feed_id, const std::string &item_id) {
    db_connection->queryOrDie("SELECT creation_datetime FROM rss_items WHERE feed_id='"
                              + feed_id + "' AND item_id='" + db_connection->escapeString(item_id) + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return false;

    if (logger->getMinimumLogLevel() >= Logger::LL_DEBUG) {
        const DbRow first_row(result_set.getNextRow());
        LOG_DEBUG("Previously retrieved item w/ ID \"" + item_id + "\" at " + first_row["creation_datetime"] + ".");
    }

    return true;
}


void UpdateLastBuildDate(DbConnection * const db_connection, const std::string &feed_url, const time_t last_build_date) {
    std::string last_build_date_string;
    if (last_build_date == TimeUtil::BAD_TIME_T)
        last_build_date_string = SqlUtil::DATETIME_RANGE_MIN;
    else
        last_build_date_string = SqlUtil::TimeTToDatetime(last_build_date);
    db_connection->queryOrDie("UPDATE rss_feeds SET last_build_date='" + last_build_date_string + "' WHERE feed_url='"
                              + db_connection->escapeString(feed_url) + "'");
}


} // unnamed namespace


UnsignedPair HarvestSyndicationURL(const RSSHarvestMode mode, const std::string &feed_url,
                                   const std::shared_ptr<HarvestParams> &harvest_params,
                                   const SiteParams &site_params, HarvesterErrorLogger * const error_logger,
                                   DbConnection * const db_connection)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;
    auto error_logger_context(error_logger->newContext(site_params.parent_journal_name_, feed_url));

    if (mode != RSSHarvestMode::NORMAL)
        LOG_INFO("Processing URL: " + feed_url);

    Downloader::Params downloader_params;
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

    const time_t last_build_date(syndication_format->getLastBuildDate());
    if (mode == RSSHarvestMode::VERBOSE) {
        LOG_DEBUG(feed_url + " (" + syndication_format->getFormatName() + "):");
        LOG_DEBUG("\tTitle: " + syndication_format->getTitle());
        if (last_build_date != TimeUtil::BAD_TIME_T)
            LOG_DEBUG("\tLast build date: " + TimeUtil::TimeTToUtcString(last_build_date));
        LOG_DEBUG("\tLink: " + syndication_format->getLink());
        LOG_DEBUG("\tDescription: " + syndication_format->getDescription());
   }

    if (mode != RSSHarvestMode::TEST and FeedContainsNoNewItems(mode, db_connection, feed_url, last_build_date))
        return total_record_count_and_previously_downloaded_record_count;

    const std::string feed_id(mode == RSSHarvestMode::TEST ? "" : GetFeedID(mode, db_connection, feed_url));
    for (const auto &item : *syndication_format) {
        if (mode != RSSHarvestMode::TEST and ItemAlreadyProcessed(db_connection, feed_id, item.getId()))
            continue;

        const std::string title(item.getTitle());
        if (not title.empty() and mode == RSSHarvestMode::VERBOSE)
            LOG_DEBUG("\t\tTitle: " + title);

        const auto record_count_and_previously_downloaded_count(
            Harvest(item.getLink(), harvest_params, site_params, error_logger, /* verbose = */ mode != RSSHarvestMode::NORMAL));
        total_record_count_and_previously_downloaded_record_count += record_count_and_previously_downloaded_count;

        if (mode != RSSHarvestMode::TEST)
            db_connection->queryOrDie("INSERT INTO rss_items SET feed_id='" + feed_id + "',item_id='"
                                      + db_connection->escapeString(item.getId()) + "'");

    }
    if (mode != RSSHarvestMode::TEST)
        UpdateLastBuildDate(db_connection, feed_url, last_build_date);

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
    static const std::unordered_map<ErrorType, RegexMatcher *> error_regexp_map{
        { BAD_STRPTIME_FORMAT,      RegexMatcher::RegexMatcherFactoryOrDie("StringToStructTm\\: don't know how to convert \\\"(.+?)\\\"") },
    };

    HarvesterError error{ UNKNOWN, "" };
    for (const auto &error_regexp : error_regexp_map) {
        if (error_regexp.second->matched(message)) {
            error.type = error_regexp.first;
            error.message = (*error_regexp.second)[1];
            break;
        }
    }

    log(error.type, journal_name, harvest_url, error.type == UNKNOWN ? message : error.message, write_to_std_error);
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
            const auto error_string(ERROR_KIND_TO_STRING_MAP.at(url_error.second.type));
            // we cannot cache the section pointer as it can get invalidated after appending a new section
            report.getSection(journal_name)->insert(url_error.first, error_string);
            report.appendSection(error_string);
            report.getSection(error_string)->insert(url_error.first, url_error.second.message);
        }

        int i(1);
        for (const auto &non_url_error : journal_error.second.non_url_errors_) {
            const auto error_string(ERROR_KIND_TO_STRING_MAP.at(non_url_error.type));
            const auto error_key(journal_name + "-non_url_error-" + std::to_string(i));

            report.getSection(journal_name)->insert(error_key, error_string);
            report.appendSection(error_string);
            report.getSection(error_string)->insert(error_key, non_url_error.message);
            ++i;
        }
    }

    report.getSection("")->insert("journal_names", journal_names);
    report.write(report_file_path);
}


} // namespace Zotero
