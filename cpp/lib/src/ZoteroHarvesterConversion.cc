/** \brief Classes related to the Zotero Harvester's JSON-to-MARC conversion API
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "Downloader.h"
#include "FileUtil.h"
#include "LobidUtil.h"
#include "NGram.h"
#include "StlHelpers.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "ZoteroHarvesterConversion.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Conversion {


void SuppressJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const Util::Harvestable &download_item)
{
    const auto suppression_regex(download_item.journal_.zotero_metadata_params_.fields_to_suppress_.find(node_name));
    if (suppression_regex != download_item.journal_.zotero_metadata_params_.fields_to_suppress_.end()) {
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


void OverrideJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                          const Util::Harvestable &download_item)
{
    const std::string ORIGINAL_VALUE_SPECIFIER("%org%");
    const auto override_pattern(download_item.journal_.zotero_metadata_params_.fields_to_override_.find(node_name));

    if (override_pattern != download_item.journal_.zotero_metadata_params_.fields_to_override_.end()) {
        if (node->getType() != JSON::JSONNode::STRING_NODE)
            LOG_ERROR("metadata override has invalid node type '" + node_name + "'");

        const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
        const auto string_value(string_node->getValue());
        const auto override_string(StringUtil::ReplaceString(ORIGINAL_VALUE_SPECIFIER, string_value,override_pattern->second));

        LOG_DEBUG("metadata field '" + node_name + "' value changed from '" + string_value + "' to '" + override_string + "'");
        string_node->setValue(override_string);
    }
}


void PostprocessTranslationServerResponse(const Util::Harvestable &download_item,
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
        JSON::VisitLeafNodes("root", json_object, SuppressJsonMetadata, std::ref(download_item));
        JSON::VisitLeafNodes("root", json_object, OverrideJsonMetadata, std::ref(download_item));
    }
}


bool ZoteroItemMatchesExclusionFilters(const Util::Harvestable &download_item,
                                       const std::shared_ptr<JSON::ObjectNode> &zotero_item)
{
    if (download_item.journal_.zotero_metadata_params_.exclusion_filters_.empty())
        return false;

    bool found_match(false);
    std::string exclusion_string;
    auto metadata_exclusion_predicate = [&found_match, &exclusion_string]
                                        (const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
                                         const Config::JournalParams &journal_params) -> void
    {
        const auto filter_regex(journal_params.zotero_metadata_params_.exclusion_filters_.find(node_name));
        if (filter_regex != journal_params.zotero_metadata_params_.exclusion_filters_.end()) {
            if (node->getType() != JSON::JSONNode::STRING_NODE)
                LOG_ERROR("metadata exclusion filter has invalid node type '" + node_name + "'");

            const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
            if (filter_regex->second->match(string_node->getValue())) {
                found_match = true;
                exclusion_string = node_name + "/" + filter_regex->second->getPattern() + "/";
            }
        }
    };

    JSON::VisitLeafNodes("root", zotero_item, metadata_exclusion_predicate, std::ref(download_item.journal_));
    if (found_match)
        LOG_INFO("zotero metadata for '" + download_item.url_.toString() + " matched exclusion filter (" + exclusion_string + ")");

    return found_match;
}


void ConvertZoteroItemToMetadataRecord(const std::shared_ptr<JSON::ObjectNode> &zotero_item,
                                       MetadataRecord * const metadata_record)
{
    metadata_record->item_type_ = zotero_item->getStringValue("itemType");
    metadata_record->title_ = zotero_item->getOptionalStringValue("title");
    metadata_record->short_title_ = zotero_item->getOptionalStringValue("shortTitle");
    metadata_record->abstract_note_ = zotero_item->getOptionalStringValue("abstractNote");
    metadata_record->publication_title_ = zotero_item->getOptionalStringValue("publicationTitle");
    metadata_record->volume_ = zotero_item->getOptionalStringValue("volume");
    metadata_record->issue_ = zotero_item->getOptionalStringValue("issue");
    metadata_record->pages_ = zotero_item->getOptionalStringValue("pages");
    metadata_record->date_ = zotero_item->getOptionalStringValue("date");
    metadata_record->doi_ = zotero_item->getOptionalStringValue("DOI");
    metadata_record->language_ = zotero_item->getOptionalStringValue("language");
    metadata_record->url_ = zotero_item->getOptionalStringValue("url");
    metadata_record->issn_ = zotero_item->getOptionalStringValue("ISSN");

    const auto creators_array(zotero_item->getOptionalArrayNode("creators"));
    if (creators_array) {
        for (const auto &entry :*creators_array) {
            const auto creator_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", entry));
            metadata_record->creators_.emplace_back(creator_object->getOptionalStringValue("firstName"),
                                                    creator_object->getOptionalStringValue("lastName"),
                                                    creator_object->getOptionalStringValue("creatorType"));
        }
    }

    const auto tags_array(zotero_item->getOptionalArrayNode("tags"));
    if (tags_array) {
        for (const auto &entry :*tags_array) {
            const auto tag_object(JSON::JSONNode::CastToObjectNodeOrDie("array_element", entry));
            const auto tag(tag_object->getOptionalStringValue("tag"));
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

                metadata_record->custom_metadata_[note.substr(0, first_colon_pos)] = note.substr(first_colon_pos + 1);
            }
        }
    }
}


std::unordered_map<std::string, std::string> fetch_author_ppn_url_to_ppn_cache;
std::mutex fetch_author_ppn_url_to_ppn_cache_mutex;


const ThreadSafeRegexMatcher AUTHOR_PPN_MATCHER("<SMALL>PPN</SMALL>.*<div><SMALL>([0-9X]+)");


std::string FetchAuthorPPN(const std::string &author, const std::string &author_lookup_base_url) {
    // "author" must be in the lastname,firstname format
    const std::string lookup_url(author_lookup_base_url + UrlUtil::UrlEncode(author));
    {
        std::lock_guard<std::mutex> lock(fetch_author_ppn_url_to_ppn_cache_mutex);
        const auto match(fetch_author_ppn_url_to_ppn_cache.find(lookup_url));
        if (match != fetch_author_ppn_url_to_ppn_cache.end())
            return match->second;
    }

    Downloader downloader(lookup_url);
    if (downloader.anErrorOccurred()) {
        LOG_WARNING("couldn't download author PPN results! downloader error: " + downloader.getLastErrorMessage());
        return "";
    }

    const auto match(AUTHOR_PPN_MATCHER.match(downloader.getMessageBody()));
    if (match) {
         std::lock_guard<std::mutex> lock(fetch_author_ppn_url_to_ppn_cache_mutex);
         fetch_author_ppn_url_to_ppn_cache.emplace(lookup_url, match[1]);
         return match[1];
    }

    return "";
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


const std::string AUTHOR_NAME_BLACKLIST(UBTools::GetTuelibPath() + "zotero_author_name_blacklist.txt");


ThreadSafeRegexMatcher InitializeBlacklistedAuthorTokenMatcher() {
    std::unordered_set<std::string> blacklisted_tokens, filtered_blacklisted_tokens;
    auto string_data(FileUtil::ReadStringOrDie(AUTHOR_NAME_BLACKLIST));
    StringUtil::Split(string_data, '\n', &blacklisted_tokens, /* suppress_empty_components = */true);

    StlHelpers::Functional::Filter(blacklisted_tokens.begin(), blacklisted_tokens.end(), filtered_blacklisted_tokens,
                                   FilterEmptyAndCommentLines);

    std::string match_pattern("\\b(");
    for (auto blacklisted_token : filtered_blacklisted_tokens) {
        // escape backslashes first to keep from overwriting other escape sequences
        blacklisted_token = StringUtil::ReplaceString("\\", "\\\\", blacklisted_token);
        blacklisted_token = StringUtil::ReplaceString("(", "\\(", blacklisted_token);
        blacklisted_token = StringUtil::ReplaceString(")", "\\)", blacklisted_token);
        blacklisted_token = StringUtil::ReplaceString(".", "\\.", blacklisted_token);
        blacklisted_token = StringUtil::ReplaceString("/", "\\/", blacklisted_token);

        match_pattern += blacklisted_token + "|";
    }
    match_pattern += ")\\b";

   return ThreadSafeRegexMatcher(match_pattern);
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


bool IsAuthorNameTokenTitle(std::string token) {
    static const std::set<std::string> VALID_TITLES {
        "jr", "sr", "sj", "s.j", "fr", "hr", "dr", "prof", "em"
    };

    bool final_period(token.back() == '.');
    if (final_period)
        token.erase(token.size() - 1);

    TextUtil::UTF8ToLower(&token);
    return VALID_TITLES.find(token) != VALID_TITLES.end();
}


bool IsAuthorNameTokenAffix(std::string token) {
    static const std::set<std::string> VALID_AFFIXES {
        "i", "ii", "iii", "iv", "v"
    };

    TextUtil::UTF8ToLower(&token);
    return VALID_AFFIXES.find(token) != VALID_AFFIXES.end();
}


void PostProcessAuthorName(std::string * const first_name, std::string * const last_name, std::string * const title,
                           std::string * const affix)
{
    std::string first_name_buffer, title_buffer;
    std::vector<std::string> tokens;

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


void IdentifyMissingLanguage(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params) {
    const unsigned minimum_token_count(5);

    if (journal_params.language_params_.expected_languages_.empty())
        return;

    if (journal_params.language_params_.expected_languages_.size() == 1) {
        metadata_record->language_ = *journal_params.language_params_.expected_languages_.begin();
        LOG_DEBUG("language set to default language '" + metadata_record->language_ + "'");
        return;
    }

    // attempt to automatically detect the language
    std::vector<std::string> top_languages;
    std::string record_text;

    if (journal_params.language_params_.source_text_fields_.empty()
        or journal_params.language_params_.source_text_fields_ == "title")
    {
        record_text = metadata_record->title_;
        // use naive tokenization to count tokens in the title
        // additionally use abstract if we have too few tokens in the title
        if (StringUtil::CharCount(record_text, ' ') < minimum_token_count) {
            record_text += " " + metadata_record->abstract_note_;
            LOG_DEBUG("too few tokens in title. applying heuristic on the abstract as well");
        }
    } else if (journal_params.language_params_.source_text_fields_ == "abstract")
        record_text = metadata_record->abstract_note_;
    else if (journal_params.language_params_.source_text_fields_ == "title+abstract")
        record_text = metadata_record->title_ + " " + metadata_record->abstract_note_;
    else
        LOG_ERROR("unknown text field '" + journal_params.language_params_.source_text_fields_ + "' for language detection");

    NGram::ClassifyLanguage(record_text, &top_languages, journal_params.language_params_.expected_languages_,
                            NGram::DEFAULT_NGRAM_NUMBER_THRESHOLD);
    metadata_record->language_ = top_languages.front();
    LOG_INFO("automatically detected language to be '" + metadata_record->language_ + "'");
}


MetadataRecord::SSGType GetSSGTypeFromString(const std::string &ssg_string) {
    const std::map<std::string, MetadataRecord::SSGType> ZEDER_STRINGS {
        { "FG_0", MetadataRecord::SSGType::FG_0 },
        { "FG_1", MetadataRecord::SSGType::FG_1 },
        { "FG_0/1", MetadataRecord::SSGType::FG_01 },
        { "FG_2,1", MetadataRecord::SSGType::FG_21 },
    };

    if (ZEDER_STRINGS.find(ssg_string) != ZEDER_STRINGS.end())
        return ZEDER_STRINGS.find(ssg_string)->second;

    return MetadataRecord::SSGType::INVALID;
}


const ThreadSafeRegexMatcher PAGE_RANGE_MATCHER("^(.+)-(.+)$");
const ThreadSafeRegexMatcher PAGE_RANGE_DIGIT_MATCHER("^(\\d+)-(\\d+)$");


void AugmentMetadataRecord(MetadataRecord * const metadata_record, const Config::JournalParams &journal_params,
                           const Config::GroupParams &group_params, const Config::EnhancementMaps &enhancement_maps)
{
    // normalise date
    struct tm tm(TimeUtil::StringToStructTm(metadata_record->date_, journal_params.strptime_format_string_));
    const std::string date_normalized(std::to_string(tm.tm_year + 1900) + "-"
                                      + StringUtil::ToString(tm.tm_mon + 1, 10, 2, '0') + "-"
                                      + StringUtil::ToString(tm.tm_mday, 10, 2, '0'));
    metadata_record->date_ = date_normalized;

    // normalise issue/volume
    if (metadata_record->issue_ == "0")
        metadata_record->issue_ = "";
    if (metadata_record->volume_ == "0")
        metadata_record->volume_ = "";

    // normalise pages
    const auto pages(metadata_record->pages_);
    // force uppercase for roman numeral detection
    auto page_match(PAGE_RANGE_MATCHER.match(StringUtil::ToUpper(pages)));
    if (page_match) {
        std::string converted_pages;
        if (TextUtil::IsRomanNumeral(page_match[1]))
            converted_pages += std::to_string(StringUtil::RomanNumeralToDecimal(page_match[1]));
        else
            converted_pages += page_match[1];

        converted_pages += "-";

        if (TextUtil::IsRomanNumeral(page_match[2]))
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

    // override ISSN (online > print > zotero) and select superior PPN (online > print)
    const auto &issn(journal_params.issn_);
    const auto &ppn(journal_params.ppn_);
    if (not issn.online_.empty()) {
        if (ppn.online_.empty())
            LOG_ERROR("cannot use online ISSN \"" + issn.online_ + "\" because no online PPN is given!");
        metadata_record->issn_ = issn.online_;
        metadata_record->superior_ppn_ = ppn.online_;
        metadata_record->superior_type_ = MetadataRecord::SuperiorType::ONLINE;

        LOG_DEBUG("use online ISSN \"" + issn.online_ + "\" with online PPN \"" + ppn.online_ + "\"");
    } else if (not issn.print_.empty()) {
        if (ppn.print_.empty())
            LOG_ERROR("cannot use print ISSN \"" + issn.print_ + "\" because no print PPN is given!");
        metadata_record->issn_ = issn.print_;
        metadata_record->superior_ppn_ = ppn.print_;
        metadata_record->superior_type_ = MetadataRecord::SuperiorType::PRINT;

        LOG_DEBUG("use print ISSN \"" + issn.print_ + "\" with print PPN \"" + ppn.print_ + "\"");
    } else {
        LOG_ERROR("ISSN and PPN could not be chosen! ISSN online: \"" + issn.online_ + "\""
                  + ", ISSN print: \"" + issn.print_ + "\", ISSN zotero: \"" + metadata_record->issn_ + "\""
                  + ", PPN online: \"" + ppn.online_ + "\", PPN print: \"" + ppn.print_ + "\"");
    }

    // fetch creator PPNs/GNDs and postprocess names
    for (auto &creator : metadata_record->creators_) {
        PostProcessAuthorName(&creator.first_name_, &creator.last_name_, &creator.title_, &creator.affix_);

        if (not creator.last_name_.empty()) {
            std::string combined_name(creator.last_name_);
            if (not creator.first_name_.empty())
                combined_name += ", " + creator.first_name_;

            creator.ppn_ = FetchAuthorPPN(combined_name, group_params.author_ppn_lookup_url_);
            if (not creator.ppn_.empty())
                LOG_DEBUG("added PPN " + creator.ppn_ + " for author " + combined_name);

            creator.gnd_number_ = LobidUtil::GetAuthorGNDNumber(combined_name, group_params.author_gnd_lookup_query_params_);
            if (not creator.gnd_number_.empty())
                LOG_DEBUG("added GND number " + creator.gnd_number_ + " for author " + combined_name);
        }
    }

    // identify language
    if (metadata_record->language_.empty() or journal_params.language_params_.force_automatic_language_detection_) {
        if (journal_params.language_params_.force_automatic_language_detection_)
            LOG_DEBUG("forcing automatic language detection");

        IdentifyMissingLanguage(metadata_record, journal_params);
    } else if (journal_params.language_params_.expected_languages_.size() == 1
               and *journal_params.language_params_.expected_languages_.begin() != metadata_record->language_)
    {
        LOG_WARNING("expected language '" + *journal_params.language_params_.expected_languages_.begin() + "' but found '"
                    + metadata_record->language_ + "'");
    }

    // fill-in ISIL, license and SSG values
    metadata_record->license_ = enhancement_maps.lookupLicense(metadata_record->issn_);
    metadata_record->ssg_ = GetSSGTypeFromString(enhancement_maps.lookupSSG(metadata_record->issn_));

    // tag reviews
    const auto &review_matcher(journal_params.review_regex_);
    if (review_matcher != nullptr) {
        if (review_matcher->match(metadata_record->title_)) {
            LOG_DEBUG("title matched review pattern");
            metadata_record->item_type_ = "review";
        } else if (review_matcher->match(metadata_record->short_title_)) {
            LOG_DEBUG("short title matched review pattern");
            metadata_record->item_type_ = "review";
        } else if (review_matcher->match(metadata_record->abstract_note_)) {
            LOG_DEBUG("abstract matched review pattern");
            metadata_record->item_type_ = "review";
        } else for (const auto &keyword : metadata_record->keywords_) {
            if (review_matcher->match(keyword)) {
                LOG_DEBUG("keyword matched review pattern");
                metadata_record->item_type_ = "review";
            }
        }
    }
}


const ThreadSafeRegexMatcher CUSTOM_MARC_FIELD_PLACEHOLDER_MATCHER("%(.+)%");


void InsertCustomMarcFields(const MetadataRecord &metadata_record, const Config::JournalParams &journal_params,
                            MARC::Record * const marc_record)
{
    for (auto custom_field : journal_params.marc_metadata_params_.fields_to_add_) {
        const auto placeholder_match(CUSTOM_MARC_FIELD_PLACEHOLDER_MATCHER.match(custom_field));
        const auto custom_field_copy(custom_field);

        if (placeholder_match) {
            std::string first_missing_placeholder;
            for (unsigned i(1); i < placeholder_match.size(); ++i) {
                const auto placeholder(placeholder_match[i]);
                const auto substitution(metadata_record.custom_metadata_.find(placeholder));
                if (substitution == metadata_record.custom_metadata_.end()) {
                    first_missing_placeholder = placeholder;
                    break;
                }

                custom_field = StringUtil::ReplaceString(placeholder_match[0], substitution->second, custom_field);
            }

            if (not first_missing_placeholder.empty()) {
                LOG_DEBUG("custom field '" + custom_field_copy + "' has missing placeholder(s) '"
                          + first_missing_placeholder + "'");
                continue;
            }
        }

        if (unlikely(custom_field.length() < MARC::Record::TAG_LENGTH))
            LOG_ERROR("custom field '" + custom_field_copy + "' is too short");

        const size_t MIN_CONTROl_FIELD_LENGTH(1);
        const size_t MIN_DATA_FIELD_LENGTH(2 /*indicators*/ + 1 /*subfield separator*/ + 1 /*subfield code*/ + 1 /*subfield value*/);

        const MARC::Tag tag(custom_field.substr(0, MARC::Record::TAG_LENGTH));
        if ((tag.isTagOfControlField() and custom_field.length() < MARC::Record::TAG_LENGTH + MIN_CONTROl_FIELD_LENGTH)
            or (not tag.isTagOfControlField() and custom_field.length() < MARC::Record::TAG_LENGTH + MIN_DATA_FIELD_LENGTH))
        {
            LOG_ERROR("custom field '" + custom_field_copy + "' is too short");
        }

        marc_record->insertField(tag, custom_field.substr(MARC::Record::TAG_LENGTH));
        LOG_DEBUG("inserted custom field '" + custom_field + "'");
    }
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



const std::map<std::string, MARC::Record::BibliographicLevel> ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP {
    { "book", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "bookSection", MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART },
    { "document", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "journalArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "magazineArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "newspaperArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "webpage", MARC::Record::BibliographicLevel::INTEGRATING_RESOURCE },
    { "review", MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART }
};


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


void GenerateMarcRecordFromMetadataRecord(const Util::Harvestable &download_item, const MetadataRecord &metadata_record,
                                          const Config::GroupParams &group_params, MARC::Record * const marc_record)
{
    const auto &item_type(metadata_record.item_type_);
    const auto biblio_level(ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.find(item_type));
    if (biblio_level == ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.end())
        LOG_ERROR("no bibliographic level available for item type " + item_type);

    *marc_record = MARC::Record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, biblio_level->second);

    // Control fields (001 depends on the hash of the record, so it's generated towards the end)
    marc_record->insertField("003", group_params.isil_);

    if (metadata_record.superior_type_ == MetadataRecord::SuperiorType::ONLINE)
        marc_record->insertField("007", "cr|||||");
    else
        marc_record->insertField("007", "tu");

    // Authors/Creators
    // Use reverse iterator to keep order, because "insertField" inserts at first possible position
    // The first creator is always saved in the "100" field, all following creators go into the 700 field
    unsigned num_creators_left(metadata_record.creators_.size());
    for (auto creator(metadata_record.creators_.rbegin()); creator != metadata_record.creators_.rend(); ++creator) {
        MARC::Subfields subfields;
        if (not creator->ppn_.empty())
            subfields.appendSubfield('0', "(DE-627)" + creator->ppn_);
        if (not creator->gnd_number_.empty())
            subfields.appendSubfield('0', "(DE-588)" + creator->gnd_number_);
        if (not creator->type_.empty()) {
            const auto creator_type_marc21(CREATOR_TYPES_TO_MARC21_MAP.find(creator->type_));
            if (creator_type_marc21 == CREATOR_TYPES_TO_MARC21_MAP.end())
                LOG_ERROR("zotero creator type '" + creator->type_ + "' could not be mapped to MARC21");

            subfields.appendSubfield('4', creator_type_marc21->second);
        }

        subfields.appendSubfield('a', StringUtil::Join(std::vector<std::string>({ creator->last_name_, creator->first_name_ }),
                                 ", "));

        if (not creator->affix_.empty())
            subfields.appendSubfield('b', creator->affix_ + ".");
        if (not creator->title_.empty())
            subfields.appendSubfield('c', creator->title_);

        if (num_creators_left == 1)
            marc_record->insertField("100", subfields, /* indicator 1 = */'1');
        else
            marc_record->insertField("700", subfields, /* indicator 1 = */'1');

        if (not creator->ppn_.empty() or not creator->gnd_number_.empty()) {
            const std::string _887_data("Autor in der Zoterovorlage [" + creator->last_name_ + ", "
                                        + creator->first_name_ + "] maschinell zugeordnet");
            marc_record->insertField("887", { { 'a', _887_data }, { '2', "ixzom" } });
        }

        --num_creators_left;
    }

    // Title
    if (metadata_record.title_.empty())
        LOG_ERROR("no title provided for download item from URL " + download_item.url_.toString());
    else
        marc_record->insertField("245", { { 'a', metadata_record.title_ } }, /* indicator 1 = */'0', /* indicator 2 = */'0');

    // Language
    if (not metadata_record.language_.empty())
        marc_record->insertField("041", { { 'a', metadata_record.language_ } });

    // Abstract Note
    if (not metadata_record.abstract_note_.empty())
        marc_record->insertField("520", { { 'a', metadata_record.abstract_note_ } });

    // Date & Year
    const auto &date(metadata_record.date_);
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
    if (not metadata_record.url_.empty())
        marc_record->insertField("856", { { 'u', metadata_record.url_ } }, /* indicator1 = */'4', /* indicator2 = */'0');

    // DOI
    const auto &doi(metadata_record.doi_);
    if (not doi.empty()) {
        marc_record->insertField("024", { { 'a', doi }, { '2', "doi" } }, '7');
        const std::string doi_url("https://doi.org/" + doi);
        if (doi_url != metadata_record.url_)
            marc_record->insertField("856", { { 'u', doi_url } }, /* indicator1 = */'4', /* indicator2 = */'0');
    }

    // Review-specific modifications
    if (item_type == "review")
        marc_record->insertField("655", { { 'a', "!106186019!" }, { '0', "(DE-588)" } }, /* indicator1 = */' ', /* indicator2 = */'7');

    // License data
    const auto &license(metadata_record.license_);
    if (license == "l")
        marc_record->insertField("856", { { 'z', "Kostenfrei" } }, /* indicator1 = */'4', /* indicator2 = */'0');
    else if (license == "kw")
        marc_record->insertField("856", { { 'z', "Teilw. kostenfrei" } }, /* indicator1 = */'4', /* indicator2 = */'0');

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
    if (not pages.empty()) {
        if (pages.find('-') == std::string::npos)
            _936_subfields.appendSubfield('g', pages);
        else
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

        if (not pages.empty())
            g_content += ", Seite " + pages;

        _773_subfields.appendSubfield('g', g_content);
        _773_subfield_g_present = true;
    }

    if (_773_subfields_iaxw_present and _773_subfield_g_present)
        marc_record->insertField("773", _773_subfields, '0', '8');
    else
        marc_record->insertField("773", _773_subfields);

    // Keywords
    for (const auto &keyword : metadata_record.keywords_)
        marc_record->insertField(MARC::GetIndexField(TextUtil::CollapseAndTrimWhitespace(keyword)));

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
    if (group_params.bsz_upload_group_ == "krimdok") {
        marc_record->insertField("852", { { 'a', group_params.isil_ } });
        marc_record->insertField("935", { { 'a', "mkri" } });
    } else if (group_params.bsz_upload_group_ == "ixtheo"
               and metadata_record.ssg_ != MetadataRecord::SSGType::INVALID)
    {
        marc_record->insertField("852", { { 'a', group_params.isil_ } });
        marc_record->insertField("935", { { 'a', "mteo" } });
    }

    // Zotero sigil
    marc_record->insertField("935", { { 'a', "zota" }, { '2', "LOK" } });

    marc_record->insertField("001", group_params.name_ + "#" + TimeUtil::GetCurrentDateAndTime("%Y-%m-%d")
                             + "#" + StringUtil::ToHexString(MARC::CalcChecksum(*marc_record)));

    // Book-keeping fields
    marc_record->insertField("ZID", { { 'a', std::to_string(download_item.journal_.zeder_id_) } });
    marc_record->insertField("JOU", { { 'a', download_item.journal_.name_ } });

    // Add custom fields
    InsertCustomMarcFields(metadata_record, download_item.journal_, marc_record);

    // Remove fields
    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : download_item.journal_.marc_metadata_params_.fields_to_remove_) {
        const auto &tag_and_subfield_code(filter.first);
        GetMatchedMARCFields(marc_record, filter.first, *filter.second.get(), &matched_fields);

        for (const auto &matched_field : matched_fields) {
            marc_record->erase(matched_field);
            LOG_DEBUG("erased field '" + tag_and_subfield_code + "' due to removal filter '" + filter.second->getPattern() + "'");
        }
    }
}


bool MarcRecordMatchesExclusionFilters(const Util::Harvestable &download_item, MARC::Record * const marc_record) {
    bool found_match(false);
    std::string exclusion_string;

    std::vector<MARC::Record::iterator> matched_fields;
    for (const auto &filter : download_item.journal_.marc_metadata_params_.exclusion_filters_) {
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


ThreadUtil::ThreadSafeCounter<unsigned> conversion_tasklet_instance_counter;


const std::vector<std::string> VALID_ITEM_TYPES_FOR_ONLINE_FIRST {
    "journalArticle", "magazineArticle"
};


bool ExcludeOnlineFirstRecord(const MetadataRecord &metadata_record, const ConversionParams &parameters) {
    if (std::find(VALID_ITEM_TYPES_FOR_ONLINE_FIRST.begin(),
                  VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end(),
                  metadata_record.item_type_) == VALID_ITEM_TYPES_FOR_ONLINE_FIRST.end())
    {
        return false;
    }

    if (metadata_record.issue_.empty() and metadata_record.volume_.empty()
        and not parameters.force_downloads_)
    {
        if (parameters.skip_online_first_articles_unconditonally_) {
            LOG_DEBUG("Skipping: online-first article unconditionally");
            return true;
        } else if (metadata_record.doi_.empty()) {
            LOG_DEBUG("Skipping: online-first article without a DOI");
            return true;
        }
    }

    return false;
}


void ConversionTasklet::run(const ConversionParams &parameters, ConversionResult * const result) {
    std::shared_ptr<JSON::JSONNode> tree_root;
    JSON::Parser json_parser(parameters.json_metadata_);

    if (not json_parser.parse(&tree_root)) {
        LOG_WARNING("failed to parse JSON: " + json_parser.getErrorMessage());
        return;
    }

    const auto &download_item(parameters.download_item_);
    auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("tree_root", tree_root));
    PostprocessTranslationServerResponse(parameters.download_item_, &array_node);

    for (const auto &entry : *array_node) {
        const auto json_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));

        try {
            if (ZoteroItemMatchesExclusionFilters(download_item, json_object))
                continue;

            MetadataRecord new_metadata_record;
            ConvertZoteroItemToMetadataRecord(json_object, &new_metadata_record);
            AugmentMetadataRecord(&new_metadata_record, download_item.journal_, parameters.group_params_,
                                  parameters.enhancement_maps_);

            if (ExcludeOnlineFirstRecord(new_metadata_record, parameters))
                continue;

            // a dummy record that will be replaced subsequently
            std::unique_ptr<MARC::Record> new_marc_record(new MARC::Record(std::string(MARC::Record::LEADER_LENGTH, ' ')));
            GenerateMarcRecordFromMetadataRecord(download_item, new_metadata_record, parameters.group_params_,
                                                 new_marc_record.get());

            if (MarcRecordMatchesExclusionFilters(download_item, new_marc_record.get()))
                continue;

            result->marc_records_.emplace_back(new_marc_record.release());
        } catch (const std::exception &x) {
            LOG_WARNING("couldn't convert record: " + std::string(x.what()));
        }
    }
}


ConversionTasklet::ConversionTasklet(std::unique_ptr<ConversionParams> parameters)
 : Util::Tasklet<ConversionParams, ConversionResult>(&conversion_tasklet_instance_counter, parameters->download_item_,
                                                     "Conversion: " + parameters->download_item_.url_.toString(),
                                                     std::bind(&ConversionTasklet::run, this, std::placeholders::_1, std::placeholders::_2),
                                                     std::move(parameters), std::unique_ptr<ConversionResult>(new ConversionResult())) {}


unsigned ConversionTasklet::GetRunningInstanceCount() {
    return conversion_tasklet_instance_counter;
}


} // end namespace Conversion


} // end namespace ZoteroHarvester
