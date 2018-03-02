/** \file    Elasticsearch.cc
 *  \brief   Implementation of utility functions relating to Elasticsearch.
 *  \author  Mario Trojan
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
#include "Elasticsearch.h"
#include <memory>
#include "FileUtil.h"
#include "IniFile.h"


std::shared_ptr<Elasticsearch::Configuration> Elasticsearch::Configuration::FactoryByConfigFile() {
    const std::string ini_path("/usr/local/var/lib/tuelib/Elasticsearch.conf");
    if (not FileUtil::Exists(ini_path))
        ERROR("config file missing: " + ini_path);

    const IniFile ini_file(ini_path);

    std::shared_ptr<Elasticsearch::Configuration> config(new Elasticsearch::Configuration);
    config->host_ = Url(ini_file.getString("Elasticsearch", "host"));
    config->index_ = ini_file.getString("Elasticsearch", "index");
    config->document_type_ = ini_file.getString("Elasticsearch", "document_type");
    return config;
}


std::shared_ptr<JSON::ObjectNode> FieldsToJSON(const Elasticsearch::Fields &fields) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    for (const auto &field : fields) {
        std::shared_ptr<JSON::StringNode> value_node(new JSON::StringNode(field.second));
        tree_root->insert(field.first, value_node);
    }
    return tree_root;
}


Elasticsearch::Fields JSONToFields(const std::shared_ptr<const JSON::ObjectNode> &json_object) {
    Elasticsearch::Fields fields;
    for (const auto &key_and_value : *json_object) {
        std::shared_ptr<JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie(key_and_value.first, key_and_value.second));
        fields[key_and_value.first] = string_node->getValue();
    }
    return fields;
}


/** \brief Sends REST queries to Elasticsearch server
*   \throws std::runtime_error on REST error or if the JSON response contained an error flag.
*/
std::shared_ptr<JSON::ObjectNode> Elasticsearch::Api::Query(const Url &host, const std::string &action, const REST::QueryType query_type, const std::shared_ptr<const JSON::JSONNode> &data) {
    Url url(host.toString() + "/" + action);
    Downloader::Params params;
    if (data != nullptr)
        params.additional_headers_.push_back("Content-Type: application/json");
    std::shared_ptr<JSON::JSONNode> result(REST::QueryJSON(url, query_type, data, params));
    std::shared_ptr<JSON::ObjectNode> result_object(JSON::JSONNode::CastToObjectNodeOrDie("Elasticsearch result", result));
    if (result_object->hasNode("error"))
        throw std::runtime_error("in Elasticsearch::query: " + result_object->getNode("error")->toString());

    DEBUG(result_object->toString());

    return result_object;
}


void Elasticsearch::Api::InsertDocument(const Url &host, const std::string &index, const Document &document) {
    const std::shared_ptr<JSON::ObjectNode> tree_root(FieldsToJSON(document.fields_));
    const std::string action(index + "/" + document.type_ + "/" + document.id_ + "?op_type=create");
    Query(host, action, REST::QueryType::PUT, tree_root);
}


void Elasticsearch::Api::CreateIndex(const Url &host, const std::string &index) {
    Query(host, index, REST::QueryType::PUT);
}


void Elasticsearch::Api::DeleteDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id) {
    const std::string action(index + "/" + type + "/" + id);
    Query(host, action, REST::QueryType::DELETE);
}


void Elasticsearch::Api::DeleteIndex(const Url &host, const std::string &index) {
    Query(host, index, REST::QueryType::DELETE);
}


Elasticsearch::Document Elasticsearch::Api::GetDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id) {
    const std::string action(index + "/" + type + "/" + id);
    std::shared_ptr<JSON::ObjectNode> result(Query(host, action, REST::QueryType::GET));
    bool found(result->getOptionalBooleanValue("found", false));
    if (not found)
        throw std::runtime_error("in Elasticsearch::getDocument: document not found!" + result->toString());

    Document document;
    document.id_ = id;
    document.type_ = type;
    document.fields_ = JSONToFields(result->getObjectNode("_source"));
    return document;
}


std::vector<std::string> Elasticsearch::Api::GetIndexList(const Url &host) {
    const std::string action("_cluster/health?level=indices");
    const std::shared_ptr<const JSON::ObjectNode> result_node(Query(host, action, REST::QueryType::GET));

    if (not result_node->hasNode("indices"))
        throw std::runtime_error("in Elasticsearch::getIndexList: indices key not found in result: " + result_node->toString());

    const std::shared_ptr<const JSON::ObjectNode> index_list_node(result_node->getObjectNode("indices"));
    std::vector<std::string> index_list;
    for (const auto &key_and_node : *index_list_node)
        index_list.push_back(key_and_node.first);

    return index_list;
}


Elasticsearch::IndexStatistics Elasticsearch::Api::GetIndexStatistics(const Url &host, const std::string &index) {
    const std::string action(index + "/_stats");
    const std::shared_ptr<const JSON::ObjectNode> result_object(Query(host, action, REST::QueryType::GET));
    const std::shared_ptr<const JSON::ObjectNode> indices_object(result_object->getObjectNode("indices"));
    const std::shared_ptr<const JSON::ObjectNode> index_object(indices_object->getObjectNode(index));
    const std::shared_ptr<const JSON::ObjectNode> total_object(index_object->getObjectNode("total"));
    const std::shared_ptr<const JSON::ObjectNode> docs_object(total_object->getObjectNode("docs"));
    IndexStatistics stats;
    stats.document_count_ = static_cast<unsigned>(docs_object->getIntegerValue("count"));
    return stats;
}


bool Elasticsearch::Api::HasDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id) {
    const std::string action(index + "/" + type + "/" + id);
    const std::shared_ptr<const JSON::ObjectNode> result(Query(host, action, REST::QueryType::GET));
    return result->getOptionalBooleanValue("found", false);
}


void Elasticsearch::Api::Reindex(const Url &host, const std::string &source_index, const std::string &target_index) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);

    std::shared_ptr<JSON::ObjectNode> source_node(new JSON::ObjectNode);
    std::shared_ptr<JSON::StringNode> source_index_node(new JSON::StringNode(source_index));
    source_node->insert("index", source_index_node);
    tree_root->insert("source", source_node);

    std::shared_ptr<JSON::ObjectNode> dest_node(new JSON::ObjectNode);
    std::shared_ptr<JSON::StringNode> dest_index_node(new JSON::StringNode(target_index));
    dest_node->insert("index", dest_index_node);
    tree_root->insert("dest", dest_node);

    Query(host, "_reindex", REST::QueryType::POST, tree_root);
}


Elasticsearch::IdToDocumentMap Elasticsearch::Api::GetDocuments(const Url &host, const std::string &index) {
    const std::string action(index + "/_search");

    const std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    std::shared_ptr<JSON::ObjectNode> query_node(new JSON::ObjectNode);
    tree_root->insert("query", query_node);
    std::shared_ptr<JSON::ObjectNode> match_all_node(new JSON::ObjectNode);
    query_node->insert("match_all", match_all_node);

    const std::shared_ptr<const JSON::ObjectNode> result_node(Query(host, action, REST::QueryType::GET, tree_root));

    IdToDocumentMap documents;
    if (result_node->hasNode("hits")) {
        const std::shared_ptr<const JSON::ObjectNode> hits_object(result_node->getObjectNode("hits"));
        const std::shared_ptr<const JSON::ArrayNode> hits_array(hits_object->getArrayNode("hits"));
        for (const auto &hit_node : *hits_array) {
            const std::shared_ptr<const JSON::ObjectNode> hit_object(JSON::JSONNode::CastToObjectNodeOrDie("hit", hit_node));
            Document document;
            document.id_ = hit_object->getStringValue("_id");
            if (hit_object->hasNode("_source")) {
                const std::shared_ptr<const JSON::ObjectNode> fields_object(hit_object->getObjectNode("_source"));
                document.fields_ = JSONToFields(fields_object);
            }
            documents[document.id_] = document;
        }
    }
    return documents;
}


void Elasticsearch::Api::UpdateDocument(const Url &host, const std::string &index, const Document &document) {
    std::shared_ptr<JSON::ObjectNode> doc_node(FieldsToJSON(document.fields_));
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    tree_root->insert("doc", doc_node);

    const std::string action(index + "/" + document.type_ + "/" + document.id_ + "/_update");
    Query(host, action, REST::QueryType::POST, tree_root);
}


void Elasticsearch::Api::UpdateOrInsertDocument(const Url &host, const std::string &index, const Document &document) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    std::shared_ptr<JSON::ObjectNode> doc_node(FieldsToJSON(document.fields_));
    tree_root->insert("doc", doc_node);
    std::shared_ptr<JSON::BooleanNode> doc_as_upsert_node(new JSON::BooleanNode(true));
    tree_root->insert("doc_as_upsert", doc_as_upsert_node);

    const std::string action(index + "/" + document.type_ + "/" + document.id_ + "/_update");
    Query(host, action, REST::QueryType::POST, tree_root);
}
