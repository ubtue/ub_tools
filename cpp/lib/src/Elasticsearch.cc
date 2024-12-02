/** \file    Elasticsearch.cc
 *  \brief   Implementation of the Elasticsearch class.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Mario Trojan
 *
 *  \copyright 2018-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Elasticsearch.h"
#include <climits>
#include "FileUtil.h"
#include "IniFile.h"
#include "UBTools.h"
#include "Url.h"
#include "UrlUtil.h"


const std::string DEFAULT_CONFIG_FILE_PATH(UBTools::GetTuelibPath() + "Elasticsearch.conf");


static void LoadIniParameters(const std::string &config_file_path, std::string * const host, std::string * const username,
                              std::string * const password, bool * const ignore_ssl_certificates) {
    if (not FileUtil::Exists(config_file_path))
        LOG_ERROR("Elasticsearch config file missing: " + config_file_path);

    const IniFile ini_file(config_file_path);

    *host = ini_file.getString("Elasticsearch", "host");
    *username = ini_file.getString("Elasticsearch", "username", "");
    *password = ini_file.getString("Elasticsearch", "password", "");
    *ignore_ssl_certificates = ini_file.getBool("Elasticsearch", "ignore_ssl_certificates", false);
}


Elasticsearch::Elasticsearch(const std::string &index): index_(index) {
    LoadIniParameters(DEFAULT_CONFIG_FILE_PATH, &host_, &username_, &password_, &ignore_ssl_certificates_);
}


size_t Elasticsearch::size() const {
    return JSON::LookupInteger("/indices/" + index_ + "/total/docs/count", query("_stats", REST::GET, JSON::ObjectNode()));
}


size_t Elasticsearch::count(const std::map<std::string, std::string> &fields_and_values) const {
    std::string subquery("_count");
    unsigned param_counter(0);
    for (const auto &field_and_value : fields_and_values) {
        if (param_counter == 0)
            subquery += "?q=";
        else
            subquery += "&";

        subquery += UrlUtil::UrlEncode(field_and_value.first) + ":" + UrlUtil::UrlEncode(field_and_value.second);
        ++param_counter;
    }
    return JSON::LookupInteger("/count", query(subquery, REST::GET, JSON::ObjectNode()));
}


void Elasticsearch::simpleInsert(const std::map<std::string, std::string> &fields_and_values) {
    query("_doc", REST::POST, JSON::ObjectNode(fields_and_values));
}


// A general comment as to the strategy we use in this function:
//
//    We know that there is an _update API endpoint, but as we insert a bunch of chunks, the number of which can change,
//    we have to use a "wildcard" strategy to first delete possibly existing chunks as the number of new chunks may be greater or
//    fewer than what may already be stored.
void Elasticsearch::insertOrUpdateDocument(const std::string &document_id, const std::string &document) {
    deleteDocument(document_id);
    simpleInsert({ { "id", document_id }, { "document", document } });
}


bool Elasticsearch::deleteDocument(const std::string &document_id) {
    const JSON::ObjectNode match_node("{ \"query\":"
                                      "    { \"match\":"
                                      "        { \"id\": \"" + JSON::EscapeString(document_id) + "\" }"
                                      "    }"
                                      "}");
    const auto result_node(query("_delete_by_query", REST::POST, match_node));
    return result_node->getIntegerNode("deleted")->getValue() >= 0;
}


std::unordered_set<std::string> Elasticsearch::selectAll(const std::string &field) const {
    const std::vector<std::map<std::string, std::string>> result(simpleSelect({ field }, {}, UINT_MAX));
    std::unordered_set<std::string> unique_values;
    for (const auto &map : result) {
        const auto key_and_value(map.find(field));
        if (key_and_value != map.cend())
            unique_values.emplace(key_and_value->second);
    }

    return unique_values;
}


std::unordered_multiset<std::string> Elasticsearch::selectAllNonUnique(const std::string &field) const {
    const std::vector<std::map<std::string, std::string>> result(simpleSelect({ field }, {}, UINT_MAX));
    std::unordered_multiset<std::string> values;
    for (const auto &map : result) {
        const auto key_and_value(map.find(field));
        if (key_and_value != map.cend())
            values.emplace(key_and_value->second);
    }

    return values;
}


std::vector<std::map<std::string, std::string>> Elasticsearch::extractResultsHelper(const std::shared_ptr<JSON::ObjectNode> &result_node,
                                                                                    const std::set<std::string> &fields) const {
    const auto hits_object_node(result_node->getObjectNode("hits"));
    if (unlikely(hits_object_node == nullptr))
        LOG_ERROR("missing \"hits\" object node in Elasticsearch result node!");
    const auto hits_array_node(hits_object_node->getArrayNode("hits"));
    if (unlikely(hits_array_node == nullptr))
        LOG_ERROR("missing \"hits\" array node in Elasticsearch result node!");

    std::vector<std::map<std::string, std::string>> search_results;
    search_results.reserve(hits_array_node->size());
    for (const auto &entry_node : *hits_array_node) {
        std::map<std::string, std::string> new_map;
        const auto entry_object_node(JSON::JSONNode::CastToObjectNodeOrDie("entry_object_node", entry_node));
        for (const auto &entry : *entry_object_node) {
            if (fields.empty() or fields.find(entry.first) != fields.cend() or entry.first == "_source") {
                // Copy existing fields but flatten the contents of _source
                if (entry.first == "_source") {
                    const auto source_object_node(JSON::JSONNode::CastToObjectNodeOrDie("source_object_node", entry.second));
                    for (const auto &source_entry : *source_object_node)
                        new_map[source_entry.first] =
                            JSON::JSONNode::CastToStringNodeOrDie("new_map[source_entry.first]", source_entry.second)->getValue();
                } else
                    new_map[entry.first] = JSON::JSONNode::CastToStringNodeOrDie("new_map[entry.first]", entry.second)->getValue();
            }
        }

        search_results.resize(search_results.size() + 1);
        std::swap(search_results.back(), new_map);
    }

    return search_results;
}


std::string Elasticsearch::extractScrollId(const std::shared_ptr<JSON::ObjectNode> &result_node) const {
    const auto scroll_id_string_node(result_node->getStringNode("_scroll_id"));
    if (unlikely(scroll_id_string_node == nullptr))
        LOG_ERROR("missing \"_scroll_id\" string node in Elasticsearch result");
    return scroll_id_string_node->getValue();
}


std::vector<std::map<std::string, std::string>> Elasticsearch::simpleSelect(const std::set<std::string> &fields,
                                                                            const std::map<std::string, std::string> &filter,
                                                                            const unsigned int max_count) const {
    const unsigned int MAX_RESULTS_PER_REQUEST(10000); // Elasticsearch Default
    const bool use_scrolling(max_count > MAX_RESULTS_PER_REQUEST);
    std::string query_string("{\n");

    if (not fields.empty()) {
        query_string += "    \"_source\": [";
        for (const auto &field : fields)
            query_string += "\"" + field + "\", ";
        query_string.resize(query_string.size() - 2); // Remove trailing comma and space.
        query_string += "],\n";
    }

    query_string += "    \"query\": {";

    if (filter.empty())
        query_string += " \"match_all\": {}";
    else {
        query_string += "\"bool\" : { \"filter\": [\n";
        for (const auto &field_and_value : filter)
            query_string += "       { \"term\": { \"" + field_and_value.first + "\": \"" + field_and_value.second + "\"} },\n";
        query_string.resize(query_string.size() - 2); // Remove the last comma and newline.
        query_string += "\n    ] }";
    }

    query_string += "    },\n";
    query_string += "    \"size\": " + std::to_string(use_scrolling ? MAX_RESULTS_PER_REQUEST : max_count) + "\n";
    query_string += "}\n";

    const std::string search_parameter(use_scrolling ? "_search?scroll=1m" : "_search");
    auto result_node(query(search_parameter, REST::POST, JSON::ObjectNode(query_string)));

    if (use_scrolling) {
        std::vector<std::map<std::string, std::string>> search_results_all;
        std::vector<std::map<std::string, std::string>> search_results_bunch(extractResultsHelper(result_node, fields));
        // Iterate until hits are empty
        while (search_results_bunch.size()) {
            search_results_all.insert(std::end(search_results_all), std::begin(search_results_bunch), std::end(search_results_bunch));
            std::string scroll_id(extractScrollId(result_node));
            result_node =
                query("_search/scroll", REST::POST, JSON::ObjectNode("{ \"scroll\": \"1m\", \"scroll_id\" : \"" + scroll_id + "\"}"),
                      true /* suppress index name */);
            search_results_bunch = extractResultsHelper(result_node, fields);
        }
        return search_results_all;
    }
    return extractResultsHelper(result_node, fields);
}


static std::string ToString(const Elasticsearch::RangeOperator op) {
    switch (op) {
    case Elasticsearch::RO_GT:
        return "gt";
    case Elasticsearch::RO_GTE:
        return "gte";
    case Elasticsearch::RO_LT:
        return "lt";
    case Elasticsearch::RO_LTE:
        return "lte";
    default:
        LOG_ERROR("we should *never* get here!");
    }
}


bool Elasticsearch::deleteRange(const std::string &field, const RangeOperator operator1, const std::string &operand1,
                                const RangeOperator operator2, const std::string &operand2) {
    const std::string range_node((operator2 == RO_NOOP or operand2.empty()) ?
                                    "{ \"query\":"
                                    "    { \"range\":"
                                    "        { \"" + field + "\": {"
                                    "            \"" + ToString(operator1) + "\": \"" + operand1 + "\""
                                        + ((operator2 == RO_NOOP or operand2.empty())
                                               ? std::string()
                                               : "      ,\"" + ToString(operator2) + "\": \"" + operand2 + "\"") +
                                    "        }}"
                                    "    }"
                                    "}"
                                                                            :
                                    "");
    const auto result_node(query("_delete_by_query", REST::POST, range_node));
    return result_node->getIntegerNode("deleted")->getValue() > 0;
}


bool Elasticsearch::fieldWithValueExists(const std::string &field, const std::string &value) {
    const auto result_node(query("_search", REST::POST,
                                 "{"
                                 "    \"query\": {"
                                 "        \"match\" : { \""
                                     + field + "\" : \"" + value
                                     + "\" }"
                                       "    }"
                                       "}"));

    const auto hits_node(result_node->getObjectNode("hits"));
    if (hits_node == nullptr)
        LOG_ERROR("No \"hits\" node in results");

    const auto total_node = hits_node->getIntegerNode("total");
    if (total_node == nullptr)
        LOG_ERROR("No \"total\" node found");
    return total_node->getValue() == 0 ? false : true;
}


std::shared_ptr<JSON::ObjectNode> Elasticsearch::query(const std::string &action, const REST::QueryType query_type,
                                                       const JSON::ObjectNode &data, const bool suppress_index_name) const {
    Downloader::Params downloader_params;
    downloader_params.authentication_username_ = username_;
    downloader_params.authentication_password_ = password_;
    downloader_params.ignore_ssl_certificates_ = ignore_ssl_certificates_;
    downloader_params.additional_headers_.push_back("Content-Type: application/json");
    Url url;
    url = Url(host_ + (not suppress_index_name ? "/" + index_ : "") + (action.empty() ? "" : "/" + action));

    std::shared_ptr<JSON::JSONNode> result(REST::QueryJSON(url, query_type, &data, downloader_params));
    std::shared_ptr<JSON::ObjectNode> result_object(JSON::JSONNode::CastToObjectNodeOrDie("Elasticsearch result", result));
    if (result_object->hasNode("error"))
        LOG_ERROR("Elasticsearch " + action + " query failed: " + result_object->getNode("error")->toString());

    return result_object;
}
