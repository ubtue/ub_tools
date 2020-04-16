/** \brief Very simple class for retrieval of values from any of our Zeder instances.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "SimpleZeder.h"
#include "Downloader.h"
#include "StringUtil.h"
#include "JSON.h"
#include "util.h"


std::string SimpleZeder::Journal::lookup(const std::string &short_column_name) const {
    const auto short_column_name_and_value(short_column_names_to_values_map_.find(short_column_name));
    return (short_column_name_and_value == short_column_names_to_values_map_.cend()) ? "" : short_column_name_and_value->second;
}


namespace {


const std::string ZEDER_URL_PREFIX(
    "http://www-ub.ub.uni-tuebingen.de/zeder/cgi-bin/zeder.cgi?action=get&Dimension=wert&Bearbeiter=&Instanz=");


void GetZederJSON(const std::string &zeder_instance, std::string * const json_blob) {
    Downloader downloader(ZEDER_URL_PREFIX + zeder_instance);
    if (downloader.anErrorOccurred())
        LOG_ERROR("failed to download Zeder data: " + downloader.getLastErrorMessage());

    const auto http_response_code(downloader.getResponseCode());
    if (http_response_code < 200 or http_response_code > 399)
        LOG_ERROR("got bad HTTP response code: " + std::to_string(http_response_code));

    *json_blob = downloader.getMessageBody();
}


typedef std::unordered_map<std::string, std::map<std::string, std::string>> ColumnNamesToEnumMaps;


// Returns a mapping from short column name to a map from short integer values as strings to symbolic enum names.
// This is needed because columns containing enum values are not specified with the actual enum constants but with
// small integers which must be mapped to the actual enum constants.
ColumnNamesToEnumMaps GetZederEnumMappings(const JSON::ArrayNode &meta_array) {
    ColumnNamesToEnumMaps column_names_to_enum_maps;
    for (const auto &entry : meta_array) {
        const auto enum_object(JSON::JSONNode::CastToObjectNodeOrDie("enum_object", entry));
        const auto short_column_name(enum_object->getStringNode("Kurz")->getValue());
        const auto enum_values_array(enum_object->getArrayNode("Optionen"));
        if (enum_values_array->empty())
            continue;

        std::map<std::string, std::string> enum_map;
        for (const auto &enum_value_desc : *enum_values_array) {
            const auto enum_value_object(JSON::JSONNode::CastToObjectNodeOrDie("enum_value_object", enum_value_desc));
            enum_map.emplace(enum_value_object->getIntegerNode("id")->toString(),
                             enum_value_object->getStringNode("wert")->getValue());

        }
        column_names_to_enum_maps.emplace(short_column_name, enum_map);
    }

    LOG_INFO("found " + std::to_string(column_names_to_enum_maps.size()) + " enum maps.");
    return column_names_to_enum_maps;
}


// In order to understand this function you need the following information:
// 1. Someone, who will remain unnamed to protect the guilty has, in his infinite wisdom (that was sacrasm for the Americans..)
//    decided not to simply have empty values but to instead use NV (= no value??)
// 2. There appear to be two kinds of legitimate values, real immediate value and values that represent an enum value.
//    The enum values are not provided as literals but are represented by small integers instead.  In order to convert those
//    integers to their string literals equivalents we need to use "column_names_to_enum_maps."  Fo those columns for which
//    such a mapping exists we have the complete mapping stored under the short column name key in "column_names_to_enum_maps."
//    IOW, if the short column name does not exist in "column_names_to_enum_maps" it is not a enum-valued column and the value
//    should be taken as is and not mapped.
std::string GetString(const JSON::ObjectNode &journal_node, const std::string &key, const ColumnNamesToEnumMaps &column_names_to_enum_maps) {
    if (not journal_node.hasNode(key))
        return "";

    const auto entry_node(journal_node.getNode(key));
    const auto entry_node_type(entry_node->getType());
    std::string value;
    switch (entry_node_type) {
    case JSON::JSONNode::STRING_NODE:
        value = journal_node.getStringNode(key)->getValue();
        if (value == "NV")
            return "";
        break;
    case JSON::JSONNode::INT64_NODE: {
        const auto integer_node(JSON::JSONNode::CastToIntegerNodeOrDie("integer_node", entry_node));
        value = std::to_string(integer_node->getValue());
        break;
    }
    default:
        LOG_ERROR("unexpected entry node type \"" + JSON::JSONNode::TypeToString(entry_node_type) + "\"!");
    }

    const auto short_column_name_and_enum_map(column_names_to_enum_maps.find(key));
    if (short_column_name_and_enum_map == column_names_to_enum_maps.cend())
        return value;

    const auto integer_and_symbolic_enum_value(short_column_name_and_enum_map->second.find(value));
    if (unlikely(integer_and_symbolic_enum_value == short_column_name_and_enum_map->second.cend()))
        LOG_ERROR("no mapping from \"" + value + "\" to a symbolic enum value found for column \"" + key + "\"!");

    return integer_and_symbolic_enum_value->second;
}


} // unnamed namespace


SimpleZeder::SimpleZeder(const InstanceType instance_type, const std::unordered_set<std::string> &column_filter) {
    std::string json_blob;
    GetZederJSON(instance_type == IXTHEO ? "ixtheo" : "krim", &json_blob);

    JSON::Parser parser(json_blob);
    std::shared_ptr<JSON::JSONNode> tree_root;
    if (not parser.parse(&tree_root))
        LOG_ERROR("failed to parse the Zeder JSON: " + parser.getErrorMessage());

    // Zeder JSON has two top-level keys, "meta" and "daten."  "daten" contains the actual rows corresponding to one journal
    // per row.  "meta" contains mappings from small integers to enumerated constants for those columns that use enum values.
    const auto root_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", tree_root));
    if (not root_node->hasNode("meta"))
        LOG_ERROR("top level object of Zeder JSON does not have a \"meta\" key!");
    const auto column_names_to_enum_maps(GetZederEnumMappings(*JSON::JSONNode::CastToArrayNodeOrDie("meta", root_node->getNode("meta"))));
    if (not root_node->hasNode("daten"))
        LOG_ERROR("top level object of Zeder JSON does not have a \"daten\" key!");
    const auto daten(JSON::JSONNode::CastToArrayNodeOrDie("daten", root_node->getNode("daten")));

    for (const auto &entry : *daten) {
        const auto journal_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));

        Journal::ShortColumnNameToValuesMap short_column_names_to_values_map;
        if (column_filter.empty()) { // Include all columns.
            for (const auto &short_column_name_and_object_entry : *journal_object) {
                const auto &short_column_name(short_column_name_and_object_entry.first);
                const auto value_as_string(StringUtil::TrimWhite(GetString(*journal_object, short_column_name, column_names_to_enum_maps)));
                if (likely(not value_as_string.empty()))
                    short_column_names_to_values_map[short_column_name] = value_as_string;
            }
        } else { // Include only the specified columns.
            for (const auto &short_column_name : column_filter) {
                const auto node(journal_object->getNode(short_column_name));
                if (node == nullptr)
                    continue;
                const auto value_as_string(StringUtil::TrimWhite(GetString(*journal_object, short_column_name, column_names_to_enum_maps)));
                if (not value_as_string.empty())
                    short_column_names_to_values_map[short_column_name] = value_as_string;
            }
            if (likely(not short_column_names_to_values_map.empty()))
                journals_.emplace_back(short_column_names_to_values_map);
        }
    }
}
