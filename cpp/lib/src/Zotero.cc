/** \brief Interaction with Zotero Translation Server
 *         public functions are named like endpoints
 *         see https://github.com/zotero/translation-server
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
#include <uuid/uuid.h>
#include "JSON.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "util.h"


const std::vector<std::string> Zotero::ExportFormats
    { "bibtex", "biblatex", "bookmarks", "coins", "csljson", "mods", "refer",
    "rdf_bibliontology", "rdf_dc", "rdf_zotero", "ris", "wikipedia", "tei" };


const std::string Zotero::DEFAULT_SUBFIELD_CODE = "eng";


struct Date {
    static const unsigned INVALID = 0;
    unsigned day_;
    unsigned month_;
    unsigned year_;
public:
    Date(): day_(INVALID), month_(INVALID), year_(INVALID) { }
};


Date StringToDate(const std::string &date_str) {
    Date date;

    time_t unix_time(WebUtil::ParseWebDateAndTime(date_str));
    if (unix_time != TimeUtil::BAD_TIME_T) {
        tm *tm(::gmtime(&unix_time));
        if (unlikely(tm == nullptr))
            ERROR("gmtime(3) failed to convert a time_t! (" + date_str + ")");
        date.day_   = tm->tm_mday;
        date.month_ = tm->tm_mon;
        date.year_  = tm->tm_year;
    } else
        WARNING("don't know how to convert \"" + date_str + "\" to a Date instance!");

    return date;
}


// We try to be unique for the machine we're on.  Beyond that we may have a problem.
std::string GetNextSessionId() {
    static unsigned counter;
    static uint32_t uuid[4];
    if (unlikely(counter == 0))
        ::uuid_generate(reinterpret_cast<unsigned char *>(uuid));
    ++counter;
    return "ub_tools_zts_client_" + StringUtil::ToString(uuid[0]) + StringUtil::ToString(uuid[1])
           + StringUtil::ToString(uuid[2]) + StringUtil::ToString(uuid[3]) + "_" + StringUtil::ToString(counter);
}


std::string GetNextControlNumber() {
    static unsigned last_control_number;
    ++last_control_number;
    static const std::string prefix("ZTS");
    return prefix + StringUtil::PadLeading(std::to_string(last_control_number), 7, '0');
}


bool Zotero::TranslationServer::Export(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
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
        return true;
    }
}


bool Zotero::TranslationServer::Import(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
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
        return true;
    }
}


bool Zotero::TranslationServer::Web(const Url &zts_server_url, const TimeLimit &time_limit, Downloader::Params downloader_params,
                                    const Url &harvest_url, const std::string &harvested_html,
                                    std::string * const response_body, unsigned * response_code, std::string * const error_message)
{
    const std::string endpoint_url(Url(zts_server_url.toString() + "/web"));
    downloader_params.additional_headers_ = { "Accept: application/json", "Content-Type: application/json" };
    downloader_params.post_data_ = "{\"url\":\"" + JSON::EscapeString(harvest_url) + "\","
                                   + "\"sessionid\":\"" + JSON::EscapeString(GetNextSessionId()) + "\"";
    if (not harvested_html.empty()) {
        //downloader_params.post_data_ += ",\"cachedHTML\":\"" + JSON::EscapeString(harvested_html) + "\"";
        INFO("TODO: implement using cached html");
    }
    downloader_params.post_data_ += "}";

    Downloader downloader(endpoint_url, downloader_params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    } else {
        *response_code = downloader.getResponseCode();
        *response_body = downloader.getMessageBody();
        return true;
    }
}


std::unique_ptr<Zotero::FormatHandler> Zotero::FormatHandler::Factory(const std::string &output_format,
                                                                      const std::string &output_file,
                                                                      std::shared_ptr<HarvestMaps> harvest_maps,
                                                                      std::shared_ptr<HarvestParams> harvest_params)
{
    if (output_format == "marcxml" or output_format == "marc21")
        return std::unique_ptr<FormatHandler>(new MarcFormatHandler(output_format, output_file, harvest_maps, harvest_params));
    else if (output_format == "json")
        return std::unique_ptr<FormatHandler>(new JsonFormatHandler(output_format, output_file, harvest_maps, harvest_params));
    else if (std::find(Zotero::ExportFormats.begin(), Zotero::ExportFormats.end(), output_format) != Zotero::ExportFormats.end())
        return std::unique_ptr<FormatHandler>(new ZoteroFormatHandler(output_format, output_file, harvest_maps, harvest_params));
    else
        ERROR("invalid output-format: " + output_format);
}

void Zotero::JsonFormatHandler::prepareProcessing() {
    output_file_object_ = new File(output_file_, "w");
    output_file_object_->write("[");
}


std::pair<unsigned, unsigned> Zotero::JsonFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    if (record_count_ > 0)
        output_file_object_->write(",");
    output_file_object_->write(object_node->toString());
    ++record_count_;
    return std::make_pair(1, 0);
}


void Zotero::JsonFormatHandler::finishProcessing() {
    output_file_object_->write("]");
    output_file_object_->close();
}


void Zotero::ZoteroFormatHandler::prepareProcessing() {
    json_buffer_ = "[";
}


std::pair<unsigned, unsigned> Zotero::ZoteroFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    if (record_count_ > 0)
        json_buffer_ += ",";
    json_buffer_ += object_node->toString();
    ++record_count_;
    return std::make_pair(1, 0);
}


void Zotero::ZoteroFormatHandler::finishProcessing() {
    json_buffer_ += "]";

    Downloader::Params downloader_params;
    std::string response_body;
    std::string error_message;

    if (not Zotero::TranslationServer::Export(harvest_params_->zts_server_url_, Zotero::DEFAULT_CONVERSION_TIMEOUT, downloader_params,
                                              output_format_, json_buffer_, &response_body, &error_message))
        ERROR("converting to target format failed: " + error_message);
    else
        FileUtil::WriteString(output_file_, response_body);
}


void Zotero::MarcFormatHandler::ExtractKeywords(std::shared_ptr<const JSON::JSONNode> tags_node, const std::string &issn,
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
                ERROR("\"" + issn_and_field_tag_and_subfield_code->second
                      + "\" is not a valid MARC tag + subfield code! (Error in \"ISSN_to_keyword_field.map\"!)");
            marc_field    = issn_and_field_tag_and_subfield_code->second.substr(0, 3);
            marc_subfield =  issn_and_field_tag_and_subfield_code->second[3];
        }
    }

    for (auto tag : *tags) {
        const std::shared_ptr<const JSON::ObjectNode> tag_object(JSON::JSONNode::CastToObjectNodeOrDie("tag", tag));
        const std::shared_ptr<const JSON::JSONNode> tag_node(tag_object->getNode("tag"));
        if (tag_node == nullptr)
            WARNING("unexpected: tag object does not contain a \"tag\" entry!");
        else if (tag_node->getType() != JSON::JSONNode::STRING_NODE)
            ERROR("unexpected: tag object's \"tag\" entry is not a string node!");
        else
            CreateSubfieldFromStringNode("tag", tag_node, marc_field, marc_subfield, new_record);
    }
}


void Zotero::MarcFormatHandler::ExtractVolumeYearIssueAndPages(const JSON::ObjectNode &object_node,
                                                               MARC::Record * const new_record)
{
    std::vector<MARC::Subfield> subfields;

    const std::string date_str(object_node.getOptionalStringValue("date"));
    if (not date_str.empty()) {
        const Date date(StringToDate(date_str));
        if (date.year_ != Date::INVALID)
            subfields.emplace_back('j', std::to_string(date.year_));
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


void Zotero::MarcFormatHandler::CreateCreatorFields(const std::shared_ptr<const JSON::JSONNode> creators_node,
                                                    MARC::Record * const marc_record)
{
    const std::shared_ptr<const JSON::ArrayNode> creators_array(JSON::JSONNode::CastToArrayNodeOrDie("creators", creators_node));
    for (auto creator_node : *creators_array) {
        const std::shared_ptr<const JSON::ObjectNode> creator_object(JSON::JSONNode::CastToObjectNodeOrDie("creator", creator_node));

        const std::shared_ptr<const JSON::JSONNode> last_name_node(creator_object->getNode("lastName"));
        if (last_name_node == nullptr)
            ERROR("creator is missing a last name!");
        const std::shared_ptr<const JSON::StringNode> last_name(JSON::JSONNode::CastToStringNodeOrDie("lastName", last_name_node));
        std::string name(last_name->getValue());

        const std::shared_ptr<const JSON::JSONNode> first_name_node(creator_object->getNode("firstName"));
        if (first_name_node != nullptr) {
            const std::shared_ptr<const JSON::StringNode> first_name(JSON::JSONNode::CastToStringNodeOrDie("firstName", first_name_node));
            name += ", " + first_name->getValue();
        }

        std::string PPN;
        const std::shared_ptr<const JSON::JSONNode> ppn_node(creator_object->getNode("ppn"));
        if (ppn_node != nullptr) {
            const std::shared_ptr<const JSON::StringNode> ppn_string_node(JSON::JSONNode::CastToStringNodeOrDie("ppn", first_name_node));
            PPN = ppn_string_node->getValue();
            name = "!" + PPN + "!";
        }

        const std::shared_ptr<const JSON::JSONNode> creator_type(creator_object->getNode("creatorType"));
        std::string creator_role;
        if (creator_type != nullptr) {
            const std::shared_ptr<const JSON::StringNode> creator_role_node(JSON::JSONNode::CastToStringNodeOrDie("creatorType", creator_type));
            creator_role = creator_role_node->getValue();
        }

        if (creator_node == *(creators_array->begin())) {
            if (creator_role.empty())
                marc_record->insertField("100", { { 'a', name } });
            else
                marc_record->insertField("100", { { 'a', name }, { 'e', creator_role } });
        } else { // Not the first creator!
            if (creator_role.empty())
                marc_record->insertField("700", { { 'a', name } });
            else
                marc_record->insertField("700", { { 'a', name }, { 'e', creator_role } });
        }
    }
}


void Zotero::MarcFormatHandler::prepareProcessing() {
    marc_writer_ = MARC::Writer::Factory(output_file_);
}


std::pair<unsigned, unsigned> Zotero::MarcFormatHandler::processRecord(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    static RegexMatcher * const ignore_fields(RegexMatcher::RegexMatcherFactory(
        "^issue|pages|publicationTitle|volume|date|tags|libraryCatalog|itemVersion|accessDate$"));
    unsigned record_count(0), previously_downloaded_count(0);
    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL,
                            MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM,
                            GetNextControlNumber());
    bool is_journal_article(false);
    std::string publication_title, parent_ppn, parent_issn, issn;
    for (const auto &key_and_node : *object_node) {
        if (ignore_fields->matched(key_and_node.first))
            continue;

        if (key_and_node.first == "language")
            new_record.insertField("041", { { 'a',
                JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue() } });
        else if (key_and_node.first == "url")
            CreateSubfieldFromStringNode(key_and_node, "856", 'u', &new_record);
        else if (key_and_node.first == "title")
            CreateSubfieldFromStringNode(key_and_node, "245", 'a', &new_record);
        else if (key_and_node.first == "abstractNote")
            CreateSubfieldFromStringNode(key_and_node, "520", 'a', &new_record, /* indicator1 = */ '3');
        else if (key_and_node.first == "date")
            CreateSubfieldFromStringNode(key_and_node, "362", 'a', &new_record, /* indicator1 = */ '0');
        else if (key_and_node.first == "DOI") {
            if (unlikely(key_and_node.second->getType() != JSON::JSONNode::STRING_NODE))
                ERROR("expected DOI node to be a string node!");
            new_record.insertField(
                "856", { { 'u', "urn:doi:" + JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue()} });
        } else if (key_and_node.first == "shortTitle")
            CreateSubfieldFromStringNode(key_and_node, "246", 'a', &new_record);
        else if (key_and_node.first == "creators")
            CreateCreatorFields(key_and_node.second, &new_record);
        else if (key_and_node.first == "itemType") {
            const std::string item_type(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue());
            if (item_type == "journalArticle") {
                is_journal_article = true;
                publication_title = object_node->getOptionalStringValue("publicationTitle");
                ExtractVolumeYearIssueAndPages(*object_node, &new_record);
            } else if (item_type == "magazineArticle")
                ExtractVolumeYearIssueAndPages(*object_node, &new_record);
            else
                WARNING("unknown item type: \"" + item_type + "\"!");
        } else if (key_and_node.first == "rights") {
            const std::string copyright(JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue());
            if (UrlUtil::IsValidWebUrl(copyright))
                new_record.insertField("542", { { 'u', copyright } });
            else
                new_record.insertField("542", { { 'f', copyright } });
        } else
            WARNING("unknown key \"" + key_and_node.first + "\" with node type "
                            + JSON::JSONNode::TypeToString(key_and_node.second->getType()) + "! ("
                            + key_and_node.second->toString() + ")");
    }

    std::shared_ptr<const JSON::JSONNode> custom_node(object_node->getNode("ubtue"));
    if (custom_node != nullptr) {
        const std::shared_ptr<const JSON::ObjectNode>custom_object(JSON::JSONNode::CastToObjectNodeOrDie("ubtue", custom_node));
        parent_issn = custom_object->getOptionalStringValue("issnRaw");
        issn = custom_object->getOptionalStringValue("issnNormalized");

        // physical form
        const std::string physical_form(custom_object->getOptionalStringValue("physicalForm"));
        if (not physical_form.empty()) {
            if (physical_form == "A")
                new_record.insertField("007", "tu");
            else if (physical_form == "O")
                new_record.insertField("007", "cr uuu---uuuuu");
            else
                ERROR("unhandled value of physical form: \""
                      + physical_form + "\"!");
        }

        // volume
        const std::string volume(custom_object->getOptionalStringValue("volume"));
        if (not volume.empty()) {
            const auto field_it(new_record.findTag("936"));
            if (field_it == new_record.end())
                new_record.insertField("936", { { 'v', volume } });
            else
                field_it->getSubfields().addSubfield('v', volume);
        }

        // license code
        const std::string license(custom_object->getOptionalStringValue("licenseCode"));
        if (license == "l") {
            const auto field_it(new_record.findTag("936"));
            if (field_it != new_record.end())
                field_it->getSubfields().addSubfield('z', "Kostenfrei");
        }

        // SSG numbers:
        const std::string ssg_numbers(custom_object->getOptionalStringValue("ssgNumbers"));
        if (not ssg_numbers.empty())
            new_record.addSubfield("084", 'a', ssg_numbers);
    }

    // keywords:
    const std::shared_ptr<const JSON::JSONNode>tags_node(object_node->getNode("tags"));
    if (tags_node != nullptr)
        ExtractKeywords(tags_node, issn, harvest_maps_->ISSN_to_keyword_field_map_, &new_record);

    // Populate 773:
    if (is_journal_article) {
        std::vector<MARC::Subfield> subfields;
        if (not publication_title.empty())
            subfields.emplace_back('a', publication_title);
        if (not parent_issn.empty())
            subfields.emplace_back('x', parent_issn);
        if (not parent_ppn.empty())
            subfields.emplace_back('w', "(DE-576))" + parent_ppn);
        if (not subfields.empty())
            new_record.insertField("773", subfields);
    }

    // language code fallback:
    if (not new_record.hasTag("041"))
        new_record.insertField("041", { { 'a', DEFAULT_SUBFIELD_CODE } });

    // previously downloaded?
    const std::string checksum(MARC::CalcChecksum(new_record, /* exclude_001 = */ true));
    if (harvest_maps_->previously_downloaded_.find(checksum) == harvest_maps_->previously_downloaded_.cend()) {
        harvest_maps_->previously_downloaded_.emplace(checksum);
        marc_writer_->write(new_record);
    } else
        ++previously_downloaded_count;
    ++record_count;

    return std::make_pair(record_count, previously_downloaded_count);
}


void Zotero::MarcFormatHandler::finishProcessing() {
    marc_writer_.reset();
}


// If "key" is in "map", then return the mapped value, o/w return "key".
inline std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map) {
    const auto &key_and_value(map.find(key));
    return (key_and_value == map.cend()) ? key : key_and_value->second;
}


// "author" must be in the lastname,firstname format. Returns the empty string if no PPN was found.
std::string DownloadAuthorPPN(const std::string &author) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
         "<SMALL>PPN</SMALL>.*<div><SMALL>([0-9X]+)"));
    const std::string lookup_url("http://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/CMD?SGE=&ACT=SRCHM&MATCFILTER=Y"
                                 "&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y"
                                 "&NOABS=Y&ACT0=SRCHA&SHRTST=50&IKT0=1&TRM0=" + UrlUtil::UrlEncode(author)
                                 +"&ACT1=*&IKT1=2057&TRM1=*&ACT2=*&IKT2=8977&TRM2=theolog*&ACT3=-&IKT3=8978-&TRM3=1"
                                 "[1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]"
                                 "[0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]?");
    Downloader downloader(lookup_url);
    if (downloader.anErrorOccurred())
        WARNING(downloader.getLastErrorMessage());
    else if (matcher->matched(downloader.getMessageBody()))
        return (*matcher)[1];
    return "";
}


void AugmentJsonCreators(const std::shared_ptr<JSON::ArrayNode> creators_array,
                                 std::vector<std::string> * const comments)
{
    for (size_t i(0); i < creators_array->size(); ++i) {
        const std::shared_ptr<JSON::ObjectNode> creator_object(creators_array->getObjectNode(i));

        const std::shared_ptr<const JSON::JSONNode> last_name_node(creator_object->getNode("lastName"));
        if (last_name_node == nullptr)
            ERROR("creator is missing a last name!");
        std::string name(creator_object->getStringValue("lastName"));

        const std::shared_ptr<const JSON::JSONNode> first_name_node(creator_object->getNode("firstName"));
        if (first_name_node != nullptr)
            name += ", " + creator_object->getStringValue("firstName");

        const std::string PPN(DownloadAuthorPPN(name));
        if (not PPN.empty()) {
            comments->emplace_back("Added author PPN " + PPN + " for author " + name);
            creator_object->insert("ppn", std::make_shared<JSON::StringNode>(PPN));
        }
    }
}


// Improve JSON result delivered by Zotero Translation Server
void AugmentJson(const std::shared_ptr<JSON::ObjectNode> object_node, const std::shared_ptr<const Zotero::HarvestMaps> harvest_maps) {
    INFO("Augmenting JSON...");
    std::map<std::string, std::string> custom_fields;
    std::vector<std::string> comments;
    std::string issn_raw, issn_normalized;
    std::shared_ptr<JSON::StringNode> language_node(nullptr);
    for (auto &key_and_node : *object_node) {
        if (key_and_node.first == "language") {
            language_node = JSON::JSONNode::CastToStringNodeOrDie("language", key_and_node.second);
            const std::string language_json(language_node->getValue());
            const std::string language_mapped(OptionalMap(language_json,
                            harvest_maps->language_to_language_code_map_));
            if (language_json != language_mapped) {
                language_node->setValue(language_mapped);
                comments.emplace_back("changed \"language\" from \"" + language_json + "\" to \"" + language_mapped + "\"");
            }
        }
        else if (key_and_node.first == "creators") {
            std::shared_ptr<JSON::ArrayNode> creators_array(JSON::JSONNode::CastToArrayNodeOrDie("creators", key_and_node.second));
            AugmentJsonCreators(creators_array, &comments);
        }
        else if (key_and_node.first == "ISSN") {
            issn_raw = JSON::JSONNode::CastToStringNodeOrDie(key_and_node.first, key_and_node.second)->getValue();
            if (unlikely(not MiscUtil::NormaliseISSN(issn_raw, &issn_normalized)))
                ERROR("\"" + issn_raw + "\" is not a valid ISSN!");

            const auto ISSN_and_parent_ppn(harvest_maps->ISSN_to_superior_ppn_map_.find(issn_normalized));
            if (ISSN_and_parent_ppn != harvest_maps->ISSN_to_superior_ppn_map_.cend()) {
                issn_raw = ISSN_and_parent_ppn->second;
                issn_normalized = ISSN_and_parent_ppn->second;
            }

            custom_fields.emplace(std::pair<std::string, std::string>("issnRaw", issn_raw));
            custom_fields.emplace(std::pair<std::string, std::string>("issnNormalized", issn_normalized));
        }
    }

    // ISSN specific overrides
    if (not issn_normalized.empty()) {
        // physical form
        const auto ISSN_and_physical_form(harvest_maps->ISSN_to_physical_form_map_.find(issn_normalized));
        if (ISSN_and_physical_form != harvest_maps->ISSN_to_physical_form_map_.cend()) {
            if (ISSN_and_physical_form->second == "A")
                custom_fields.emplace(std::pair<std::string, std::string>("physicalForm", "A"));
            else if (ISSN_and_physical_form->second == "O")
                custom_fields.emplace(std::pair<std::string, std::string>("physicalForm", "O"));
            else
                ERROR("unhandled entry in physical form map: \""
                      + ISSN_and_physical_form->second + "\"!");
        }

        // language
        const auto ISSN_and_language(harvest_maps->ISSN_to_language_code_map_.find(issn_normalized));
        if (ISSN_and_language != harvest_maps->ISSN_to_language_code_map_.cend()) {
            if (language_node != nullptr) {
                const std::string language_old(language_node->getValue());
                language_node->setValue(ISSN_and_language->second);
                comments.emplace_back("changed \"language\" from \"" + language_old + "\" to \"" + ISSN_and_language->second + "\" due to ISSN map");
            } else {
                language_node = std::make_shared<JSON::StringNode>(ISSN_and_language->second);
                object_node->insert("language", language_node);
                comments.emplace_back("added \"language\" \"" + ISSN_and_language->second + "\" due to ISSN map");
            }
        }

        // volume
        const std::string volume(object_node->getOptionalStringValue("volume"));
        if (volume.empty()) {
            const auto ISSN_and_volume(harvest_maps->ISSN_to_volume_map_.find(issn_normalized));
            if (ISSN_and_volume != harvest_maps->ISSN_to_volume_map_.cend()) {
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
        const auto ISSN_and_license_code(harvest_maps->ISSN_to_licence_map_.find(issn_normalized));
        if (ISSN_and_license_code != harvest_maps->ISSN_to_licence_map_.end()) {
            if (ISSN_and_license_code->second != "l")
                WARNING("ISSN_to_licence.map contains an ISSN that has not been mapped to an \"l\" but \""
                        + ISSN_and_license_code->second
                        + "\" instead and we don't know what to do with it!");
            else
                custom_fields.emplace(std::pair<std::string, std::string>("licenseCode", ISSN_and_license_code->second));
        }

        // SSG numbers:
        const auto ISSN_and_SSGN_numbers(harvest_maps->ISSN_to_SSG_map_.find(issn_normalized));
        if (ISSN_and_SSGN_numbers != harvest_maps->ISSN_to_SSG_map_.end())
            custom_fields.emplace(std::pair<std::string, std::string>("ssgNumbers", ISSN_and_SSGN_numbers->second));
    }

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
        object_node->insert("ubtue", custom_object);
    }
}


bool ParseLine(const std::string &line, std::string * const key, std::string * const value) {
    key->clear(), value->clear();

    // Extract the key:
    auto ch(line.cbegin());
    while (ch != line.cend() and *ch != '=') {
        if (unlikely(*ch == '\\')) {
            ++ch;
            if (unlikely(ch == line.cend()))
                return false;
        }
        *key += *ch++;
    }
    if (unlikely(ch == line.cend()))
        return false;
    ++ch; // Skip over the equal-sign.

    // Extract value:
    while (ch != line.cend() and *ch != '#' /* Comment start. */) {
        if (unlikely(*ch == '\\')) {
            ++ch;
            if (unlikely(ch == line.cend()))
                return false;
        }
        *value += *ch++;
    }
    StringUtil::RightTrim(value);

    return not key->empty() and not value->empty();
}


void Zotero::LoadMapFile(const std::string &filename, std::unordered_map<std::string, std::string> * const from_to_map) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(filename));

    unsigned line_no(0);
    while (not input->eof()) {
        std::string line(input->getline());
        ++line_no;

        StringUtil::Trim(&line);
        std::string key, value;
        if (not ParseLine(line, &key, &value))
            ERROR("invalid input on line \"" + std::to_string(line_no) + "\" in \""
                          + input->getPath() + "\"!");
        from_to_map->emplace(key, value);
    }
}


const std::shared_ptr<RegexMatcher> Zotero::LoadSupportedURLsRegex(const std::string &map_directory_path) {
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
        ERROR("compilation of the combined regex failed: " + err_msg);

    return supported_urls_regex;
}


std::shared_ptr<Zotero::HarvestMaps> Zotero::LoadMapFilesFromDirectory(const std::string &map_directory_path) {
    std::shared_ptr<HarvestMaps> maps(new HarvestMaps);
    LoadMapFile(map_directory_path + "language_to_language_code.map", &maps->language_to_language_code_map_);
    LoadMapFile(map_directory_path + "ISSN_to_language_code.map", &maps->ISSN_to_language_code_map_);
    LoadMapFile(map_directory_path + "ISSN_to_licence.map", &maps->ISSN_to_licence_map_);
    LoadMapFile(map_directory_path + "ISSN_to_keyword_field.map", &maps->ISSN_to_keyword_field_map_);
    LoadMapFile(map_directory_path + "ISSN_to_physical_form.map", &maps->ISSN_to_physical_form_map_);
    LoadMapFile(map_directory_path + "ISSN_to_superior_ppn.map", &maps->ISSN_to_superior_ppn_map_);
    LoadMapFile(map_directory_path + "ISSN_to_volume.map", &maps->ISSN_to_volume_map_);
    LoadMapFile(map_directory_path + "ISSN_to_SSG.map", &maps->ISSN_to_SSG_map_);
    return maps;
}


std::pair<unsigned, unsigned> Zotero::Harvest(const std::string &harvest_url,
                                              const std::string &harvested_html,
                                              const std::shared_ptr<Zotero::HarvestParams> harvest_params,
                                              const std::shared_ptr<const Zotero::HarvestMaps> harvest_maps,
                                              bool log)
{
    std::pair<unsigned, unsigned> record_count_and_previously_downloaded_count;
    static std::unordered_set<std::string> already_harvested_urls;
    if (already_harvested_urls.find(harvest_url) != already_harvested_urls.end()) {
        logger->info("Skipping URL (already harvested): " + harvest_url);
        return record_count_and_previously_downloaded_count;
    }
    already_harvested_urls.emplace(harvest_url);

    logger->info("Harvesting URL: " + harvest_url);

    std::string response_body, error_message;
    unsigned response_code;
    harvest_params->min_url_processing_time_.sleepUntilExpired();
    Downloader::Params downloader_params;
    const bool download_result(Zotero::TranslationServer::Web(harvest_params->zts_server_url_, /* time_limit = */ Zotero::DEFAULT_TIMEOUT, downloader_params,
                                                              Url(harvest_url), harvested_html, &response_body, &response_code, &error_message));

    harvest_params->min_url_processing_time_.restart();
    if (not download_result) {
        logger->info("Zotero conversion failed: " + error_message);
        return std::make_pair(0, 0);
    }

    // 500 => internal server error (e.g. error in translator))
    if (response_code == 500) {
        logger->info("Error: " + response_body);
        return std::make_pair(0, 0);
    }

    // 501 => not implemented (e.g. no translator available)
    if (response_code == 501) {
        logger->debug("Skipped (" + response_body + ")");
        return std::make_pair(0, 0);
    }

    std::shared_ptr<JSON::JSONNode> tree_root(nullptr);
    JSON::Parser json_parser(response_body);
    if (not (json_parser.parse(&tree_root)))
        ERROR("failed to parse returned JSON: " + json_parser.getErrorMessage() + "\n" + response_body);

    // 300 => multiple matches found, try to harvest children
    if (response_code == 300) {
        logger->info("multiple articles found => trying to harvest children");
        if (tree_root->getType() == JSON::ArrayNode::OBJECT_NODE) {
            const std::shared_ptr<const JSON::ObjectNode>object_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", tree_root));
            for (const auto &key_and_node : *object_node) {
                std::pair<unsigned, unsigned> record_count_and_previously_downloaded_count2 =
                    Harvest(key_and_node.first, "", harvest_params, harvest_maps, false /* log */);

                record_count_and_previously_downloaded_count.first += record_count_and_previously_downloaded_count2.first;
                record_count_and_previously_downloaded_count.second += record_count_and_previously_downloaded_count2.second;
            }
        }
    } else {
        const std::shared_ptr<const JSON::ArrayNode>json_array(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));
        for (const auto entry : *json_array) {
            const std::shared_ptr<JSON::ObjectNode> json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
            AugmentJson(json_object, harvest_maps);
            record_count_and_previously_downloaded_count = harvest_params->format_handler_->processRecord(json_object);
        }
    }
    ++harvest_params->harvested_url_count_;

    if (log) {
        logger->info("Harvested " + StringUtil::ToString(record_count_and_previously_downloaded_count.first) + " record(s) from "
                  + harvest_url + '\n' + "of which "
                  + StringUtil::ToString(record_count_and_previously_downloaded_count.first
                      - record_count_and_previously_downloaded_count.second)
                  + " records were new records.");
    }
    return record_count_and_previously_downloaded_count;
}
