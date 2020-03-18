/** \file    convert_json_to_marc.cc
 *  \brief   Converts JSON to MARC.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2020 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <utility>
#include <vector>
#include "FileUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {


struct FieldDescriptor {
    std::string name_;
    std::string tag_, overflow_tag_;
    bool repeat_field_;
    std::vector<std::pair<char, std::string>> subfield_codes_to_json_tags_; // For mapping to variable fields
    std::string json_tag_; // For mapping to control fields
    std::string field_contents_prefix_; // For mapping to data fields
    bool required_;
public:
    explicit FieldDescriptor(const std::string &name, const std::string &tag, const std::string &overflow_tag, const bool repeat_field,
                             const std::vector<std::pair<char, std::string>> &subfield_codes_to_json_tags, const std::string &json_tag,
                             const std::string &field_contents_prefix, const bool required);
    bool operator<(const FieldDescriptor &other) const { return tag_ < other.tag_; }
};


FieldDescriptor::FieldDescriptor(const std::string &name, const std::string &tag, const std::string &overflow_tag, const bool repeat_field,
                                 const std::vector<std::pair<char, std::string>> &subfield_codes_to_json_tags, const std::string &json_tag,
                                 const std::string &field_contents_prefix, const bool required)
    : name_(name), tag_(tag), overflow_tag_(overflow_tag), repeat_field_(repeat_field),
      subfield_codes_to_json_tags_(subfield_codes_to_json_tags), json_tag_(json_tag), field_contents_prefix_(field_contents_prefix),
      required_(required)
{
    if (not overflow_tag_.empty() and repeat_field_)
        LOG_ERROR("field \"" + name_ + "\" can't have both, an over flow tag and being a repeat field!");

    if (subfield_codes_to_json_tags_.empty() and json_tag_.empty())
        LOG_ERROR("field \"" + name_ + "\" neither a mapping to the contents of a control field nor the contents of data subfields has been specified!");
}


class JSONNodeToBibliographicLevelMapper {
    std::string json_tag_;
    MARC::Record::BibliographicLevel default_;
    std::vector<std::pair<RegexMatcher *, MARC::Record::BibliographicLevel>> regex_to_bibliographic_level_map;
public:
    JSONNodeToBibliographicLevelMapper(const std::string &item_type_tag, const std::string &item_type_map);
    MARC::Record::BibliographicLevel getBibliographicLevel(const JSON::ObjectNode &node) const;
};


// Expects two values separated by a colon.  Backslash-escaped colons do not count as the separator colon!
void SplitPatternAndItemType(const std::string &pattern_and_type, std::string * const pattern, std::string * const type) {
    bool escaped(false), in_pattern(true);
    for (const char ch : pattern_and_type) {
        if (escaped) {
            if (in_pattern)
                *pattern += ch;
            else
                *type += ch;
            escaped = false;
        } else if (ch == '\\')
            escaped = true;
        else if (ch == ':') {
            if (not in_pattern)
                LOG_ERROR("bad regex pattern to item type mapping with additional unescaped colon: \"" + pattern_and_type + "\"!");
            in_pattern = false;
        } else {
            if (in_pattern)
                *pattern += ch;
            else
                *type += ch;
            escaped = false;
        }
    }

    if (escaped)
        LOG_ERROR("bad regex pattern to item type mapping: \"" + pattern_and_type + "\"!");
}


MARC::Record::BibliographicLevel MapTypeStringToBibliographicLevel(const std::string &item_type) {
    if (item_type == "monograph")
        return MARC::Record::MONOGRAPH_OR_ITEM;
    else if (item_type == "book chapter")
        return MARC::Record::MONOGRAPHIC_COMPONENT_PART;
    else if (item_type == "journal article")
        return MARC::Record::SERIAL_COMPONENT_PART;
    else
        LOG_ERROR("\"" + item_type + "\" is not a valid item type!");
}


JSONNodeToBibliographicLevelMapper::JSONNodeToBibliographicLevelMapper(const std::string &item_type_tag, const std::string &item_type_map)
    : json_tag_(item_type_tag)
{
    default_ = MARC::Record::UNDEFINED;

    std::vector<std::string> pattens_and_types;
    StringUtil::Split(item_type_map, '|', &pattens_and_types);
    for (auto pattern_and_type(pattens_and_types.cbegin()); pattern_and_type != pattens_and_types.cend(); ++pattern_and_type) {
        std::string pattern, type;
        SplitPatternAndItemType(*pattern_and_type, &pattern, &type);
        if (pattern.empty()) {
            if (pattern_and_type != pattens_and_types.cend() - 1)
                LOG_ERROR("default w/o pattern must be the last entry in the pattern to item type mapping!");
            default_ = MapTypeStringToBibliographicLevel(type);
            return;
        }

        std::string err_msg;
        const auto regex(RegexMatcher::RegexMatcherFactory(pattern, &err_msg));
        if (regex == nullptr)
            LOG_ERROR("bad regex pattern in pattern to item type mapping: \"" + *pattern_and_type + "\"! (" + err_msg + ")");

        regex_to_bibliographic_level_map.emplace_back(regex, MapTypeStringToBibliographicLevel(type));

    }
}


MARC::Record::BibliographicLevel JSONNodeToBibliographicLevelMapper::getBibliographicLevel(const JSON::ObjectNode &node) const {
}


void ProcessGlobalSection(const IniFile::Section &global_section, std::string * const root_path,
                          JSONNodeToBibliographicLevelMapper ** const json_node_to_bibliographic_level_mapper)
{
    *root_path = global_section.getString("root_path");

    if (global_section.hasEntry("item_type_tag") and not global_section.hasEntry("item_type_map"))
        LOG_ERROR("Global section \"has item_type_tag\" but not \"item_type_map\"!");

    if (not global_section.hasEntry("item_type_tag") and global_section.hasEntry("item_type_map"))
        LOG_ERROR("Global section \"has item_type_map\" but not \"item_type_tag\"!");

    std::string item_type_tag, item_type_map;
    if (global_section.hasEntry("item_type_tag")) {
        item_type_tag = global_section.getString("item_type_tag");
        item_type_map = global_section.getString("item_type_map");
    }

    *json_node_to_bibliographic_level_mapper = new JSONNodeToBibliographicLevelMapper(item_type_tag, item_type_map);
}


std::vector<FieldDescriptor> LoadFieldDescriptors(const std::string &inifile_path, std::string * const root_path,
                                                  JSONNodeToBibliographicLevelMapper ** const json_node_to_bibliographic_level_mapper)
{
    std::vector<FieldDescriptor> field_descriptors;

    const IniFile ini_file(inifile_path);
    for (const auto &section : ini_file) {
        const auto &section_name(section.getSectionName());
        if (section_name == "Global")
            ProcessGlobalSection(section, root_path, json_node_to_bibliographic_level_mapper);
        else { // A section describing a mapping to a field
            const auto tag(section.getString("tag", ""));
            if (tag.empty())
                LOG_ERROR("missing tag in section \"" + section_name + "\" in \"" + ini_file.getFilename() + "\"!");
            if (tag.length() != MARC::Record::TAG_LENGTH)
                LOG_ERROR("invalid tag in section \"" + section_name + "\" in \"" + ini_file.getFilename() + "\"!");

            std::vector<std::pair<char, std::string>> subfield_codes_to_json_tags;
            for (const auto section_entry : section) {
                if (not StringUtil::StartsWith(section_entry.name_, "subfield_"))
                    continue;

                if (section_entry.name_.length() != __builtin_strlen("subfield_" + 1))
                    LOG_ERROR("invalid section entry \"" + section_entry.name_ + "\"!");
                const char subfield_code(section_entry.name_.back());
                const auto json_tag(section_entry.value_);
                subfield_codes_to_json_tags.emplace_back(subfield_code, json_tag);
            }
            const auto json_tag(section.getString("json_tag", ""));
            if (subfield_codes_to_json_tags.empty() and json_tag.empty())
                LOG_ERROR("missing JSON source tag(s) for MARC field tag \"" + tag + "\" in section \"" + section_name + "\"!");
            if (not subfield_codes_to_json_tags.empty() and not json_tag.empty())
                 LOG_ERROR("can't have subfield and non-subfield contents for MARC field tag \"" + tag + "\" in section \""
                           + section_name + "\"!");
            const auto field_contents_prefix(section.getString("field_contents_prefix", ""));
            if (not field_contents_prefix.empty() and not subfield_codes_to_json_tags.empty())
                LOG_ERROR("can't specify a field contents prefix when subfields have been specified for MARC field tag \""
                          + tag + "\" in section \"" + section_name + "\"!");

            field_descriptors.emplace_back(section_name, tag, section.getString("overflow_tag", ""), section.getBool("repeat_field", false),
                                           subfield_codes_to_json_tags, json_tag, field_contents_prefix,
                                           section.getBool("required", false));
        }
    }

    std::sort(field_descriptors.begin(), field_descriptors.end());
    return field_descriptors;
}


void ProcessFieldDescriptor(const FieldDescriptor &field_descriptor, const std::shared_ptr<const JSON::ObjectNode> &object,
                            MARC::Record * const record)
{
    bool created_at_least_one_field(false);
    if (not field_descriptor.json_tag_.empty()) { // Control field
        const auto node(object->getNode(field_descriptor.json_tag_));
        if (node != nullptr) {
            if (node->getType() != JSON::JSONNode::ARRAY_NODE)
                record->insertField(MARC::Tag(field_descriptor.tag_), node->toString());
            else {
                const auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("node", node));
                if (field_descriptor.repeat_field_) {
                }
            }
        } else if (field_descriptor.required_)
            LOG_ERROR("missing JSON tag \"" + field_descriptor.json_tag_ + "\" for required field \"" + field_descriptor.name_ + "\"!");
    } else { // Data field

    }

    if (field_descriptor.required_ and not created_at_least_one_field)
        LOG_ERROR("required entry for \"" + field_descriptor.name_ + "\" not found!");
}


void GenerateSingleMARCRecordFromJSON(const std::shared_ptr<const JSON::ObjectNode> &object,
                                      const JSONNodeToBibliographicLevelMapper &json_node_to_bibliographic_level_mapper,
                                      const std::vector<FieldDescriptor> &field_descriptors, MARC::Writer * const marc_writer)
{
    std::string control_number;
    const auto descriptor_for_field_001(std::find_if(field_descriptors.begin(), field_descriptors.end(),
                                                     [](const FieldDescriptor &descriptor){ return descriptor.tag_ == "001"; }));
    if (descriptor_for_field_001 != field_descriptors.end()) {
        control_number = object->getOptionalStringValue(descriptor_for_field_001->json_tag_);
        if (not control_number.empty())
            control_number = descriptor_for_field_001->field_contents_prefix_ + control_number;
    }

    const auto bibliographic_level(json_node_to_bibliographic_level_mapper.getBibliographicLevel(*object));
    MARC::Record new_record(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, bibliographic_level, control_number);
    for (const auto field_descriptor : field_descriptors) {
        if (field_descriptor.tag_ != "001")
            ProcessFieldDescriptor(field_descriptor, object, &new_record);
    }
    marc_writer->write(new_record);
}


void GenerateMARCFromJSON(const std::shared_ptr<const JSON::JSONNode> &object_or_array_root,
                          const JSONNodeToBibliographicLevelMapper &json_node_to_bibliographic_level_mapper,
                          const std::vector<FieldDescriptor> &field_descriptors, MARC::Writer * const marc_writer)
{
    switch (object_or_array_root->getType()) {
    case JSON::JSONNode::OBJECT_NODE:
        GenerateSingleMARCRecordFromJSON(JSON::JSONNode::CastToObjectNodeOrDie("object_or_array_root", object_or_array_root),
                                         json_node_to_bibliographic_level_mapper, field_descriptors, marc_writer);
        break;
    case JSON::JSONNode::ARRAY_NODE: {
        const auto array_node(JSON::JSONNode::CastToArrayNodeOrDie("object_or_array_root", object_or_array_root));
        for (const auto &array_element : *array_node)
            GenerateSingleMARCRecordFromJSON(JSON::JSONNode::CastToObjectNodeOrDie("array_element", array_element),
                                             json_node_to_bibliographic_level_mapper, field_descriptors, marc_writer);
        break;
    }
    default:
        LOG_ERROR("\"root_path\" in section \"Gobal\" does not reference a JSON object or array!");
    }
}


} // namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        ::Usage("config_file json_input marc_output");

    std::string root_path;
    JSONNodeToBibliographicLevelMapper *json_node_to_bibliographic_level_mapper;
    const auto field_descriptors(LoadFieldDescriptors(argv[1], &root_path, &json_node_to_bibliographic_level_mapper));

    const std::string json_file_path(argv[2]);
    const auto json_source(FileUtil::ReadStringOrDie(json_file_path));
    JSON::Parser parser(json_source);
    std::shared_ptr<JSON::JSONNode> tree_root;
    if (not parser.parse(&tree_root))
        LOG_ERROR("Failed to parse the contents of \"" + json_file_path + "\": " + parser.getErrorMessage());
    const auto object_or_array_root(JSON::LookupNode(root_path, tree_root));

    auto marc_writer(MARC::Writer::Factory(argv[3]));
    GenerateMARCFromJSON(object_or_array_root, *json_node_to_bibliographic_level_mapper, field_descriptors, marc_writer.get());

    return EXIT_SUCCESS;
}
