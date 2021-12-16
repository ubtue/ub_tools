/** \brief Classes related to the Zotero Harvester's JSON-to-MARC conversion API
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

#include "BSZUtil.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "LobidUtil.h"
#include "NGram.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "ZoteroHarvesterConversion.h"
#include "ZoteroHarvesterZederInterop.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Conversion {


std::string MetadataRecord::toString() const {
    std::string out("MetadataRecord {\n");

    out += "\turl: " + url_ + ",\n";
    out += "\titem_type: " + item_type_ + ",\n";
    out += "\ttitle: " + title_ + ",\n";
    if (not short_title_.empty())
        out += "\tshort_title: " + short_title_ + ",\n";
    if (not abstract_note_.empty())
        out += "\tabstract_note: " + abstract_note_ + ",\n";
    if (not publication_title_.empty())
        out += "\tpublication_title: " + publication_title_ + ",\n";
    if (not volume_.empty())
        out += "\tvolume: " + volume_ + ",\n";
    if (not issue_.empty())
        out += "\tissue: " + issue_ + ",\n";
    if (not pages_.empty())
        out += "\tpages: " + pages_ + ",\n";
    if (not date_.empty())
        out += "\tdate: " + date_ + ",\n";
    if (not doi_.empty())
        out += "\tdoi: " + doi_ + ",\n";
    for (const auto &language : languages_)
        out += "\tlanguage: " + language + ",\n";
    if (not issn_.empty())
        out += "\tissn: " + issn_ + ",\n";
    if (not superior_ppn_.empty())
        out += "\tsuperior_ppn: " + superior_ppn_ + ",\n";
    out += "\tsuperior_type: " + std::to_string(static_cast<int>(superior_type_)) + ",\n";
    out += "\tssg: " + std::to_string(static_cast<int>(ssg_)) + ",\n";

    if (not creators_.empty()) {
        std::string creators("creators: [\n");
        for (const auto &creator : creators_) {
            creators += "\t\t{\n";
            creators += "\t\t\tfirst_name: " + creator.first_name_ + ",\n";
            creators += "\t\t\tlast_name: " + creator.last_name_ + ",\n";
            creators += "\t\t\ttype: " + creator.type_ + ",\n";
            if (not creator.affix_.empty())
                creators += "\t\t\taffix: " + creator.affix_ + ",\n";
            if (not creator.title_.empty())
                creators += "\t\t\ttitle: " + creator.title_ + ",\n";
            if (not creator.ppn_.empty())
                creators += "\t\t\tppn: " + creator.ppn_ + ",\n";
            if (not creator.gnd_number_.empty())
                creators += "\t\t\tgnd_number: " + creator.gnd_number_ + ",\n";
            creators += "\t\t},\n";
        }
        creators += "\t]";
        out += "\t" + creators + ",\n";
    }

    if (not keywords_.empty()) {
        std::string keywords("keywords: [ ");
        for (const auto &keyword : keywords_)
            keywords += keyword + ", ";
        TextUtil::UTF8Truncate(&keywords, keywords.size() - 2);
        keywords += " ]";
        out += "\t" + keywords + ",\n";
    }

    if (not custom_metadata_.empty()) {
        std::string custom_metadata("custom_metadata: [\n");
        for (const auto &metadata : custom_metadata_)
            custom_metadata += "\t\t{ " + metadata.first + ", " + metadata.second + " },\n";
        custom_metadata += "\t]";
        out += "\t" + custom_metadata + ",\n";
    }

    out += "}";
    return out;
}


MetadataRecord::SSGType MetadataRecord::GetSSGTypeFromString(const std::string &ssg_string) {
    const std::map<std::string, SSGType> ZEDER_STRINGS {
        { "FG_0",   SSGType::FG_0 },
        { "FG_1",   SSGType::FG_1 },
        { "FG_0/1", SSGType::FG_01 },
        { "FG_2,1", SSGType::FG_21 },
    };

    if (ZEDER_STRINGS.find(ssg_string) != ZEDER_STRINGS.end())
        return ZEDER_STRINGS.find(ssg_string)->second;

    return SSGType::INVALID;
}


void SuppressJsonMetadataForParams(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                                   const Config::ZoteroMetadataParams &zotero_metadata_params)
{
    const auto suppression_regex(zotero_metadata_params.fields_to_suppress_.find(node_name));
    if (suppression_regex != zotero_metadata_params.fields_to_suppress_.end()) {
        if (node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("metadata suppression filter has invalid node type '" + node_name + "'");

        const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
        if (suppression_regex->second->match(string_node->getValue())) {
            LOG_DEBUG("suppression regex '" + suppression_regex->second->getPattern() +
                      "' matched metadata field '" + node_name + "' value '" + string_node->getValue() + "'");
            string_node->setValue("");
        }
    }
}


void SuppressJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const ConversionParams &parameters)
{
    SuppressJsonMetadataForParams(node_name, node, parameters.global_params_.zotero_metadata_params_);
    SuppressJsonMetadataForParams(node_name, node, parameters.download_item_.journal_.zotero_metadata_params_);
}


void OverrideJsonMetadataForParams(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                                   const Config::ZoteroMetadataParams &zotero_metadata_params)
{
    const std::string ORIGINAL_VALUE_SPECIFIER("%org%");
    const auto override_pattern(zotero_metadata_params.fields_to_override_.find(node_name));

    if (override_pattern != zotero_metadata_params.fields_to_override_.end()) {
        if (node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("metadata override has invalid node type '" + node_name + "'");

        const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
        const auto string_value(string_node->getValue());
        const auto override_string(StringUtil::ReplaceString(ORIGINAL_VALUE_SPECIFIER, string_value,override_pattern->second));

        LOG_DEBUG("metadata field '" + node_name + "' value changed from '" + string_value + "' to '" + override_string + "'");
        string_node->setValue(override_string);
    }
}


void OverrideJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const ConversionParams &parameters)
{
    OverrideJsonMetadataForParams(node_name, node, parameters.global_params_.zotero_metadata_params_);
    OverrideJsonMetadataForParams(node_name, node, parameters.download_item_.journal_.zotero_metadata_params_);
}


void PostprocessTranslationServerResponse(const ConversionParams &parameters,
                                          std::shared_ptr<JSON::ArrayNode> * const response_json_array)
{
    // 'response_json_array' is a JSON array of metadata objects pertaining to individual URLs

    // firstly, we need to process item notes. they are encoded as separate objects
    // so, we'll need to iterate through the entires and append individual notes to their parents
    std::shared_ptr<JSON::ArrayNode> augmented_array(new JSON::ArrayNode());
    JSON::ObjectNode *last_entry(nullptr);

    for (auto entry : **response_json_array) {
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

    // swap the augmented array with the old one
    *response_json_array = augmented_array;

    // next, we modify the metadata objects to suppress and/or override individual fields
    for (auto entry : **response_json_array) {
        const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        JSON::VisitLeafNodes("root", json_object, SuppressJsonMetadata, std::ref(parameters));
        JSON::VisitLeafNodes("root", json_object, OverrideJsonMetadata, std::ref(parameters));
    }
}


bool ZoteroItemMatchesExclusionFiltersForParams(const std::shared_ptr<JSON::ObjectNode> &zotero_item,
                                             const Util::HarvestableItem &download_item,
                                             const Config::ZoteroMetadataParams &zotero_metadata_params)
{
    if (zotero_metadata_params.exclusion_filters_.empty())
        return false;

    bool found_match(false);
    std::string exclusion_string;
    auto metadata_exclusion_predicate = [&found_match, &exclusion_string]
                                        (const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                                         const Config::ZoteroMetadataParams &zotero_metadata_params) -> void
    {
        const auto filter_regex(zotero_metadata_params.exclusion_filters_.find(node_name));
        if (filter_regex != zotero_metadata_params.exclusion_filters_.end()) {
            if (node->getType() != JSON::JSONNode::STRING_NODE)
                LOG_ERROR("metadata exclusion filter has invalid node type '" + node_name + "'");

            const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
            if (filter_regex->second->match(string_node->getValue())) {
                found_match = true;
                exclusion_string = node_name + "/" + filter_regex->second->getPattern() + "/";
            }
        }
    };

    JSON::VisitLeafNodes("root", zotero_item, metadata_exclusion_predicate, std::ref(zotero_metadata_params));
    if (found_match)
        LOG_INFO("zotero metadata for '" + download_item.url_.toString() + " matched exclusion filter (" + exclusion_string + ")");

    return found_match;
}


bool ZoteroItemMatchesExclusionFilters(const std::shared_ptr<JSON::ObjectNode> &zotero_item,
                                       const ConversionParams &parameters)
{
    return (ZoteroItemMatchesExclusionFiltersForParams(zotero_item, parameters.download_item_, parameters.global_params_.zotero_metadata_params_)
            or ZoteroItemMatchesExclusionFiltersForParams(zotero_item, parameters.download_item_, parameters.download_item_.journal_.zotero_metadata_params_));
}


static inline std::string GetStrippedHTMLStringFromJSON(const std::shared_ptr<JSON::ObjectNode> &json_object,
                                                        const std::string &field_name)
{ return HtmlUtil::StripHtmlTags(json_object->getOptionalStringValue(field_name)); }


void ConvertZoteroItemToMetadataRecord(const std::shared_ptr<JSON::ObjectNode> &zotero_item,
                                       MetadataRecord * const metadata_record)
{
    metadata_record->item_type_ = GetStrippedHTMLStringFromJSON(zotero_item, "itemType");
    metadata_record->title_ = GetStrippedHTMLStringFromJSON(zotero_item, "title");
    metadata_record->short_title_ = GetStrippedHTMLStringFromJSON(zotero_item, "shortTitle");
    metadata_record->abstract_note_ = GetStrippedHTMLStringFromJSON(zotero_item, "abstractNote");
    metadata_record->publication_title_ = GetStrippedHTMLStringFromJSON(zotero_item, "publicationTitle");
    if (metadata_record->publication_title_.empty())
        metadata_record->publication_title_ = GetStrippedHTMLStringFromJSON(zotero_item, "websiteTitle");
    metadata_record->volume_ = GetStrippedHTMLStringFromJSON(zotero_item, "volume");
    metadata_record->issue_ = GetStrippedHTMLStringFromJSON(zotero_item, "issue");
    metadata_record->pages_ = GetStrippedHTMLStringFromJSON(zotero_item, "pages");
    metadata_record->date_ = GetStrippedHTMLStringFromJSON(zotero_item, "date");
    metadata_record->doi_ = GetStrippedHTMLStringFromJSON(zotero_item, "DOI");
    const std::string language(GetStrippedHTMLStringFromJSON(zotero_item, "language"));
    if (not language.empty())
        metadata_record->languages_.emplace(language);
    metadata_record->url_ = GetStrippedHTMLStringFromJSON(zotero_item, "url");
    metadata_record->issn_ = GetStrippedHTMLStringFromJSON(zotero_item, "ISSN");

    const auto creators_array(zotero_item->getOptionalArrayNode("creators"));
    if (creators_array and not creators_array->empty()) {
        for (const auto &entry :*creators_array) {
            const auto creator_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", entry));
            if (creator_object->hasNode("firstName") or creator_object->hasNode("lastName")) {
                metadata_record->creators_.emplace_back(GetStrippedHTMLStringFromJSON(creator_object, "firstName"),
                                                        GetStrippedHTMLStringFromJSON(creator_object, "lastName"),
                                                        GetStrippedHTMLStringFromJSON(creator_object, "creatorType"));
            } else if (creator_object->hasNode("name")) {
                metadata_record->creators_.emplace_back("",
                                                        GetStrippedHTMLStringFromJSON(creator_object, "name"),
                                                        GetStrippedHTMLStringFromJSON(creator_object, "creatorType"));
            } else {
                LOG_WARNING("Don't know how to handle authors in non-empty creator_object " + creator_object->toString());
            }
        }
    }

    const auto tags_array(zotero_item->getOptionalArrayNode("tags"));
    if (tags_array) {
        for (const auto &entry :*tags_array) {
            const auto tag_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", entry));
            const auto tag(GetStrippedHTMLStringFromJSON(tag_object, "tag"));
            if (not tag.empty())
                metadata_record->keywords_.emplace_back(tag);
        }
    }

    const auto notes_array(zotero_item->getOptionalArrayNode("notes"));
    if (notes_array) {
        for (const auto &entry :*notes_array) {
            const auto note_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", entry));
            const auto note(note_object->getOptionalStringValue("note"));
            if (not note.empty()) {
                const auto first_colon_pos(note.find(':'));
                if (unlikely(first_colon_pos == std::string::npos)) {
                    LOG_WARNING("additional metadata in \"notes\" is missing a colon! data: '" + note + "'");
                    continue;   // could be a valid note added by the translator
                }

                metadata_record->custom_metadata_.emplace(note.substr(0, first_colon_pos), note.substr(first_colon_pos + 1));
            }
        }
    }
}


void SplitIntoFirstAndLastAuthorNames(const std::string &author, std::string * const first_name, std::string * const last_name) {
    const auto normalised_author(TextUtil::CollapseAndTrimWhitespace(author));
    const auto name_separator(normalised_author.rfind(' '));
    if (name_separator != std::string::npos) {
        *first_name = normalised_author.substr(0, name_separator);
        *last_name = normalised_author.substr(name_separator + 1);
    } else {
        *first_name = normalised_author;
        last_name->clear();
    }
}


bool FilterEmptyAndCommentLines(std::string str) {
    StringUtil::TrimWhite(&str);
    return not str.empty() and str.front() != '#';
}


const std::string AUTHOR_NAME_BLACKLIST(UBTools::GetTuelibPath() + "zotero-enhancement-maps/author_name_blacklist.txt");


ThreadSafeRegexMatcher InitializeBlacklistedAuthorTokenMatcher() {
    std::unordered_set<std::string> blacklisted_tokens, filtered_blacklisted_tokens;
    auto string_data(FileUtil::ReadStringOrDie(AUTHOR_NAME_BLACKLIST));
    StringUtil::Split(string_data, '\n', &blacklisted_tokens, /* suppress_empty_components = */true);

    StlHelpers::Functional::Filter(blacklisted_tokens.begin(), blacklisted_tokens.end(), filtered_blacklisted_tokens,
                                   FilterEmptyAndCommentLines);

    std::string match_pattern("\\b(");
    bool is_first_token(true);
    for (const auto &blacklisted_token : filtered_blacklisted_tokens) {
        if (not is_first_token)
            match_pattern += "|";
        else
            is_first_token = false;
        match_pattern += RegexMatcher::Escape(blacklisted_token);
    }
    match_pattern += ")\\b";

   return ThreadSafeRegexMatcher(match_pattern, ThreadSafeRegexMatcher::ENABLE_UTF8 | ThreadSafeRegexMatcher::ENABLE_UCP | ThreadSafeRegexMatcher::CASE_INSENSITIVE);
}


const ThreadSafeRegexMatcher BLACKLISTED_AUTHOR_TOKEN_MATCHER(InitializeBlacklistedAuthorTokenMatcher());


void StripBlacklistedTokensFromAuthorName(std::string * const first_name, std::string * const last_name) {
    std::string first_name_buffer(BLACKLISTED_AUTHOR_TOKEN_MATCHER.replaceAll(*first_name, "")),
                last_name_buffer(BLACKLISTED_AUTHOR_TOKEN_MATCHER.replaceAll(*last_name, ""));

    StringUtil::TrimWhite(&first_name_buffer);
    StringUtil::TrimWhite(&last_name_buffer);

    *first_name = first_name_buffer;
    *last_name = last_name_buffer;
}


bool Is655Keyword(const std::string &keyword) {
    static const auto keyword_list(FileUtil::ReadLines::ReadOrDie(UBTools::GetTuelibPath() + "zotero-enhancement-maps/marc655_keywords.txt",
                                                                  FileUtil::ReadLines::TRIM_LEFT_AND_RIGHT, FileUtil::ReadLines::TO_LOWER));
    const std::string keyword_lower(TextUtil::UTF8ToLower(keyword));
    return std::find(keyword_list.begin(), keyword_list.end(), keyword_lower) != keyword_list.end();
}


static const std::set<std::string> VALID_TITLES {
    "jr", "sr", "sj", "s.j", "s.j.", "fr", "hr", "dr", "prof", "em"
};


bool IsAuthorNameTokenTitle(std::string token) {
    bool final_period(token.back() == '.');
    if (final_period)
        token.erase(token.size() - 1);

    TextUtil::UTF8ToLower(&token);
    return VALID_TITLES.find(token) != VALID_TITLES.end();
}


static const std::set<std::string> VALID_AFFIXES {
    "i", "ii", "iii", "iv", "v"
};


bool IsAuthorNameTokenAffix(std::string token) {
    TextUtil::UTF8ToLower(&token);
    return VALID_AFFIXES.find(token) != VALID_AFFIXES.end();
}


void JoinAuthorTokens(const std::vector<std::string> &tokens_first, std::string * const first_name,
                      const std::vector<std::string> &tokens_last, std::string * const last_name) {
        StringUtil::Join(tokens_first, ' ', first_name);
        StringUtil::Join(tokens_last, ' ', last_name);
}


void AdjustFirstAndLastNameByLanguage(std::string * const first_name, std::string * const last_name, const std::set<std::string> &languages) {
    // In Spanish we have two last name components, so move over if appropriate
    if (languages.find("spa") != languages.end()) {
        // Skip transformation if the first name/last name association already seems reasonable
        static const auto first_name_end_preposition(ThreadSafeRegexMatcher("(des?\\s+las?|del|\\p{Lu}[.])$",
             ThreadSafeRegexMatcher::ENABLE_UTF8 | ThreadSafeRegexMatcher::ENABLE_UCP | ThreadSafeRegexMatcher::CASE_INSENSITIVE));
        if (first_name_end_preposition.match(*first_name))
            return;

        std::vector<std::string> first_name_tokens;
        std::vector<std::string> last_name_tokens;
        StringUtil::Split(*first_name, ' ', &first_name_tokens, /* suppress_empty_components = */ true);
        if (first_name_tokens.size() <= 1)
            return;
        StringUtil::Split(*last_name, ' ', &last_name_tokens, /* suppress_empty_components = */ true);
        if (last_name_tokens.size() >= 2) // Probably fixed elsewhere...
            return;
        // Special handling for "y"
        const auto y_iterator(std::find(first_name_tokens.begin(), first_name_tokens.end(), "y"));
        if (y_iterator != first_name_tokens.end()) {
            const auto offset(std::distance(first_name_tokens.begin(), y_iterator));
            if (offset >=1) {
                last_name_tokens.insert(last_name_tokens.begin(), std::make_move_iterator(last_name_tokens.begin() + (offset - 1)),
                                                                  std::make_move_iterator(last_name_tokens.end()));
                first_name_tokens.erase(first_name_tokens.begin() + (offset - 1));
                JoinAuthorTokens(first_name_tokens, first_name, last_name_tokens, last_name);
                return;
            }
        }
        last_name_tokens.insert(last_name_tokens.begin(), first_name_tokens.back());
        first_name_tokens.pop_back();
        JoinAuthorTokens(first_name_tokens, first_name, last_name_tokens, last_name);
    }
}


void PostProcessAuthorName(std::string * const first_name, std::string * const last_name, std::string * const title,
                           std::string * const affix, const std::set<std::string> &languages)
{
    std::string first_name_buffer, title_buffer;
    std::vector<std::string> tokens;

    AdjustFirstAndLastNameByLanguage(first_name, last_name, languages);

    StringUtil::Split(*first_name, ' ', &tokens, /* suppress_empty_components = */true);
    for (const auto &token : tokens) {
        if (IsAuthorNameTokenTitle(token))
            title_buffer += token + " ";
        else
            first_name_buffer += token + " ";
    }

    std::string last_name_buffer, affix_buffer;
    StringUtil::Split(*last_name, ' ', &tokens, /* suppress_empty_components = */true);
    for (const auto &token : tokens) {
        if (IsAuthorNameTokenTitle(token))
            title_buffer += token + " ";
        else if (IsAuthorNameTokenAffix(token))
            affix_buffer += token + " ";
        else
            last_name_buffer += token + " ";
    }

    TextUtil::CollapseAndTrimWhitespace(&first_name_buffer);
    TextUtil::CollapseAndTrimWhitespace(&last_name_buffer);
    TextUtil::CollapseAndTrimWhitespace(&title_buffer);
    TextUtil::CollapseAndTrimWhitespace(&affix_buffer);

    StripBlacklistedTokensFromAuthorName(&first_name_buffer, &last_name_buffer);

    *title = title_buffer;
    *affix = affix_buffer;
    // try to reparse the name if either part of the name is empty
    if (first_name_buffer.empty())
        SplitIntoFirstAndLastAuthorNames(last_name_buffer, first_name, last_name);
    else if (last_name_buffer.empty())
        SplitIntoFirstAndLastAuthorNames(first_name_buffer, first_name, last_name);
    else if (not first_name_buffer.empty() and not last_name_buffer.empty()) {
        *first_name = first_name_buffer;
        *last_name = last_name_buffer;
    }

    LOG_DEBUG("post-processed author first name = '" + *first_name + "', last name = '" + *last_name +
              "', title = '" + *title + "', affix = '" + *affix + "'");
}


const std::string TIKA_SERVER_DETECT_STRING_LANGUAGE_URL("http://localhost:9998/language/string");
std::string TikaDetectLanguage(const std::string &record_text) {
    Downloader downloader;
    if (not downloader.putData(TIKA_SERVER_DETECT_STRING_LANGUAGE_URL, record_text))
        LOG_ERROR("Could not send data to Tika server");
    const std::string tika_detected_language(downloader.getMessageBody());
    if (tika_detected_language.empty())
        return "";
    return Config::GetNormalizedLanguage(tika_detected_language);
}


void NormalizeGivenLanguages(MetadataRecord * const metadata_record) {
    // Normalize given languages
    // We cant remove during iteration, so we use a copy
    std::set<std::string> languages(metadata_record->languages_);
    metadata_record->languages_.clear();
    for (const auto &language : languages) {
        if (not Config::IsAllowedLanguage(language)) {
            LOG_WARNING("Removing invalid language: " + language);
            continue;
        }

        if (not Config::IsNormalizedLanguage(language)) {
            const std::string normalized_language(Config::GetNormalizedLanguage(language));
            LOG_INFO("Normalized language: " + language + " => " + normalized_language);
            metadata_record->languages_.emplace(normalized_language);
        } else
            metadata_record->languages_.emplace(language);
    }
}


std::string DetectLanguage(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params) {
    std::string record_text;
    if (journal_params.language_params_.source_text_fields_.empty()
        or journal_params.language_params_.source_text_fields_ == "title")
    {
        record_text = metadata_record->title_;
    } else if (journal_params.language_params_.source_text_fields_ == "abstract")
        record_text = metadata_record->abstract_note_;
    else if (journal_params.language_params_.source_text_fields_ == "title+abstract")
        record_text = metadata_record->title_ + " " + metadata_record->abstract_note_;
    else
        LOG_ERROR("unknown text field '" + journal_params.language_params_.source_text_fields_ + "' for language detection");

    std::string detected_language(TikaDetectLanguage(record_text));
    // Fallback to custom NGram
    if (detected_language.empty()) {
        std::vector<NGram::DetectedLanguage> detected_languages;
        NGram::ClassifyLanguage(record_text, &detected_languages, journal_params.language_params_.expected_languages_,
                             /*alternative_cutoff_factor = */ 0);
        const auto top_language(detected_languages.front());
        detected_language = top_language.language_;
    }
    return detected_language;
}


void AdjustLanguages(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params) {
    NormalizeGivenLanguages(metadata_record);

    // Check if the needed settings exist
    if (journal_params.language_params_.expected_languages_.empty())
        return;

    // Override directly via configuration (if corresponding mode is set)
    if (journal_params.language_params_.mode_ == Config::LanguageParams::FORCE_LANGUAGES) {
        LOG_INFO("Override languages with configuration value: " + StringUtil::Join(journal_params.language_params_.expected_languages_, ","));
        metadata_record->languages_ = journal_params.language_params_.expected_languages_;
        return;
    }

    // automatic language detection
    std::string detected_language, configured_or_detected_info;
    if (journal_params.language_params_.expected_languages_.size() == 1) {
        detected_language = *journal_params.language_params_.expected_languages_.begin();
        configured_or_detected_info = "single configured";
    } else {
        detected_language = DetectLanguage(metadata_record, journal_params);
        configured_or_detected_info = "detected";
    }

    // compare language from zotero to detected language
    if (not detected_language.empty()) {
        if (journal_params.language_params_.mode_ == Config::LanguageParams::FORCE_DETECTION) {
            LOG_INFO ("Force language detection active - " + configured_or_detected_info + " language: " + detected_language);
            if (journal_params.language_params_.expected_languages_.find(detected_language) == journal_params.language_params_.expected_languages_.end()) {
                LOG_INFO("Detected language : " + detected_language + " is not in the given set of admissible languages. "
                         "No language will be set");
                metadata_record->languages_.clear();
            } else {
                LOG_INFO("Using detected language: " + detected_language);
                metadata_record->languages_ = { detected_language };
            }

        } else if (metadata_record->languages_.empty()) {
            LOG_INFO("Using " + configured_or_detected_info + " language: " + detected_language);
            metadata_record->languages_.emplace(detected_language);
        } else if (*metadata_record->languages_.begin() == detected_language and metadata_record->languages_.size() == 1)
            LOG_INFO("The given language is equal to the " + configured_or_detected_info + " language: " + detected_language);
        else {
            LOG_INFO("The given language " + StringUtil::Join(metadata_record->languages_, ",") + " and the " + configured_or_detected_info + " language " + detected_language + " are different. "
                     "No language will be set.");
            metadata_record->languages_.clear();
        }
    }
}


bool DetectReviewsWithMatcher(MetadataRecord * const metadata_record, ThreadSafeRegexMatcher * const review_matcher) {
    bool review_detected(false);

    if (review_matcher->match(metadata_record->title_)) {
        LOG_DEBUG("title matched review pattern");
        review_detected = true;
    } else if (review_matcher->match(metadata_record->short_title_)) {
        LOG_DEBUG("short title matched review pattern");
        review_detected = true;
    } else {
        for (const auto &keyword : metadata_record->keywords_) {
            if (review_matcher->match(keyword)) {
                LOG_DEBUG("keyword matched review pattern");
                review_detected = true;
            }
        }
    }

    if (review_detected)
        metadata_record->item_type_ = "review";
    return review_detected;
}


void DetectReviews(MetadataRecord * const metadata_record, const ConversionParams &parameters) {
    const auto &global_review_matcher(parameters.global_params_.review_regex_.get());
    if (global_review_matcher != nullptr and DetectReviewsWithMatcher(metadata_record, global_review_matcher))
        return;

    const auto &journal_review_matcher(parameters.download_item_.journal_.review_regex_.get());
    if (journal_review_matcher != nullptr)
        DetectReviewsWithMatcher(metadata_record, journal_review_matcher);
}


bool DetectNotesWithMatcher(MetadataRecord * const metadata_record, ThreadSafeRegexMatcher * const notes_matcher) {
    if (notes_matcher->match(metadata_record->title_)) {
        LOG_DEBUG("title matched note pattern");
        metadata_record->item_type_ = "note";
        return true;
    }
    return false;
}


void DetectNotes(MetadataRecord * const metadata_record, const ConversionParams &parameters) {
    const auto &global_notes_matcher(parameters.global_params_.notes_regex_.get());
    if (global_notes_matcher != nullptr and DetectNotesWithMatcher(metadata_record, global_notes_matcher))
        return;

    const auto &journal_notes_matcher(parameters.download_item_.journal_.notes_regex_.get());
    if (journal_notes_matcher != nullptr)
        DetectNotesWithMatcher(metadata_record, journal_notes_matcher);
}



const ThreadSafeRegexMatcher PAGE_RANGE_MATCHER("^(.+)-(.+)$");
const ThreadSafeRegexMatcher PAGE_RANGE_DIGIT_MATCHER("^(\\d+)-(\\d+)$");
const ThreadSafeRegexMatcher PAGE_ROMAN_NUMERAL_MATCHER("^M{0,4}(CM|CD|D?C{0,3})(XC|XL|L?X{0,3})(IX|IV|V?I{0,3})$");
const ThreadSafeRegexMatcher PROPER_LAST_NAME("^(?!\\p{L}\\.).*$");


bool IsProperLastName(const std::string &last_name) {
    return PROPER_LAST_NAME.match(last_name);
}


std::string GetSWBLookupURL(const ConversionParams &parameters) {
    const auto &subgroup_params(parameters.subgroup_params_);
    if (not subgroup_params.author_swb_lookup_url_.empty())
        return subgroup_params.author_swb_lookup_url_;
    return parameters.group_params_.author_swb_lookup_url_;
}


void AugmentMetadataRecord(MetadataRecord * const metadata_record, const ConversionParams &parameters)
{
    const auto &journal_params(parameters.download_item_.journal_);

    // normalise date
    if (not metadata_record->date_.empty()) {
        struct tm tm(TimeUtil::StringToStructTm(metadata_record->date_, journal_params.strptime_format_string_));
        const std::string date_normalized(std::to_string(tm.tm_year + 1900) + "-"
                                          + StringUtil::ToString(tm.tm_mon + 1, 10, 2, '0') + "-"
                                          + StringUtil::ToString(tm.tm_mday, 10, 2, '0'));
        metadata_record->date_ = date_normalized;
    }

    // normalise issue/volume
    StringUtil::LeftTrim(&metadata_record->issue_, '0');
    StringUtil::LeftTrim(&metadata_record->volume_, '0');

    // normalise pages
    const auto pages(metadata_record->pages_);
    // force uppercase for roman numeral detection
    auto page_match(PAGE_RANGE_MATCHER.match(StringUtil::ASCIIToUpper(pages)));
    if (page_match) {
        std::string converted_pages;
        if (PAGE_ROMAN_NUMERAL_MATCHER.match(page_match[1]))
            converted_pages += std::to_string(StringUtil::RomanNumeralToDecimal(page_match[1]));
        else
            converted_pages += page_match[1];

        converted_pages += "-";

        if (PAGE_ROMAN_NUMERAL_MATCHER.match(page_match[2]))
            converted_pages += std::to_string(StringUtil::RomanNumeralToDecimal(page_match[2]));
        else
            converted_pages += page_match[2];

        if (converted_pages != pages) {
            LOG_DEBUG("converted roman numeral page range '" + pages + "' to decimal page range '"
                      + converted_pages + "'");
            metadata_record->pages_ = converted_pages;
        }
    }

    page_match = PAGE_RANGE_DIGIT_MATCHER.match(metadata_record->pages_);
    if (page_match and page_match[1] == page_match[2])
        metadata_record->pages_ = page_match[1];

    // override publication title
    metadata_record->publication_title_ = journal_params.name_;

    // override ISSN (online > print > zotero) and select superior PPN (online > print)
    const auto &issn(journal_params.issn_);
    const auto &ppn(journal_params.ppn_);
    if (not issn.online_.empty()) {
        if (ppn.online_.empty())
            throw std::runtime_error("cannot use online ISSN \"" + issn.online_ + "\" because no online PPN is given!");
        metadata_record->issn_ = issn.online_;
        metadata_record->superior_ppn_ = ppn.online_;
        metadata_record->superior_type_ = MetadataRecord::SuperiorType::ONLINE;

        LOG_DEBUG("use online ISSN \"" + issn.online_ + "\" with online PPN \"" + ppn.online_ + "\"");
    } else if (not issn.print_.empty()) {
        if (ppn.print_.empty())
            throw std::runtime_error("cannot use print ISSN \"" + issn.print_ + "\" because no print PPN is given!");
        metadata_record->issn_ = issn.print_;
        metadata_record->superior_ppn_ = ppn.print_;
        metadata_record->superior_type_ = MetadataRecord::SuperiorType::PRINT;

        LOG_DEBUG("use print ISSN \"" + issn.print_ + "\" with print PPN \"" + ppn.print_ + "\"");
    } else {
        throw std::runtime_error("ISSN and PPN could not be chosen! ISSN online: \"" + issn.online_ + "\""
                                 + ", ISSN print: \"" + issn.print_ + "\", ISSN zotero: \"" + metadata_record->issn_ + "\""
                                 + ", PPN online: \"" + ppn.online_ + "\", PPN print: \"" + ppn.print_ + "\"");
    }

    // autodetect or map language
    AdjustLanguages(metadata_record, journal_params);

    // fetch creator GNDs and postprocess names
    for (auto &creator : metadata_record->creators_) {
        PostProcessAuthorName(&creator.first_name_, &creator.last_name_, &creator.title_, &creator.affix_,
                              metadata_record->languages_);

        if (not creator.last_name_.empty()) {
            std::string combined_name(creator.last_name_);
            if (not creator.first_name_.empty())
                combined_name += ", " + creator.first_name_;

            if (IsProperLastName(creator.last_name_)) {
                const std::string author_swb_lookup_url(GetSWBLookupURL(parameters));
                creator.gnd_number_ = HtmlUtil::StripHtmlTags(BSZUtil::GetAuthorGNDNumber(combined_name, author_swb_lookup_url));
                if (not creator.gnd_number_.empty())
                    LOG_DEBUG("added GND number " + creator.gnd_number_ + " for author " + combined_name + " (SWB lookup)");
            }
        }
    }

    // fill-in license and SSG values
    if (journal_params.license_ == "LF")
        metadata_record->license_ = journal_params.license_;
    else if (metadata_record->custom_metadata_.find("LF") != metadata_record->custom_metadata_.end())
        metadata_record->license_ = "LF";
    else
        metadata_record->license_ = "ZZ";
    // Skip SSG on selective evaluation so its is not automatically included in the FID stock
    if (not parameters.download_item_.journal_.selective_evaluation_)
        metadata_record->ssg_ = MetadataRecord::GetSSGTypeFromString(journal_params.ssgn_);

    DetectReviews(metadata_record, parameters);
    DetectNotes(metadata_record, parameters);
}


const ThreadSafeRegexMatcher CUSTOM_MARC_FIELD_PLACEHOLDER_MATCHER("%([^%]+)%");


void InsertCustomMarcFieldsForParams(const MetadataRecord &metadata_record, MARC::Record * const marc_record,
                                     const Config::MarcMetadataParams &marc_metadata_params)
{
    for (const auto &custom_field : marc_metadata_params.fields_to_add_) {
        if (unlikely(custom_field.length() < MARC::Record::TAG_LENGTH))
            LOG_ERROR("custom field's tag is too short: '" + custom_field + "'");

        // Determine which fields to add, depending on placeholders
        std::vector<std::string> fields_to_add;
        const auto placeholder_match(CUSTOM_MARC_FIELD_PLACEHOLDER_MATCHER.match(custom_field));
        if (not placeholder_match)
            fields_to_add.emplace_back(custom_field);
        else {
            if (placeholder_match.size() > 2)
                LOG_ERROR("only 1 placeholder allowed: " + custom_field);

            const std::string placeholder_full(placeholder_match[0]);
            const std::string placeholder_id(placeholder_match[1]);
            const auto substitutions(metadata_record.custom_metadata_.equal_range(placeholder_id));
            if (substitutions.first == metadata_record.custom_metadata_.end()) {
                LOG_DEBUG("custom field '" + custom_field + "' has missing values for placeholder '"
                          + placeholder_full + "'");
                continue;
            }

            for (auto iter(substitutions.first); iter != substitutions.second; ++iter) {
                // Make sure we fit into MARC binary field
                const unsigned max_content_length(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH -
                                         (custom_field.length() - (placeholder_id.length() + 2 /* for 2*'%' */)));
                if (iter->second.length() > max_content_length) {
                    std::string content_truncated(iter->second);
                    StringUtil::Truncate(max_content_length,  &content_truncated);
                    fields_to_add.emplace_back(StringUtil::ReplaceString(placeholder_full, content_truncated, custom_field));
                } else
                    fields_to_add.emplace_back(StringUtil::ReplaceString(placeholder_full, iter->second, custom_field));
            }
        }

        // Add fields
        const size_t MIN_CONTROL_FIELD_LENGTH(1);
        const size_t MIN_DATA_FIELD_LENGTH(2 /*indicators*/ + 1 /*subfield separator*/ + 1 /*subfield code*/ + 1 /*subfield value*/);

        for (const auto &field_to_add : fields_to_add) {
            const MARC::Tag tag(field_to_add.substr(0, MARC::Record::TAG_LENGTH));
            if ((tag.isTagOfControlField() and field_to_add.length() < MARC::Record::TAG_LENGTH + MIN_CONTROL_FIELD_LENGTH)
               or (not tag.isTagOfControlField() and field_to_add.length() < MARC::Record::TAG_LENGTH + MIN_DATA_FIELD_LENGTH))
            {
                LOG_ERROR("custom field '" + field_to_add + "' is too short");
            }
            marc_record->insertField(tag, field_to_add.substr(MARC::Record::TAG_LENGTH));
            LOG_DEBUG("inserted custom field '" + field_to_add + "'");
        }
    }
}


void InsertCustomMarcFields(const MetadataRecord &metadata_record, const ConversionParams &parameters,
                            MARC::Record * const marc_record)
{
    InsertCustomMarcFieldsForParams(metadata_record, marc_record, parameters.global_params_.marc_metadata_params_);
    InsertCustomMarcFieldsForParams(metadata_record, marc_record, parameters.group_params_.marc_metadata_params_);
    InsertCustomMarcFieldsForParams(metadata_record, marc_record, parameters.download_item_.journal_.marc_metadata_params_);
}


bool GetMatchedMARCFields(MARC::Record * marc_record, const std::string &field_or_field_and_subfield_code,
                          const ThreadSafeRegexMatcher &matcher, std::vector<MARC::Record::iterator> * const matched_fields)
{
    if (unlikely(field_or_field_and_subfield_code.length() < MARC::Record::TAG_LENGTH
                 or field_or_field_and_subfield_code.length() > MARC::Record::TAG_LENGTH + 1))
    {
        LOG_ERROR("\"field_or_field_and_subfield_code\" must be a tag or a tag plus a subfield code!");
    }

    const char subfield_code((field_or_field_and_subfield_code.length() == MARC::Record::TAG_LENGTH + 1) ?
                             field_or_field_and_subfield_code[MARC::Record::TAG_LENGTH] : '\0');

    matched_fields->clear();
    const MARC::Record::Range field_range(marc_record->getTagRange(field_or_field_and_subfield_code.substr(0,
                                          MARC::Record::TAG_LENGTH)));

    for (auto field_itr(field_range.begin()); field_itr != field_range.end(); ++field_itr) {
        const auto &field(*field_itr);
        if (subfield_code != '\0' and field.hasSubfield(subfield_code)) {
            if (matcher.match(field.getFirstSubfieldWithCode(subfield_code)))
                matched_fields->emplace_back(field_itr);
        } else if (matcher.match(field.getContents()))
            matched_fields->emplace_back(field_itr);
    }

    return not matched_fields->empty();
}


void RemoveCustomMarcFieldsForParams(MARC::Record * const marc_record, const Config::MarcMetadataParams &marc_metadata_params) {
    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : marc_metadata_params.fields_to_remove_) {
        const auto &tag_and_subfield_code(filter.first);
        if (marc_record->deleteFieldWithSubfieldCodeMatching(tag_and_subfield_code.substr(0,3),
                                                             tag_and_subfield_code[3], *filter.second.get()))
            LOG_DEBUG("erased field '" + tag_and_subfield_code + "' due to removal filter '" + filter.second->getPattern() + "'");
    }
}


void RemoveCustomMarcFields(MARC::Record * const marc_record, const ConversionParams &parameters) {
    RemoveCustomMarcFieldsForParams(marc_record, parameters.global_params_.marc_metadata_params_);
    RemoveCustomMarcFieldsForParams(marc_record, parameters.download_item_.journal_.marc_metadata_params_);
}


void RemoveCustomMarcSubfieldsForParams(MARC::Record * const marc_record, const Config::MarcMetadataParams &marc_metadata_params) {
    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : marc_metadata_params.subfields_to_remove_) {
        const auto &tag_and_subfield_code(filter.first);
        auto &matcher(*filter.second.get());
        GetMatchedMARCFields(marc_record, filter.first, matcher, &matched_fields);

        for (const auto &matched_field : matched_fields) {
            matched_field->removeSubfieldWithPattern(tag_and_subfield_code[3], matcher);
            LOG_DEBUG("erased subfield '" + tag_and_subfield_code + "' due to removal filter '" + filter.second->getPattern() + "'");
        }
    }
}


void RemoveCustomMarcSubfields(MARC::Record * const marc_record, const ConversionParams &parameters) {
    RemoveCustomMarcSubfieldsForParams(marc_record, parameters.global_params_.marc_metadata_params_);
    RemoveCustomMarcSubfieldsForParams(marc_record, parameters.download_item_.journal_.marc_metadata_params_);
}


void RewriteMarcFieldsForParams(MARC::Record * const marc_record, const Config::MarcMetadataParams &marc_metadata_params) {
    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : marc_metadata_params.rewrite_filters_) {
        const auto &tag_and_subfield_code(filter.first);
        auto &matcher(*filter.second.first.get());
        GetMatchedMARCFields(marc_record, filter.first, matcher, &matched_fields);

        for (const auto &matched_field : matched_fields) {
            char subfield_code(tag_and_subfield_code[3]);
            if (matched_field->hasSubfield(subfield_code)) {
                const std::string old_subfield_value(matched_field->getFirstSubfieldWithCode(subfield_code));
                const std::string replacement(filter.second.second);
                const std::string new_subfield_value(matcher.replaceWithBackreferences(old_subfield_value, replacement));
                matched_field->insertOrReplaceSubfield(subfield_code, new_subfield_value);
                LOG_DEBUG("Rewrote '" + tag_and_subfield_code + "' with content '" + old_subfield_value +
                          "' due to filter '" + matcher.getPattern() + "', replacement string '" + replacement
                          + "' and result '" + new_subfield_value + "'");
            }
        }
    }
}



void RewriteMarcFields(MARC::Record * const marc_record, const ConversionParams &parameters) {
    RewriteMarcFieldsForParams(marc_record, parameters.global_params_.marc_metadata_params_);
    RewriteMarcFieldsForParams(marc_record, parameters.download_item_.journal_.marc_metadata_params_);
}


// Zotero values see https://raw.githubusercontent.com/zotero/zotero/master/test/tests/data/allTypesAndFields.js
// MARC21 values see https://www.loc.gov/marc/relators/relaterm.html
const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP {
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


std::string TruncateAbstractField(const std::string &abstract_field) {
   const unsigned max_admissible_content_length(MARC::Record::MAX_VARIABLE_FIELD_DATA_LENGTH -
                                               (2 /* indicators */ + 1 /* separator */ + 1 /* subfield code */));
   return abstract_field.length() > max_admissible_content_length ?
          TextUtil::UTF8ByteTruncate(abstract_field, max_admissible_content_length - 3 /* trailing dots */) + "..." :
          abstract_field;
}


void GenerateMarcRecordFromMetadataRecord(const MetadataRecord &metadata_record, const ConversionParams &parameters,
                                          MARC::Record * const marc_record, std::string * const marc_record_hash)
{
    *marc_record = MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL,
                                MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART);

    // Control fields (001 depends on the hash of the record, so it's generated towards the end)
    marc_record->insertField("003", parameters.group_params_.isil_);

    if (metadata_record.superior_type_ == MetadataRecord::SuperiorType::ONLINE)
        marc_record->insertField("007", "cr|||||");
    else
        marc_record->insertField("007", "tu");

    // Authors/Creators
    // The first creator is always saved in the "100" field, all following creators go into the 700 field
    const auto zeder_instance(ZederInterop::GetZederInstanceForGroup(parameters.group_params_));
    unsigned num_creators_left(metadata_record.creators_.size());
    for (const auto &creator : metadata_record.creators_) {
        MARC::Subfields subfields;
        if (not creator.ppn_.empty())
            subfields.appendSubfield('0', "(DE-627)" + creator.ppn_);
        if (not creator.gnd_number_.empty())
            subfields.appendSubfield('0', "(DE-588)" + creator.gnd_number_);
        if (not creator.type_.empty()) {
            const auto creator_type_marc21(CREATOR_TYPES_TO_MARC21_MAP.find(creator.type_));
            if (creator_type_marc21 == CREATOR_TYPES_TO_MARC21_MAP.end())
                LOG_ERROR("zotero creator type '" + creator.type_ + "' could not be mapped to MARC21");

            subfields.appendSubfield('4', creator_type_marc21->second);
        }

        subfields.appendSubfield('a', StringUtil::Join(std::vector<std::string>({ creator.last_name_, creator.first_name_ }),
                                 ", "));

        if (not creator.affix_.empty())
            subfields.appendSubfield('b', creator.affix_ + ".");
        if (not creator.title_.empty())
            subfields.appendSubfield('c', creator.title_);
        subfields.appendSubfield('e', "VerfasserIn");

        if (num_creators_left == metadata_record.creators_.size())
            marc_record->insertFieldAtEnd("100", subfields, /* indicator 1 = */'1');
        else
            marc_record->insertFieldAtEnd("700", subfields, /* indicator 1 = */'1');

        if (not creator.ppn_.empty() or not creator.gnd_number_.empty()) {
            const std::string _887_data("Autor in der Zoterovorlage [" + creator.last_name_ + ", "
                                        + creator.first_name_ + "] maschinell zugeordnet");
            const std::string _subfield2_value(zeder_instance == Zeder::Flavour::IXTHEO ? "ixzom" : "krzom");
            marc_record->insertFieldAtEnd("887", { { 'a', _887_data }, { '2', _subfield2_value } });
        }

        --num_creators_left;
    }

    // RDA
    marc_record->insertField("040", { { 'a', "DE-627" }, { 'b', "ger" }, { 'c', "DE-627" }, { 'e', "rda" }, });

    // Title
    if (metadata_record.title_.empty())
        throw std::runtime_error("no title provided for download item from URL " + parameters.download_item_.url_.toString());
    else
        marc_record->insertField("245", { { 'a', metadata_record.title_ } }, /* indicator 1 = */'0', /* indicator 2 = */'0');

    // Languages
    for (const auto &language : metadata_record.languages_)
        marc_record->insertFieldAtEnd("041", { { 'a', language } });

    // Abstract Note
    if (not metadata_record.abstract_note_.empty())
        marc_record->insertField("520", { { 'a', TruncateAbstractField(metadata_record.abstract_note_) } });

    // Date & Year
    const auto &date(metadata_record.date_);
    const auto &item_type(metadata_record.item_type_);
    if (not date.empty() and item_type != "journalArticle" and item_type != "review")
        marc_record->insertField("362", { { 'a', date } });

    unsigned year_num(0);
    std::string year;
    if (TimeUtil::StringToYear(date, &year_num))
        year = std::to_string(year_num);
    else
        year = TimeUtil::GetCurrentYear();

    marc_record->insertField("264", { { 'c', year } });

    // URL
    if (not metadata_record.url_.empty()) {
        MARC::Subfields subfields({ { 'u', metadata_record.url_ } });
        if (not metadata_record.license_.empty())
            subfields.appendSubfield('z', metadata_record.license_);
        marc_record->insertField("856", subfields, /* indicator1 = */'4', /* indicator2 = */'0');
    }

    // DOI
    const auto &doi(metadata_record.doi_);
    if (not doi.empty()) {
        marc_record->insertField("024", { { 'a', doi }, { '2', "doi" } }, '7');
        const std::string doi_url("https://doi.org/" + doi);
        if (doi_url != metadata_record.url_) {
            MARC::Subfields subfields({ { 'u', doi_url } });
            if (not metadata_record.license_.empty())
                subfields.appendSubfield('z', metadata_record.license_);
            marc_record->insertField("856", subfields, /* indicator1 = */'4', /* indicator2 = */'0');
        }
    }

    // Review-specific modifications
    if (item_type == "review") {
        marc_record->insertField("655", { { 'a', "Rezension" }, { '0', "(DE-588)4049712-4" },
                                 { '0', "(DE-627)106186019" }, { '2', "gnd-content" } },
                                 /* indicator1 = */' ', /* indicator2 = */'7');
    }

    if (item_type == "note")
        marc_record->insertField("NOT", { { 'a', "1" } });


    // Differentiating information about source (see BSZ Konkordanz MARC 936)
    MARC::Subfields _936_subfields;
    const auto &volume(metadata_record.volume_);
    const auto &issue(metadata_record.issue_);
    if (not volume.empty()) {
        _936_subfields.appendSubfield('d', volume);
        if (not issue.empty())
            _936_subfields.appendSubfield('e', issue);
    } else if (not issue.empty())
        _936_subfields.appendSubfield('d', issue);

    const std::string pages(metadata_record.pages_);
    static const std::string ARTICLE_NUM_INDICATOR("article");
    if (not pages.empty()) {
        if (StringUtil::StartsWith(pages, ARTICLE_NUM_INDICATOR)) {
            _936_subfields.appendSubfield('i', StringUtil::TrimWhite(pages.substr(ARTICLE_NUM_INDICATOR.length())));
        } else
           _936_subfields.appendSubfield('h', pages);
    }

    _936_subfields.appendSubfield('j', year);
    if (not _936_subfields.empty())
        marc_record->insertField("936", _936_subfields, 'u', 'w');

    // Information about superior work (See BSZ Konkordanz MARC 773)
    MARC::Subfields _773_subfields;
    const std::string publication_title(metadata_record.publication_title_);
    if (not publication_title.empty()) {
        _773_subfields.appendSubfield('i', "In: ");
        _773_subfields.appendSubfield('t', publication_title);
    }
    if (not metadata_record.issn_.empty())
        _773_subfields.appendSubfield('x', metadata_record.issn_);
    if (not metadata_record.superior_ppn_.empty())
        _773_subfields.appendSubfield('w', "(DE-627)" + metadata_record.superior_ppn_);

    // 773g, example: "52 (2018), 1, Seite 1-40" => <volume>(<year>), <issue>, S. <pages>
    const bool _773_subfields_iaxw_present(not _773_subfields.empty());
    bool _773_subfield_g_present(false);
    std::string g_content;
    if (not volume.empty()) {
        g_content += volume + " (" + year + ")";
        if (not issue.empty())
            g_content += ", " + issue;

        if (not pages.empty()) {
            if (StringUtil::StartsWith(pages, ARTICLE_NUM_INDICATOR))
                g_content += ", " + StringUtil::ReplaceString(ARTICLE_NUM_INDICATOR, "Artikel", pages);
            else
                g_content += ", Seite " + pages;
        }

        _773_subfields.appendSubfield('g', g_content);
        _773_subfield_g_present = true;
    }

    if (_773_subfields_iaxw_present and _773_subfield_g_present)
        marc_record->insertField("773", _773_subfields, '0', '8');
    else
        marc_record->insertField("773", _773_subfields);

    // Keywords
    for (const auto &keyword : metadata_record.keywords_) {
        const std::string normalized_keyword(TextUtil::CollapseAndTrimWhitespace(keyword));
        if (Is655Keyword(normalized_keyword))
            marc_record->insertField("655", 'a', normalized_keyword, ' ', '4');
        else
            marc_record->insertField(MARC::GetIndexField(normalized_keyword));
    }

    // SSG numbers
    if (metadata_record.ssg_ != MetadataRecord::SSGType::INVALID) {
        MARC::Subfields _084_subfields;
        switch(metadata_record.ssg_) {
        case MetadataRecord::SSGType::FG_0:
            _084_subfields.appendSubfield('a', "0");
            break;
        case MetadataRecord::SSGType::FG_1:
            _084_subfields.appendSubfield('a', "1");
            break;
        case MetadataRecord::SSGType::FG_01:
            _084_subfields.appendSubfield('a', "0");
            _084_subfields.appendSubfield('a', "1");
            break;
        case MetadataRecord::SSGType::FG_21:
            _084_subfields.appendSubfield('a', "2,1");
            break;
        default:
            break;
        }
        _084_subfields.appendSubfield('2', "ssgn");
        marc_record->insertField("084", _084_subfields);
    }

    // Abrufzeichen und ISIL
    // Do not include subject links on selective evaluation to prevent automatic inclusion in the FID stock
    if (zeder_instance == Zeder::Flavour::IXTHEO) {
        if (not parameters.download_item_.journal_.selective_evaluation_)
            marc_record->insertFieldAtEnd("935", { { 'a', "mteo" } });
        marc_record->insertFieldAtEnd("935", { { 'a', "ixzs" }, { '2', "LOK" } });
    } else if ((zeder_instance == Zeder::Flavour::KRIMDOK) and not parameters.download_item_.journal_.selective_evaluation_) {
        marc_record->insertFieldAtEnd("935", { { 'a', "mkri" } });
    }
    marc_record->insertField("852", { { 'a', parameters.group_params_.isil_ } });

    // Zotero sigil
    marc_record->insertFieldAtEnd("935", { { 'a', "zota" }, { '2', "LOK" } });

    // Selective evaluation
    if (parameters.download_item_.journal_.selective_evaluation_)
        marc_record->insertFieldAtEnd("935", { { 'a', "NABZ" }, { '2', "LOK" } });

    // Book-keeping fields
    if (not metadata_record.url_.empty())
        marc_record->insertField("URL", { { 'a', metadata_record.url_ } });
    else
        marc_record->insertField("URL", { { 'a', parameters.download_item_.url_.toString() } });
    marc_record->insertField("ZID", { { 'a', std::to_string(parameters.download_item_.journal_.zeder_id_) },
                                      { 'b', StringUtil::ASCIIToLower(Zeder::FLAVOUR_TO_STRING_MAP.at(zeder_instance)) } });
    marc_record->insertField("JOU", { { 'a', parameters.download_item_.journal_.name_ } });

    // Add custom fields
    InsertCustomMarcFields(metadata_record, parameters, marc_record);

    // Remove fields
    RemoveCustomMarcFields(marc_record, parameters);

    // Remove subfields from specific field
    RemoveCustomMarcSubfields(marc_record, parameters);

    // Rewrite fields
    RewriteMarcFields(marc_record, parameters);

    // Has to be generated in the very end as it contains the hash of the record
    *marc_record_hash = CalculateMarcRecordHash(*marc_record);
    marc_record->insertField("001", parameters.group_params_.name_ + "#" + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d")
                             + "#" + *marc_record_hash);
}


bool MarcRecordMatchesExclusionFiltersForParams(MARC::Record * const marc_record, const Util::HarvestableItem &download_item,
                                                const Config::MarcMetadataParams &marc_metadata_params)
{
    bool found_match(false);
    std::string exclusion_string;

    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : marc_metadata_params.exclusion_filters_) {
        if (GetMatchedMARCFields(marc_record, filter.first, *filter.second.get(), &matched_fields)) {
            exclusion_string = filter.first + "/" + filter.second->getPattern() + "/";
            found_match = true;
            break;
        }
    }

    if (found_match)
        LOG_INFO("MARC field for '" + download_item.url_.toString() + " matched exclusion filter (" + exclusion_string + ")");
    return found_match;
}


bool MarcRecordMatchesExclusionFilters(const Util::HarvestableItem &download_item, const Config::GlobalParams &global_params,
                                       MARC::Record * const marc_record)
{
    return (MarcRecordMatchesExclusionFiltersForParams(marc_record, download_item, global_params.marc_metadata_params_) or
            MarcRecordMatchesExclusionFiltersForParams(marc_record, download_item, download_item.journal_.marc_metadata_params_));
}


const std::set<MARC::Tag> EXCLUDED_FIELDS_DURING_CHECKSUM_CALC{
    "001", "URL", "ZID", "JOU",
};


std::string CalculateMarcRecordHash(const MARC::Record &marc_record) {
    return StringUtil::ToHexString(MARC::CalcChecksum(marc_record, EXCLUDED_FIELDS_DURING_CHECKSUM_CALC));
}


const std::vector<std::string> UNDESIRED_ITEM_TYPES{
    "webpage"
};


static bool ExcludeUndesiredItemTypes(const MetadataRecord &metadata_record) {
    if (std::find(UNDESIRED_ITEM_TYPES.begin(),
                  UNDESIRED_ITEM_TYPES.end(),
                  metadata_record.item_type_) != UNDESIRED_ITEM_TYPES.end())
    {
        LOG_DEBUG("Skipping: undesired item type \"" + metadata_record.item_type_ + "\"");
        return true;
    }

    return false;
}


const std::vector<std::string> VALID_ITEM_TYPES_FOR_ONLINE_FIRST{
    "journalArticle", "magazineArticle", "review"
};


bool ExcludeOnlineFirstRecord(const MetadataRecord &metadata_record, const ConversionParams &parameters) {
    if (std::find(VALID_ITEM_TYPES_FOR_ONLINE_FIRST.begin(),
                  VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end(),
                  metadata_record.item_type_) == VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end())
    {
        return false;
    }

    if (metadata_record.issue_.empty() and metadata_record.volume_.empty()) {
        if (parameters.global_params_.skip_online_first_articles_unconditionally_) {
            LOG_DEBUG("Skipping: online-first article unconditionally");
            return true;
        } else if (metadata_record.doi_.empty()) {
            LOG_DEBUG("Skipping: online-first article without a DOI");
            return true;
        }
    }

    return false;
}


bool ExcludeEarlyViewRecord(const MetadataRecord &metadata_record, const ConversionParams &/*unused*/) {
    if (std::find(VALID_ITEM_TYPES_FOR_ONLINE_FIRST.begin(),
                  VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end(),
                  metadata_record.item_type_) == VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end())
    {
        return false;
    }

    if (metadata_record.issue_ == "n/a" or metadata_record.volume_ == "n/a") {
        LOG_DEBUG("Skipping: early-view article");
        return true;
    }

    return false;
}


void ConversionTasklet::run(const ConversionParams &parameters, ConversionResult * const result) {
    LOG_INFO("Converting item " + parameters.download_item_.toString());

    std::shared_ptr<JSON::JSONNode> tree_root;
    JSON::Parser json_parser(parameters.json_metadata_);

    if (not json_parser.parse(&tree_root)) {
        LOG_WARNING("failed to parse JSON: " + json_parser.getErrorMessage());
        return;
    }

    const auto &download_item(parameters.download_item_);
    auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));
    PostprocessTranslationServerResponse(parameters, &array_node);

    if (array_node->size() == 0) {
        LOG_WARNING("no items found in translation server response");
        LOG_WARNING("JSON response:\n" + parameters.json_metadata_);
        return;
    }

    for (const auto &entry : *array_node) {
        const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));

        try {
            if (ZoteroItemMatchesExclusionFilters(json_object, parameters)) {
                ++result->num_skipped_since_exclusion_filters_;
                continue;
            }

            MetadataRecord new_metadata_record;
            ConvertZoteroItemToMetadataRecord(json_object, &new_metadata_record);

            if (ExcludeUndesiredItemTypes(new_metadata_record)) {
                LOG_WARNING("Skipping record with URL " +
                            (new_metadata_record.url_.empty() ? new_metadata_record.url_ : "[Unknown]") +
                            " because it is an undesired item type");
                ++result->num_skipped_since_undesired_item_type_;
                continue;
            }

            AugmentMetadataRecord(&new_metadata_record, parameters);

            LOG_DEBUG("Augmented metadata record: " + new_metadata_record.toString());
            if (new_metadata_record.url_.empty() and new_metadata_record.doi_.empty())
                throw std::runtime_error("no URL set");

            if (ExcludeOnlineFirstRecord(new_metadata_record, parameters)) {
                ++result->num_skipped_since_online_first_;
                continue;
            } else if (ExcludeEarlyViewRecord(new_metadata_record, parameters)) {
                ++result->num_skipped_since_early_view_;
                continue;
            }

            // a dummy record that will be replaced subsequently
            std::unique_ptr<MARC::Record> new_marc_record(new MARC::Record(std::string(MARC::Record::LEADER_LENGTH, ' ')));
            std::string new_marc_record_hash;
            GenerateMarcRecordFromMetadataRecord(new_metadata_record, parameters, new_marc_record.get(), &new_marc_record_hash);

            if (MarcRecordMatchesExclusionFilters(download_item, parameters.global_params_, new_marc_record.get())) {
                ++result->num_skipped_since_exclusion_filters_;
                continue;
            }

            result->marc_records_.emplace_back(new_marc_record.release());
            LOG_INFO("Generated record with hash '" + new_marc_record_hash + "'\n");
        } catch (const std::exception &x) {
            LOG_WARNING("couldn't convert record: " + std::string(x.what()));
        }
    }

    LOG_INFO("Conversion complete");
}


ConversionTasklet::ConversionTasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const instance_counter,
                                     std::unique_ptr<ConversionParams> parameters)
 : Util::Tasklet<ConversionParams, ConversionResult>(instance_counter, parameters->download_item_,
                                                     "Conversion: " + parameters->download_item_.url_.toString(),
                                                     std::bind(&ConversionTasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                                     std::unique_ptr<ConversionResult>(new ConversionResult()),
                                                     std::move(parameters), ResultPolicy::YIELD) {}



void *ConversionManager::BackgroundThreadRoutine(void * parameter) {
    static const unsigned BACKGROUND_THREAD_SLEEP_TIME(16 * 1000);   // ms -> us

    ConversionManager * const conversion_manager(reinterpret_cast<ConversionManager *>(parameter));

    while (not conversion_manager->stop_background_thread_.load()) {
        conversion_manager->processQueue();
        conversion_manager->cleanupCompletedTasklets();

        ::usleep(BACKGROUND_THREAD_SLEEP_TIME);
    }

    pthread_exit(nullptr);
}


void ConversionManager::processQueue() {
    if (conversion_tasklet_execution_counter_ == MAX_CONVERSION_TASKLETS)
        return;

    std::lock_guard<std::mutex> conversion_queue_lock(conversion_queue_mutex_);
    while (not conversion_queue_.empty()
           and conversion_tasklet_execution_counter_ < MAX_CONVERSION_TASKLETS)
    {
        std::shared_ptr<ConversionTasklet> tasklet(conversion_queue_.front());
        active_conversions_.emplace_back(tasklet);
        conversion_queue_.pop_front();
        tasklet->start();
    }
}


void ConversionManager::cleanupCompletedTasklets() {
    for (auto iter(active_conversions_.begin()); iter != active_conversions_.end();) {
        if ((*iter)->isComplete()) {
            iter = active_conversions_.erase(iter);
            continue;
        }
        ++iter;
    }
}


ConversionManager::ConversionManager(const Config::GlobalParams &global_params)
 : global_params_(global_params), stop_background_thread_(false)
{
    if (::pthread_create(&background_thread_, nullptr, BackgroundThreadRoutine, this) != 0)
        LOG_ERROR("background conversion manager thread creation failed!");
}


ConversionManager::~ConversionManager() {
    stop_background_thread_.store(true);
    const auto retcode(::pthread_join(background_thread_, nullptr));
    if (retcode != 0)
        LOG_WARNING("couldn't join with the conversion manager background thread! result = " + std::to_string(retcode));

    active_conversions_.clear();
    conversion_queue_.clear();
}


std::unique_ptr<Util::Future<ConversionParams, ConversionResult>> ConversionManager::convert(const Util::HarvestableItem &source,
                                                                                             const std::string &json_metadata,
                                                                                             const Config::GroupParams &group_params,
                                                                                             const Config::SubgroupParams &subgroup_params)
{
    std::unique_ptr<ConversionParams> parameters(new ConversionParams(source, json_metadata,
                                                 global_params_, group_params, subgroup_params));
    std::shared_ptr<ConversionTasklet> new_tasklet(new ConversionTasklet(&conversion_tasklet_execution_counter_,
                                                   std::move(parameters)));

    {
        std::lock_guard<std::mutex> conversion_queue_lock(conversion_queue_mutex_);
        conversion_queue_.emplace_back(new_tasklet);
    }

    std::unique_ptr<Util::Future<ConversionParams, ConversionResult>>
        conversion_result(new Util::Future<ConversionParams, ConversionResult>(new_tasklet));
    return conversion_result;
}


} // end namespace Conversion


} // end namespace ZoteroHarvester
