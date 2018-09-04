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
#include "DbConnection.h"
#include "MiscUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "SyndicationFormat.h"
#include "TimeUtil.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "util.h"


// Forward declaration:
class File;


namespace Zotero {


const std::map<HarvesterType, std::string> HARVESTER_TYPE_TO_STRING_MAP{
    { HarvesterType::RSS, "RSS" },
    { HarvesterType::CRAWL, "CRAWL" },
    { HarvesterType::DIRECT, "DIRECT" }
};
const std::map<HarvesterConfigEntry, std::string> HARVESTER_CONFIG_ENTRY_TO_STRING_MAP{
    { HarvesterConfigEntry::TYPE, "type" },
    { HarvesterConfigEntry::GROUP, "group" },
    { HarvesterConfigEntry::PARENT_PPN, "parent_ppn" },
    { HarvesterConfigEntry::PARENT_ISSN_PRINT, "parent_issn_print" },
    { HarvesterConfigEntry::PARENT_ISSN_ONLINE, "parent_issn_online" },
    { HarvesterConfigEntry::STRPTIME_FORMAT, "strptime_format" },
    { HarvesterConfigEntry::FEED, "feed" },
    { HarvesterConfigEntry::URL, "url" },
    { HarvesterConfigEntry::BASE_URL, "base_url" },
    { HarvesterConfigEntry::EXTRACTION_REGEX, "extraction_regex" },
    { HarvesterConfigEntry::MAX_CRAWL_DEPTH, "max_crawl_depth" }
};


const std::string DEFAULT_SIMPLE_CRAWLER_CONFIG_PATH("/usr/local/var/lib/tuelib/zotero_crawler.conf");
const std::string ISSN_TO_MISC_BITS_MAP_PATH_LOCAL("/usr/local/var/lib/tuelib/issn_to_misc_bits.map");
const std::string ISSN_TO_MISC_BITS_MAP_DIR_REMOTE("/mnt/ZE020150/FID-Entwicklung/issn_to_misc_bits");


const std::vector<std::string> EXPORT_FORMATS{
    "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei",
    "json", "marc21", "marcxml"
};


// Zotero values see https://raw.githubusercontent.com/zotero/zotero/master/test/tests/data/allTypesAndFields.js
// MARC21 values see https://www.loc.gov/marc/relators/relaterm.html
const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP{
    { "artist",             "art" },
    { "attorneyAgent",      "csl" },
    { "author",             "aut" },
    { "bookAuthor",         "edc" },
    { "cartographer",       "ctg" },
    { "castMember",         "act" },
    { "commenter",          "cwt" },
    { "composer",           "cmp" },
    { "contributor",        "ctb" },
    { "cosponsor",          "spn" },
    { "director",           "drt" },
    { "editor",             "edt" },
    { "guest",              "pan" },
    { "interviewee",        "ive" },
    { "inventor",           "inv" },
    { "performer",          "prf" },
    { "podcaster",          "brd" },
    { "presenter",          "pre" },
    { "producer",           "pro" },
    { "programmer",         "prg" },
    { "recipient",          "rcp" },
    { "reviewedAuthor",     "aut" },
    { "scriptwriter",       "aus" },
    { "seriesEditor",       "edt" },
    { "sponsor",            "spn" },
    { "translator",         "trl" },
    { "wordsBy",            "wam" },
};


const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type) {
    const auto creator_type_zotero_and_marc21(CREATOR_TYPES_TO_MARC21_MAP.find(zotero_creator_type));
    if (creator_type_zotero_and_marc21 == CREATOR_TYPES_TO_MARC21_MAP.end())
        LOG_ERROR("Zotero creatorType could not be mapped to MARC21: \"" + zotero_creator_type + "\"");
    return creator_type_zotero_and_marc21->second;
}


const std::map<std::string, MARC::Record::BibliographicLevel> ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP{
    { "book", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "bookSection", MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART },
    { "document", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "journalArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "magazineArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "newspaperArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "webpage", MARC::Record::BibliographicLevel::INTEGRATING_RESOURCE }
};


const std::string DEFAULT_SUBFIELD_CODE("eng");


namespace TranslationServer {


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
         std::string * const error_message, const std::string &/*harvested_html*/)
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


void LoadGroup(const IniFile::Section &section, std::map<std::string, GroupParams> * const group_name_to_params_map) {
    GroupParams new_group_params;
    new_group_params.name_              = section.getSectionName();
    new_group_params.user_agent_        = section.getString("user_agent");
    new_group_params.isil_              = section.getString("isil");
    new_group_params.author_lookup_url_ = section.getString("author_lookup_url");
    group_name_to_params_map->emplace(section.getSectionName(), new_group_params);
}


std::unique_ptr<FormatHandler> FormatHandler::Factory(DbConnection * const db_connection, const std::string &output_format,
                                                      const std::string &output_file,
                                                      const std::shared_ptr<const HarvestParams> &harvest_params)
{
    if (output_format == "marcxml" or output_format == "marc21")
        return std::unique_ptr<FormatHandler>(new MarcFormatHandler(db_connection, output_file, harvest_params));
    else if (output_format == "json")
        return std::unique_ptr<FormatHandler>(new JsonFormatHandler(db_connection, output_format, output_file, harvest_params));
    else if (std::find(EXPORT_FORMATS.begin(), EXPORT_FORMATS.end(), output_format) != EXPORT_FORMATS.end())
        return std::unique_ptr<FormatHandler>(new ZoteroFormatHandler(db_connection, output_format, output_file,
                                                                      harvest_params));
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
        return "marc21";
    case MARC::FileType::XML:
        return "marcxml";
    default:
        LOG_ERROR("we should *never* get here!");
    }
}


MarcFormatHandler::MarcFormatHandler(DbConnection * const db_connection, const std::string &output_file,
                                     const std::shared_ptr<const HarvestParams> &harvest_params)
    : FormatHandler(db_connection, GuessOutputFormat(output_file), output_file, harvest_params),
      marc_writer_(MARC::Writer::Factory(output_file_))
{
}


void MarcFormatHandler::ExtractKeywords(std::shared_ptr<const JSON::JSONNode> tags_node, const std::string &issn,
                                        const std::unordered_map<std::string, std::string> &ISSN_to_keyword_field_map,
                                        MARC::Record * const new_record)
{
    const std::shared_ptr<const JSON::ArrayNode> tags(JSON::JSONNode::CastToArrayNodeOrDie("tags", tags_node));

    // Where to stuff the data:
    std::string marc_field("653");
    char marc_subfield('a');
    if (not issn.empty()) {
        const auto issn_and_field_tag_and_subfield_code(ISSN_to_keyword_field_map.find(issn));
        if (issn_and_field_tag_and_subfield_code != ISSN_to_keyword_field_map.end()) {
            if (unlikely(issn_and_field_tag_and_subfield_code->second.length() != 3 + 1))
                LOG_ERROR("\"" + issn_and_field_tag_and_subfield_code->second
                          + "\" is not a valid MARC tag + subfield code! (Error in \"ISSN_to_keyword_field.map\"!)");
            marc_field    = issn_and_field_tag_and_subfield_code->second.substr(0, 3);
            marc_subfield =  issn_and_field_tag_and_subfield_code->second[3];
        }
    }

    for (const auto &tag : *tags) {
        const std::shared_ptr<const JSON::ObjectNode> tag_object(JSON::JSONNode::CastToObjectNodeOrDie("tag", tag));
        const std::shared_ptr<const JSON::JSONNode> tag_node(tag_object->getNode("tag"));
        if (tag_node == nullptr)
            LOG_ERROR("unexpected: tag object does not contain a \"tag\" entry!");
        else if (tag_node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("unexpected: tag object's \"tag\" entry is not a string node!");
        else
            CreateSubfieldFromStringNode("tag", tag_node, marc_field, marc_subfield, new_record);
    }
}


void MarcFormatHandler::ExtractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node, MARC::Record * const new_record) {
    std::vector<MARC::Subfield> subfields;

    std::shared_ptr<const JSON::JSONNode> custom_node(object_node.getNode("ubtue"));
    if (custom_node != nullptr) {
        const std::shared_ptr<const JSON::ObjectNode>custom_object(JSON::JSONNode::CastToObjectNodeOrDie("ubtue", custom_node));
        std::string date_str(custom_object->getOptionalStringValue("date_normalized"));
        if (date_str.empty())
            date_str = custom_object->getOptionalStringValue("date_raw");

        const std::string STRPTIME_FORMAT(site_params_->strptime_format_.empty() ? "%Y-%m-%d" : site_params_->strptime_format_);
        const struct tm tm(TimeUtil::StringToStructTm(date_str, STRPTIME_FORMAT));
        subfields.emplace_back('j', std::to_string(tm.tm_year + 1900));
    }

    const std::string issue(object_node.getOptionalStringValue("issue"));
    if (not issue.empty())
        subfields.emplace_back('e', issue);

    const std::string pages(object_node.getOptionalStringValue("pages"));
    if (not pages.empty())
        subfields.emplace_back('h', pages);

    const std::string volume(object_node.getOptionalStringValue("volume"));
    if (not volume.empty())
        subfields.emplace_back('d', volume);

    if (not subfields.empty())
        new_record->insertField("936", subfields);
}


void MarcFormatHandler::CreateCreatorFields(const std::shared_ptr<const JSON::JSONNode> creators_node, MARC::Record * const marc_record) {
    const std::shared_ptr<const JSON::ArrayNode> creators_array(JSON::JSONNode::CastToArrayNodeOrDie("creators", creators_node));

    // only use 100 if we have exactly 1 creator, else it is impossible to say which is the most important one
    std::string tag(creators_array->size() > 1 ? "700" : "100");
    for (const auto &creator_node : *creators_array) {
        const std::shared_ptr<const JSON::ObjectNode> creator_object(JSON::JSONNode::CastToObjectNodeOrDie("creator",
                                                                                                           creator_node));
        MARC::Subfields subfields;

        const std::shared_ptr<const JSON::JSONNode> last_name_node(creator_object->getNode("lastName"));
        if (last_name_node == nullptr)
            throw std::runtime_error("MarcFormatHandler::CreateCreatorFields: screator is missing a last name!");
        const std::shared_ptr<const JSON::StringNode> last_name(JSON::JSONNode::CastToStringNodeOrDie("lastName",
                                                                                                      last_name_node));
        std::string name(last_name->getValue());
        const std::shared_ptr<const JSON::JSONNode> first_name_node(creator_object->getNode("firstName"));
        if (first_name_node != nullptr) {
            const std::shared_ptr<const JSON::StringNode> first_name(JSON::JSONNode::CastToStringNodeOrDie("firstName",
                                                                                                           first_name_node));
            name += ", " + first_name->getValue();
        }
        subfields.addSubfield('a', name);

        const std::shared_ptr<const JSON::JSONNode> ppn_node(creator_object->getNode("ppn"));
        if (ppn_node != nullptr) {
            const std::shared_ptr<const JSON::StringNode> ppn_string_node(JSON::JSONNode::CastToStringNodeOrDie("ppn",
                                                                                                                ppn_node));
            subfields.addSubfield('0', "(DE-576)" + ppn_string_node->getValue());
        }

        const std::shared_ptr<const JSON::JSONNode> creator_type(creator_object->getNode("creatorType"));
        std::string creator_role;
        if (creator_type != nullptr) {
            const std::shared_ptr<const JSON::StringNode> creator_role_node(JSON::JSONNode::CastToStringNodeOrDie("creatorType",
                                                                                                                  creator_type));
            subfields.addSubfield('4', GetCreatorTypeForMarc21(creator_role_node->getValue()));
        }

        marc_record->insertField(tag, subfields);
    }
}


void InsertDOI(MARC::Record * const record, const std::string &doi) {
    record->insertField("024", { { 'a', doi }, { '2', "doi" } });
}


MARC::Record MarcFormatHandler::processJSON(const std::shared_ptr<const JSON::ObjectNode> &object_node, std::string * const url,
                                            std::string * const publication_title, std::string * const abbreviated_publication_title,
                                            std::string * const website_title)
{
    const std::string item_type(object_node->getStringValue("itemType"));
    const auto bibliographic_level_iterator(ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.find(item_type));
    if (bibliographic_level_iterator == ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.end())
        LOG_ERROR("No bibliographic level mapping entry available for Zotero item type: " + item_type);

    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, bibliographic_level_iterator->second);

    if (item_type == "journalArticle") {
        *publication_title = object_node->getOptionalStringValue("publicationTitle");
        *abbreviated_publication_title = object_node->getOptionalStringValue("journalAbbreviation");
        ExtractVolumeYearIssueAndPages(*object_node, &new_record);
    } else if (item_type == "magazineArticle" or item_type == "newspaperArticle") {
        *publication_title = object_node->getOptionalStringValue("publicationTitle");
        ExtractVolumeYearIssueAndPages(*object_node, &new_record);
    } else if (item_type == "webpage") {
        // just add the encoding marker
        new_record.insertField("935", { { 'c', "website" } });
    } else
        LOG_ERROR("unknown item type: \"" + item_type + "\"! JSON node dump: " + object_node->toString());

    static RegexMatcher * const ignore_fields(RegexMatcher::RegexMatcherFactory(
        "^itemType|issue|journalAbbreviation|pages|publicationTitle|volume|version|date|tags|libraryCatalog|itemVersion|accessDate|key|websiteType|ISSN|ubtue$"));
    for (const auto &key_and_node : *object_node) {
        if (ignore_fields->matched(key_and_node.first))
            continue;

        if (key_and_node.first == "language")
            new_record.insertField("041", { { 'a',
                JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue() } });
        else if (key_and_node.first == "url") {
            CreateSubfieldFromStringNode(key_and_node, "856", 'u', &new_record);
            *url = reinterpret_cast<const JSON::StringNode * const>(key_and_node.second.get())->getValue();
        } else if (key_and_node.first == "title")
            CreateSubfieldFromStringNode(key_and_node, "245", 'a', &new_record);
        else if (key_and_node.first == "abstractNote")
            CreateSubfieldFromStringNode(key_and_node, "520", 'a', &new_record, /* indicator1 = */ '3');
        else if (key_and_node.first == "date")
            CreateSubfieldFromStringNode(key_and_node, "362", 'a', &new_record, /* indicator1 = */ '0');
        else if (key_and_node.first == "DOI") {
            if (unlikely(key_and_node.second->getType() != JSON::JSONNode::STRING_NODE))
                LOG_ERROR("expected DOI node to be a string node!");
            InsertDOI(&new_record, JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue());
        } else if (key_and_node.first == "shortTitle")
            CreateSubfieldFromStringNode(key_and_node, "246", 'a', &new_record);
        else if (key_and_node.first == "creators")
            CreateCreatorFields(key_and_node.second, &new_record);
        else if (key_and_node.first == "rights") {
            const std::string copyright(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first,
                                                                              key_and_node.second)->getValue());
            if (UrlUtil::IsValidWebUrl(copyright))
                new_record.insertField("542", { { 'u', copyright } });
            else
                new_record.insertField("542", { { 'f', copyright } });
        } else if (key_and_node.first == "extra") {
            // comment field, can contain anything, even DOI's (e.g. for a webpage which has no DOI field)
            const std::string extra(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first,
                                                                          key_and_node.second)->getValue());
            static RegexMatcher * const doi_matcher(RegexMatcher::RegexMatcherFactory("^DOI:\\s*([0-9a-zA-Z./]+)$"));
            if (doi_matcher->matched(extra))
                InsertDOI(&new_record, (*doi_matcher)[1]);
        } else if (key_and_node.first == "websiteTitle")
            *website_title = JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue();
        else
            LOG_WARNING("unknown key \"" + key_and_node.first + "\" with node type "
                      + JSON::JSONNode::TypeToString(key_and_node.second->getType()) + "! ("
                      + key_and_node.second->toString() + "), whole record: " + object_node->toString());
    }

    new_record.insertField("001", site_params_->group_params_->name_ + "#" + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d")
                           + "#" + StringUtil::ToHexString(MARC::CalcChecksum(new_record)));
    return new_record;
}


// Populates the "ubtue" node.
void MarcFormatHandler::extractCustomNodeParameters(std::shared_ptr<const JSON::JSONNode> custom_node, CustomNodeParameters * const
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
        LOG_WARNING("No ISSN found for article.");

    custom_node_params->parent_journal_name = custom_object->getOptionalStringValue("parent_journal_name");
    custom_node_params->harvest_url = custom_object->getOptionalStringValue("harvest_url");
    custom_node_params->physical_form = custom_object->getOptionalStringValue("physicalForm");
    custom_node_params->volume = custom_object->getOptionalStringValue("volume");
    custom_node_params->license = custom_object->getOptionalStringValue("licenseCode");
    custom_node_params->ssg_numbers = custom_object->getOptionalStringValue("ssgNumbers");
}


std::pair<unsigned, unsigned> MarcFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    std::string publication_title, abbreviated_publication_title, url, website_title;
    MARC::Record new_record(processJSON(object_node, &url, &publication_title, &abbreviated_publication_title, &website_title));

    unsigned previously_downloaded_count(0);
    BSZUpload::DeliveryMode delivery_mode(MarcFormatHandler::getDeliveryMode());

    std::shared_ptr<const JSON::JSONNode> custom_node(object_node->getNode("ubtue"));
    CustomNodeParameters custom_node_params;
    if (custom_node != nullptr)
        extractCustomNodeParameters(custom_node, &custom_node_params);

    std::string issn_normalized(custom_node_params.issn_normalized),
                parent_journal_name(custom_node_params.parent_journal_name),
                harvest_url(custom_node_params.harvest_url);

    // physical form
    const std::string physical_form(custom_node_params.physical_form);
    if (not physical_form.empty()) {
        if (physical_form == "A")
            new_record.insertField("007", "tu");
        else if (physical_form == "O")
            new_record.insertField("007", "cr uuu---uuuuu");
        else
            LOG_ERROR("unhandled value of physical form: \"" + physical_form + "\"!");
    }

    // volume
    const std::string volume(custom_node_params.volume);
    if (not volume.empty()) {
        auto field_it(new_record.findTag("936"));
        if (field_it == new_record.end())
            new_record.insertField("936", { { 'v', volume } });
        else
            field_it->getSubfields().addSubfield('v', volume);
    }

    // license code
    const std::string license(custom_node_params.license);
    if (license == "l") {
        auto field_it(new_record.findTag("936"));
        if (field_it != new_record.end())
            field_it->getSubfields().addSubfield('z', "Kostenfrei");
    }

    // SSG numbers:
    const std::string ssg_numbers(custom_node_params.ssg_numbers);
    if (not ssg_numbers.empty())
        new_record.addSubfield("084", 'a', ssg_numbers);


    // title:
    if (not website_title.empty() and not new_record.hasTag("245"))
        new_record.insertField("245", { { 'a', website_title } });


    // keywords:
    const std::shared_ptr<const JSON::JSONNode>tags_node(object_node->getNode("tags"));
    if (tags_node != nullptr)
        ExtractKeywords(tags_node, custom_node_params.issn_normalized, site_params_->global_params_->maps_->ISSN_to_keyword_field_map_, &new_record);

    // Populate 773:
    if (new_record.isArticle()) {
        const auto superior_ppn_and_title(site_params_->global_params_->maps_->ISSN_to_superior_ppn_and_title_map_.find(issn_normalized));
        if (superior_ppn_and_title != site_params_->global_params_->maps_->ISSN_to_superior_ppn_and_title_map_.end()) {
            std::vector<MARC::Subfield> subfields;
            if (publication_title.empty())
                publication_title = superior_ppn_and_title->second.getTitle();
            if (not publication_title.empty())
                subfields.emplace_back('a', publication_title);
            if (not abbreviated_publication_title.empty())
                subfields.emplace_back('p', abbreviated_publication_title);
            if (not issn_normalized.empty())
                subfields.emplace_back('x', issn_normalized);
            const std::string journal_ppn(superior_ppn_and_title->second.getPPN());
            if (not journal_ppn.empty())
                subfields.emplace_back('w', "(DE-576)" + journal_ppn);
            if (not subfields.empty())
                new_record.insertField("773", subfields);
        }
    }

    // 003 field => insert ISIL:
    new_record.insertField("003", site_params_->group_params_->isil_);

    // language code fallback:
    if (not new_record.hasTag("041"))
        new_record.insertField("041", { { 'a', DEFAULT_SUBFIELD_CODE } });

    // previously downloaded?
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
            ++previously_downloaded_count;
    }

    return std::make_pair(/* record count */1, previously_downloaded_count);
}


void LoadISSNToPPNMap(std::unordered_map<std::string, PPNandTitle> * const ISSN_to_superior_ppn_map) {
     enum ISSN_TO_PPN_OFFSET {ISSN_OFFSET = 0, PPN_OFFSET = 1, TITLE_OFFSET = 4};
     std::vector<std::vector<std::string>> parsed_issn_to_superior_content;
     TextUtil::ParseCSVFileOrDie(ISSN_TO_MISC_BITS_MAP_PATH_LOCAL, &parsed_issn_to_superior_content, ',', (char) 0x00);
     for (const auto parsed_line : parsed_issn_to_superior_content) {
         const std::string ISSN(parsed_line[ISSN_OFFSET]);
         const std::string PPN(parsed_line[PPN_OFFSET]);
         const std::string title(StringUtil::RightTrim(" \t", parsed_line[TITLE_OFFSET]));
         ISSN_to_superior_ppn_map->emplace(ISSN, PPNandTitle(PPN, title));
     }
}


AugmentMaps::AugmentMaps(const std::string &map_directory_path) {
    MiscUtil::LoadMapFile(map_directory_path + "language_to_language_code.map", &language_to_language_code_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_language_code.map", &ISSN_to_language_code_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_licence.map", &ISSN_to_licence_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_keyword_field.map", &ISSN_to_keyword_field_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_physical_form.map", &ISSN_to_physical_form_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_volume.map", &ISSN_to_volume_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_SSG.map", &ISSN_to_SSG_map_);
    LoadISSNToPPNMap(&ISSN_to_superior_ppn_and_title_map_);
}


// If "key" is in "map", then return the mapped value, o/w return "key".
inline std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map) {
    const auto &key_and_value(map.find(key));
    return (key_and_value == map.cend()) ? key : key_and_value->second;
}


// "author" must be in the lastname,firstname format. Returns the empty string if no PPN was found.
std::string DownloadAuthorPPN(const std::string &author, const SiteParams &site_params) {
    const std::string LOOKUP_URL(site_params.group_params_->author_lookup_url_ + UrlUtil::UrlEncode(author));

    static std::unordered_map<std::string, std::string> url_to_lookup_result_cache;
    const auto url_and_lookup_result(url_to_lookup_result_cache.find(LOOKUP_URL));
    if (url_and_lookup_result == url_to_lookup_result_cache.end()) {
        static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("<SMALL>PPN</SMALL>.*<div><SMALL>([0-9X]+)"));
        Downloader downloader(LOOKUP_URL);
        if (downloader.anErrorOccurred())
            LOG_ERROR(downloader.getLastErrorMessage());
        else if (matcher->matched(downloader.getMessageBody())) {
            url_to_lookup_result_cache.emplace(LOOKUP_URL, (*matcher)[1]);
            return (*matcher)[1];
        } else
            url_to_lookup_result_cache.emplace(LOOKUP_URL, "");
    } else
        return url_and_lookup_result->second;

    return "";
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

            const std::string PPN(DownloadAuthorPPN(name, site_params));
            if (not PPN.empty()) {
                comments->emplace_back("Added author PPN " + PPN + " for author " + name);
                creator_object->insert("ppn", std::make_shared<JSON::StringNode>(PPN));
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
    std::string issn_raw, issn_normalized;
    std::shared_ptr<JSON::StringNode> language_node(nullptr);
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
            struct tm tm(TimeUtil::StringToStructTm(date_raw, site_params.strptime_format_));
            std::string date_normalized(std::to_string(tm.tm_year + 1900) + "-" + StringUtil::ToString(tm.tm_mon + 1, 10, 2, '0') + "-"
                                        + StringUtil::ToString(tm.tm_mday, 10, 2, '0'));
            custom_fields.emplace(std::pair<std::string, std::string>("date_normalized", date_normalized));
            comments.emplace_back("normalized date to: " + date_normalized);
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

    // ISSN specific overrides
    if (not issn_normalized.empty()) {
        const auto ISSN_parent_ppn_and_title(
            site_params.global_params_->maps_->ISSN_to_superior_ppn_and_title_map_.find(issn_normalized));
        if (ISSN_parent_ppn_and_title != site_params.global_params_->maps_->ISSN_to_superior_ppn_and_title_map_.cend())
            custom_fields.emplace(std::pair<std::string, std::string>("PPN", ISSN_parent_ppn_and_title->second.getPPN()));

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


std::pair<unsigned, unsigned> Harvest(const std::string &harvest_url, const std::shared_ptr<HarvestParams> harvest_params,
                                      const SiteParams &site_params, const std::string &harvested_html, bool log)
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

    LOG_INFO("Harvesting URL: " + harvest_url);

    std::string response_body, error_message;
    unsigned response_code;
    harvest_params->min_url_processing_time_.sleepUntilExpired();
    Downloader::Params downloader_params;
    downloader_params.user_agent_ = harvest_params->user_agent_;
    bool download_succeeded(TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ DEFAULT_TIMEOUT,
                                                   downloader_params, Url(harvest_url), &response_body, &response_code,
                                                   &error_message, harvested_html));

    harvest_params->min_url_processing_time_.restart();
    if (not download_succeeded) {
        LOG_WARNING(site_params.parent_journal_name_ + "\t" + harvest_url + "\tZotero conversion failed: " + error_message);
        return std::make_pair(0, 0);
    }

    // 300 => multiple matches found, try to harvest children (send the response_body right back to the server, to get all of them)
    if (response_code == 300) {
        LOG_DEBUG("multiple articles found => trying to harvest children");
        download_succeeded = TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ DEFAULT_TIMEOUT,
                                                    downloader_params, response_body, &response_body, &response_code, &error_message);
        if (not download_succeeded) {
            LOG_WARNING(site_params.parent_journal_name_ + "\t" + harvest_url + "\tDownload multiple results failed: " + error_message);
            return std::make_pair(0, 0);
        }
    }

    // Process either single or multiple results (response_body is array by now)
    std::shared_ptr<JSON::JSONNode> tree_root(nullptr);
    JSON::Parser json_parser(response_body);
    if (not (json_parser.parse(&tree_root)))
        LOG_ERROR(site_params.parent_journal_name_ + "\t" + harvest_url + "\tfailed to parse returned JSON: " + json_parser.getErrorMessage() + "\n" + response_body);

    const std::shared_ptr<const JSON::ArrayNode>json_array(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));
    int processed_json_entries(0);
    for (const auto entry : *json_array) {
        const std::shared_ptr<JSON::ObjectNode> json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        ++processed_json_entries;

        try {
            AugmentJson(harvest_url, json_object, site_params);
            record_count_and_previously_downloaded_count = harvest_params->format_handler_->processRecord(json_object);
        } catch (const std::exception &x) {
            LOG_WARNING(site_params.parent_journal_name_ + "\t" + harvest_url + "\tCouldn't process record! Error: "
                        + std::string(x.what()));
            return record_count_and_previously_downloaded_count;
        }
    }

    if (processed_json_entries == 0)
        LOG_WARNING(site_params.parent_journal_name_ + "\t" + harvest_url + "\tZotero translation server returned an empty response!"                  "Response code = " + std::to_string(response_code));

    ++harvest_params->harvested_url_count_;

    if (log) {
        LOG_DEBUG("Harvested " + StringUtil::ToString(record_count_and_previously_downloaded_count.first) + " record(s) from "
                 + harvest_url + '\n' + "of which "
                 + StringUtil::ToString(record_count_and_previously_downloaded_count.first
                                        - record_count_and_previously_downloaded_count.second)
                 + " records were new records.");
    }
    return record_count_and_previously_downloaded_count;
}

/*

inline static std::string DeliveryModeToSqlEnum(BSZUpload::DeliveryMode delivery_mode) {
    return StringUtil::ToLower(std::find_if(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.begin(), BSZUpload::STRING_TO_DELIVERY_MODE_MAP.end(),
                                           [delivery_mode](const std::pair<std::string, int> &entry) -> bool { return static_cast<int>
                                           (delivery_mode) == entry.second; })->first);
}


inline static BSZUPload::DeliveryMode SqlEnumToDeliveryMode(const std::string &delivery_mode) {
    return static_cast<DeliveryMode>(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.at(StringUtil::ToUpper(delivery_mode)));
}


inline static void UpdateDownloadTrackerEntryFromDbRow(const DbRow &row, DownloadTracker::Entry * const entry) {
    if (row.empty())
        LOG_ERROR("Couldn't extract DownloadTracker entry from empty DbRow");

    entry->url_ = row["url"];
    entry->last_harvest_time_ = SqlUtil::DatetimeToTimeT(row["last_harvest_time"]);
    entry->journal_name_ = row["journal_name"];
    entry->error_message_ = row["error_message"];
    entry->hash_ = row["checksum"];
    entry->delivery_mode_ = SqlEnumToDeliveryMode(row["delivery_mode"]);
} */


UnsignedPair HarvestSite(const SimpleCrawler::SiteDesc &site_desc, const SimpleCrawler::Params &crawler_params,
                         const std::shared_ptr<RegexMatcher> &supported_urls_regex,
                         const std::shared_ptr<HarvestParams> &harvest_params, const SiteParams &site_params,
                         File * const progress_file)
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
                Harvest(page_details.url_, harvest_params, site_params, page_details.body_));
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
                        const SiteParams &site_params)
{
    return Harvest(url, harvest_params, site_params, "");
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
                                   const SiteParams &site_params, DbConnection * const db_connection)
{
    UnsignedPair total_record_count_and_previously_downloaded_record_count;

    if (mode != RSSHarvestMode::NORMAL)
        LOG_INFO("Processing URL: " + feed_url);

    Downloader::Params downloader_params;
    downloader_params.user_agent_ = harvest_params->user_agent_;
    Downloader downloader(feed_url, downloader_params);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Download problem for \"" + feed_url + "\": " + downloader.getLastErrorMessage());
        return total_record_count_and_previously_downloaded_record_count;
    }

    SyndicationFormat::AugmentParams syndication_format_site_params;
    syndication_format_site_params.strptime_format_ = site_params.strptime_format_;
    std::string err_msg;
    std::unique_ptr<SyndicationFormat> syndication_format(
        SyndicationFormat::Factory(downloader.getMessageBody(), syndication_format_site_params, &err_msg));
    if (syndication_format == nullptr) {
        LOG_WARNING("Problem parsing XML document for \"" + feed_url + "\": " + err_msg);
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
            Harvest(item.getLink(), harvest_params, site_params, "", /* verbose = */ mode != RSSHarvestMode::NORMAL));
        total_record_count_and_previously_downloaded_record_count += record_count_and_previously_downloaded_count;

        if (mode != RSSHarvestMode::TEST)
            db_connection->queryOrDie("INSERT INTO rss_items SET feed_id='" + feed_id + "',item_id='"
                                      + db_connection->escapeString(item.getId()) + "'");

    }
    if (mode != RSSHarvestMode::TEST)
        UpdateLastBuildDate(db_connection, feed_url, last_build_date);

    return total_record_count_and_previously_downloaded_record_count;
}


} // namespace Zotero
