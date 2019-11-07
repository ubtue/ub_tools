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

#include "StringUtil.h"
#include "ZoteroHarvesterConversion.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Conversion {


Util::ThreadLocal<RegexMatcher> page_range_matcher([]() -> std::unique_ptr<RegexMatcher> {
    return std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie("^(.+)-(.+)$"));
});
Util::ThreadLocal<RegexMatcher> page_range_digit_matcher([]() -> std::unique_ptr<RegexMatcher> {
    return std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie("^(\\d+)-(\\d+)$"));
});


// void SuppressJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
//                           const Util::Harvestable &download_item)
// {
//     const auto suppression_regex(download_item.journal_.zotero_metadata_params_.fields_to_suppress_.find(node_name));
//     if (suppression_regex != download_item.journal_.zotero_metadata_params_.fields_to_suppress_.metadata_suppression_filters_.end()) {
//         if (node->getType() != JSON::JSONNode::STRING_NODE)
//             LOG_ERROR("metadata suppression filter has invalid node type '" + node_name + "'");

//         const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
//         if (suppression_regex->second->matched(string_node->getValue())) {
//             LOG_DEBUG("suppression regex '" + suppression_regex->second->getPattern() +
//                         "' matched metadata field '" + node_name + "' value '" + string_node->getValue() + "'");
//             string_node->setValue("");
//         }
//     }
// }


// void OverrideJsonMetadata(const std::string &node_name, const std::shared_ptr<JSON::JSONNode> &node,
//                           const SiteParams &site_params)
// {
//     static const std::string ORIGINAL_VALUE_SPECIFIER("%org%");

//     const auto override_pattern(site_params.metadata_overrides_.find(node_name));
//     if (override_pattern != site_params.metadata_overrides_.end()) {
//         if (node->getType() != JSON::JSONNode::STRING_NODE)
//             LOG_ERROR("metadata override has invalid node type '" + node_name + "'");

//         const auto string_node(JSON::JSONNode::CastToStringNodeOrDie(node_name, node));
//         const auto string_value(string_node->getValue());
//         const auto override_string(StringUtil::ReplaceString(ORIGINAL_VALUE_SPECIFIER, string_value,override_pattern->second));

//         LOG_DEBUG("metadata field '" + node_name + "' value changed from '" + string_value + "' to '" + override_string + "'");
//         string_node->setValue(override_string);
//     }
// }


void PostprocessZoteroItem(std::shared_ptr<JSON::ArrayNode> * const response_json_array) {
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
//        JSON::VisitLeafNodes("root", json_object, SuppressJsonMetadata, std::ref(site_params));
//        JSON::VisitLeafNodes("root", json_object, OverrideJsonMetadata, std::ref(site_params));
    }


}



} // end namespace Conversion


} // end namespace ZoteroHarvester
