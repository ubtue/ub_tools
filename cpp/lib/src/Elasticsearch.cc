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
#include <cstdlib>


std::shared_ptr<JSON::ObjectNode> Elasticsearch::FieldsToJSON(const Fields fields) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    for (const auto &field : fields) {
        JSON::StringNode * value_node = new JSON::StringNode(field.second);
        tree_root->insert(field.first, value_node);
    }
    return tree_root;
}


Elasticsearch::Fields Elasticsearch::JSONToFields(const JSON::ObjectNode * const json_object) {
    Fields fields;
    for (const auto &key_and_value : *json_object)
        fields[key_and_value.first] = key_and_value.second->toString();
    return fields;
}


JSON::ObjectNode * Elasticsearch::query(const std::string &action, const REST::QueryType query_type, std::shared_ptr<JSON::JSONNode> data) {
    Url url(host_.toString() + "/" + action);
    Downloader::Params params;
    if (data != nullptr) {
        params.additional_headers_.push_back("Content-Type: application/json");
    }
    JSON::JSONNode * result(REST::Query(url, query_type, data, params));
    JSON::ObjectNode * result_object(JSON::JSONNode::CastToObjectNodeOrDie("Elasticsearch result", result));
    if (result_object->hasNode("error"))
        throw std::runtime_error(result_object->getNode("error")->toString());

    DEBUG(result_object->toString());

    return result_object;
}


void Elasticsearch::createDocument(const Document &document) {
    const std::shared_ptr<JSON::ObjectNode> tree_root(FieldsToJSON(document.fields_));
    std::string action(index_ + "/" + document_type_ + "/" + document.id_ + "?op_type=create");
    query(action, REST::QueryType::PUT, tree_root);
}


void Elasticsearch::createIndex() {
    std::string action(index_);
    query(action, REST::QueryType::PUT, nullptr);
}


void Elasticsearch::deleteDocument(const std::string &id) {
    std::string action(index_ + "/" + document_type_ + "/" + id);
    query(action, REST::QueryType::DELETE, nullptr);
}


void Elasticsearch::deleteIndex() {
    std::string action(index_);
    query(action, REST::QueryType::DELETE, nullptr);
}


Elasticsearch::Document Elasticsearch::getDocument(const std::string &id) {
    std::string action(index_ + "/" + document_type_ + "/" + id);
    const JSON::ObjectNode * const result(query(action, REST::QueryType::GET, nullptr));
    bool found(result->getOptionalBooleanValue("found", false));
    if (not found)
        throw std::runtime_error("document not found!" + result->toString());

    Document document;
    document.id_ = id;
    document.fields_ = JSONToFields(result->getObjectNode("_source"));
    return document;
}


std::vector<std::string> Elasticsearch::getIndexList() {
    std::string action("_cluster/health?level=indices");
    const JSON::ObjectNode * const result_node(query(action, REST::QueryType::GET, nullptr));

    if (not result_node->hasNode("indices"))
        throw std::runtime_error("indices key not found in result: " + result_node->toString());

    const JSON::ObjectNode * const index_list_node(result_node->getObjectNode("indices"));
    std::vector<std::string> index_list;
    for (const auto &key_and_node : *index_list_node)
        index_list.push_back(key_and_node.first);

    return index_list;
}


bool Elasticsearch::hasDocument(const std::string &id) {
    std::string action(index_ + "/" + document_type_ + "/" + id);
    const JSON::ObjectNode * const result(query(action, REST::QueryType::GET, nullptr));
    return result->getOptionalBooleanValue("found", false);
}


Elasticsearch::Documents Elasticsearch::searchAllDocuments() {
    std::string action(index_ + "/_search");

    const std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    JSON::ObjectNode * query_node(new JSON::ObjectNode);
    tree_root->insert("query", query_node);
    JSON::ObjectNode * match_all_node(new JSON::ObjectNode);
    query_node->insert("match_all", match_all_node);

    const JSON::ObjectNode * const result_node(query(action, REST::QueryType::GET, tree_root));

    Documents documents;
    if (result_node->hasNode("hits")) {
        const JSON::ObjectNode * const hits_object(result_node->getObjectNode("hits"));
        const JSON::ArrayNode * const hits_array(hits_object->getArrayNode("hits"));
        for (const auto &hit_node : *hits_array) {
            const JSON::ObjectNode * const hit_object(JSON::JSONNode::CastToObjectNodeOrDie("hit", hit_node));
            Document document;
            document.id_ = hit_object->getStringValue("_id");
            if (hit_object->hasNode("_source")) {
                const JSON::ObjectNode * const fields_object(hit_object->getObjectNode("_source"));
                document.fields_ = JSONToFields(fields_object);
            }
            documents[document.id_] = document;
        }
    }
    return documents;
}


void Elasticsearch::updateDocument(const Document &document) {
    const std::shared_ptr<JSON::ObjectNode> doc_node(FieldsToJSON(document.fields_));
    const std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    tree_root->insert("doc", doc_node.get());
    std::string action(index_ + "/" + document_type_ + "/" + document.id_ + "_update?pretty");
    query(action, REST::QueryType::POST, tree_root);
}
