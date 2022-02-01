/** \file    generate_keyword_normalizations.cc
 *  \brief   Create a mapping file from forms w/ different capitalizations to a single form for keywords.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019-2020 Library of the University of Tübingen

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
#include <unordered_map>
#include <vector>
#include "FileUtil.h"
#include "JSON.h"
#include "Solr.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


struct CapitalizationAndCount {
    std::string capitalization_;
    unsigned count_;
public:
    CapitalizationAndCount() = default;
    CapitalizationAndCount(const CapitalizationAndCount &other) = default;
    explicit CapitalizationAndCount(const std::string &capitalization): capitalization_(capitalization), count_(1) { }
};




unsigned ProcessJSON(
    const std::string &json_result, const std::string &solr_field, std::string * const cursor_mark,
    std::unordered_map<std::string, std::vector<CapitalizationAndCount>> * const lowercase_form_to_capitalizations_and_counts_map)
{
    JSON::Parser parser(json_result);
    std::shared_ptr<JSON::JSONNode> tree_root;
    if (not parser.parse(&tree_root))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const auto root_object_node(JSON::JSONNode::CastToObjectNodeOrDie("tree_root", tree_root));
    const auto response_node(root_object_node->getObjectNode("response"));
    const auto docs_node(response_node->getArrayNode("docs"));
    const auto cursorNode(root_object_node->getStringNode("nextCursorMark"));
    *cursor_mark  = cursorNode->getValue();


    unsigned item_count(0);
    for (const auto &item : *docs_node) {
        ++item_count;
        const auto item_object(JSON::JSONNode::CastToObjectNodeOrDie("item", item));
        const auto topic_array(item_object->getArrayNode(solr_field));

        for (const auto &single_topic : *topic_array) {
            const auto single_topic_string(JSON::JSONNode::CastToStringNodeOrDie("single_topic_string", single_topic));
            const auto lowercase_form(TextUtil::UTF8ToLower(single_topic_string->getValue()));
            const auto lowercase_form_and_capitalizations(lowercase_form_to_capitalizations_and_counts_map->find(lowercase_form));
            if (lowercase_form_and_capitalizations == lowercase_form_to_capitalizations_and_counts_map->end())
                (*lowercase_form_to_capitalizations_and_counts_map)[lowercase_form] =
                    std::vector<CapitalizationAndCount>{ CapitalizationAndCount(single_topic_string->getValue()) };
            else {
                auto &capitalizations_and_counts_vector(lowercase_form_and_capitalizations->second);
                auto capitalization_and_count(
                    std::find_if(capitalizations_and_counts_vector.begin(), capitalizations_and_counts_vector.end(),
                                 [&single_topic_string](CapitalizationAndCount &entry)
                                     { return entry.capitalization_ == single_topic_string->getValue(); }));
                if (capitalization_and_count != capitalizations_and_counts_vector.end())
                    ++(capitalization_and_count->count_);
                else
                    capitalizations_and_counts_vector.emplace_back(CapitalizationAndCount(single_topic_string->getValue()));
            }
        }
    }

    return item_count;
}


void CollectStats(
    const std::string &solr_host_and_port, const std::string &solr_field,
    std::unordered_map<std::string, std::vector<CapitalizationAndCount>> * const lowercase_form_to_capitalizations_and_counts_map)
{

    unsigned total_item_count(0);
    std::string cursor_mark("*");
    for (;;) {
        std::string json_result, err_msg;
        if (unlikely(not Solr::Query(solr_field + ":*", solr_field, &json_result, &err_msg,
                                     solr_host_and_port, /* timeout = */ 600, Solr::JSON,  "cursorMark=" + cursor_mark + "&sort=id+asc&rows=100000")))
            LOG_ERROR("Solr query failed or timed-out: " + err_msg);

        std::string new_cursor_mark;
        const unsigned item_count(ProcessJSON(json_result, solr_field, &new_cursor_mark, lowercase_form_to_capitalizations_and_counts_map));
        if (cursor_mark == new_cursor_mark) {
            LOG_INFO("processed " + std::to_string(total_item_count) + " items and added "
                     + std::to_string(lowercase_form_to_capitalizations_and_counts_map->size()) + " entries into our map.");
            return;
        }
        cursor_mark = new_cursor_mark;
        total_item_count += item_count;
        LOG_INFO("Item count so far: " + std::to_string(total_item_count));
    }
}


bool IsInitialCapsVersion(const std::string &keyphrase) {
    std::vector<std::string> words;
    StringUtil::SplitThenTrimWhite(keyphrase, ' ', &words);
    for (const auto &word : words) {
        if (word != TextUtil::InitialCaps(word))
            return false;
    }

    return true;
}


void GenerateCanonizationMap(
    File * const output,
    const std::unordered_map<std::string, std::vector<CapitalizationAndCount>> &lowercase_form_to_capitalizations_and_counts_map)
{
    for (const auto &lowercase_form_and_capitalizations : lowercase_form_to_capitalizations_and_counts_map) {
        const auto &capitalizations(lowercase_form_and_capitalizations.second);
        if (capitalizations.size() == 1)
            continue;

        // Pick a capitalization with the highest occurrence count:
        unsigned canonical_index(0), max_count(0);
        for (unsigned i(0); i < capitalizations.size(); ++i) {
            if (IsInitialCapsVersion(capitalizations[i].capitalization_)) {
                canonical_index = i;
                break;
            }

            if (capitalizations[i].count_ > max_count) {
                max_count = capitalizations[i].count_;
                canonical_index = i;
            }
        }

        std::string non_canonical_forms;
        for (unsigned i(0); i < capitalizations.size(); ++i) {
            if (i == canonical_index)
                continue;

            if (not non_canonical_forms.empty())
                non_canonical_forms += '|';
            non_canonical_forms += capitalizations[i].capitalization_;
        }

        *output << non_canonical_forms << "->" << capitalizations[canonical_index].capitalization_ << '\n';
    }
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("solr_host_and_port keyword_normalization_map");

    const std::string SOLR_HOST_AND_PORT(argv[1]);
    const std::string OUTPUT_FILENAME(argv[2]);
    const auto output(FileUtil::OpenOutputFileOrDie(OUTPUT_FILENAME));

    std::unordered_map<std::string, std::vector<CapitalizationAndCount>> lowercase_form_to_capitalizations_and_counts_map;
    CollectStats(SOLR_HOST_AND_PORT, "topic_facet_de", &lowercase_form_to_capitalizations_and_counts_map);
    GenerateCanonizationMap(output.get(), lowercase_form_to_capitalizations_and_counts_map);

    return EXIT_SUCCESS;
}
