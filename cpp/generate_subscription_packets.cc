/** \file   generate_subscription_packets.cc
 *  \brief  Imports data from Zeder and writes a subscription packets defintion file.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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

#include <map>
#include <unordered_map>
#include <set>
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "StringUtil.h"
#include "util.h"


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

    const auto value(journal_node.getStringNode(key)->getValue());
    if (value == "NV")
        return "";

    const auto short_column_name_and_enum_map(column_names_to_enum_maps.find(key));
    if (short_column_name_and_enum_map == column_names_to_enum_maps.cend())
        return value;

    const auto integer_and_symbolic_enum_value(short_column_name_and_enum_map->second.find(value));
    if (unlikely(integer_and_symbolic_enum_value == short_column_name_and_enum_map->second.cend()))
        LOG_ERROR("no mapping from \"" + value + "\" to a symbolic enum value found for column \"" + key + "\"!");

    return integer_and_symbolic_enum_value->second;
}


// Return true if an entry of "class_list" equals one of the vertical-bar-separated values of "expected_values_str."
bool FoundExpectedClassValue(const std::string &expected_values_str, const std::string &class_list_str) {
    std::vector<std::string> expected_values;
    StringUtil::Split(expected_values_str, '|', &expected_values);

    std::vector<std::string> class_list;
    StringUtil::SplitThenTrimWhite(class_list_str, ',', &class_list);

    for (const auto &class_str : class_list) {
        if (std::find_if(expected_values.cbegin(), expected_values.cend(),
                         [&class_str](auto &expected_value) { return class_str == expected_value; }) != expected_values.cend())
            return true;
    }
    return false;
}


bool IncludeJournal(const JSON::ObjectNode &journal_object, const IniFile::Section &filter_section,
                    const ColumnNamesToEnumMaps &column_names_to_enum_maps)
{
    std::string true_string;
    for (const auto &entry : filter_section) {
        if (entry.name_.empty() or entry.name_ == "description")
            continue;

        std::string zeder_column_name(entry.name_);
        if (entry.name_ == "except_class")
            zeder_column_name = "class";

        const auto node(journal_object.getNode(zeder_column_name));
        if (node == nullptr)
            return false;

        const auto value_as_string(StringUtil::TrimWhite(GetString(journal_object, zeder_column_name, column_names_to_enum_maps)));
        if (value_as_string.empty())
            return false;
        true_string += " " + zeder_column_name + ":" + value_as_string;

        if (zeder_column_name != "class") {
            if (::strcasecmp(value_as_string.c_str(),entry.value_.c_str()) != 0)
                return false;
        } else { // class or except_class
            const bool found_it(FoundExpectedClassValue(entry.value_, value_as_string));
            if ((not found_it and entry.name_ == "class")
                or (found_it and entry.name_ == "except_class"))
                return false;
        }
    }

    return true;
}


// Please note that Zeder PPN entries are separated by spaces and, unlike what the column names "print_ppn" and
// "online_ppn" imply may in rare cases contain space-separated lists of PPN's.
void ProcessPPNs(const std::string &ppns, std::set<std::string> * const bundle_ppns) {
    std::vector<std::string> individual_ppns;
    StringUtil::Split(ppns, ' ', &individual_ppns);
    bundle_ppns->insert(individual_ppns.cbegin(), individual_ppns.cend());
}


std::string EscapeDoubleQuotes(const std::string &s) {
    std::string escaped_s;
    escaped_s.reserve(s.size());

    for (const char ch : s) {
        if (ch == '"' or ch == '\\')
            escaped_s += '\\';
        escaped_s += ch;
    }

    return escaped_s;
}


void GenerateBundleDefinition(const JSON::ArrayNode &journals_array, const std::string &bundle_instances,
                              const IniFile::Section &section,
                              const ColumnNamesToEnumMaps &column_names_to_enum_maps, File * const output_file)
{
    unsigned included_journal_count(0);
    std::set<std::string> bundle_ppns; // We use a std::set because it is automatically being sorted for us.
    for (const auto &entry : journals_array) {
        const auto journal_object(JSON::JSONNode::CastToObjectNodeOrDie("entry", entry));
        if (not IncludeJournal(*journal_object, section, column_names_to_enum_maps))
            continue;

        ++included_journal_count;
        const auto print_ppns(GetString(*journal_object, "pppn", column_names_to_enum_maps));
        const auto online_ppns(GetString(*journal_object, "eppn", column_names_to_enum_maps));

        if (print_ppns.empty() and online_ppns.empty()) {
            --included_journal_count;
            const auto zeder_id(std::to_string(journal_object->getIntegerNode("DT_RowId")->getValue()));
            LOG_WARNING("Zeder entry #" + zeder_id + " is missing print and online PPN's!");
            continue;
        }

        ProcessPPNs(print_ppns, &bundle_ppns);
        ProcessPPNs(online_ppns, &bundle_ppns);
    }

    if (bundle_ppns.empty())
        LOG_WARNING("No bundle generated for \"" + section.getSectionName() + "\" because there were no matching entries in Zeder!");
    else {
        (*output_file) << '[' << section.getSectionName() << "]\n";
        (*output_file) << "display_name = \"" << EscapeDoubleQuotes(section.getSectionName()) << "\"\n";
        (*output_file) << "instances    = \"" << bundle_instances << "\"\n";
        (*output_file) << "ppns         = " << StringUtil::Join(bundle_ppns, ',') << '\n';
        (*output_file) << '\n';
    }

    LOG_INFO("included " + std::to_string(included_journal_count) + " journal(s) with " + std::to_string(bundle_ppns.size())
             + " PPN's in the bundle for \"" + section.getSectionName() + "\".");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("packet_definition_config_file packet_subscriptions_output\n"
                "\tFor the documentation of the input config file, please see data/generate_subscription_packets.README.");

    const IniFile packet_definitions_ini_file(argv[1]);
    const auto zeder_instance(packet_definitions_ini_file.getString("", "zeder_instance"));
    const auto bundle_instances(packet_definitions_ini_file.getString("", "bundle_instances"));

    std::string json_blob;
    GetZederJSON(zeder_instance, &json_blob);
    FileUtil::WriteString("blob.json", json_blob);

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

    const auto bundle_definitions_output_file(FileUtil::OpenOutputFileOrDie(argv[2]));
    for (const auto &section : packet_definitions_ini_file) {
        if (section.getSectionName().empty())
            continue; // Skip the global section.
        GenerateBundleDefinition(*daten, bundle_instances, section, column_names_to_enum_maps, bundle_definitions_output_file.get());
    }

    return EXIT_SUCCESS;
}
