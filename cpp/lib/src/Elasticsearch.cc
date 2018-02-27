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


std::shared_ptr<JSON::ObjectNode> Elasticsearch::FieldsToJSON(const Fields &fields) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    for (const auto &field : fields) {
        std::shared_ptr<JSON::StringNode> value_node(new JSON::StringNode(field.second));
        tree_root->insert(field.first, value_node);
    }
    return tree_root;
}


Elasticsearch::Fields Elasticsearch::JSONToFields(const std::shared_ptr<const JSON::ObjectNode> &json_object) {
    Fields fields;
    for (const auto &key_and_value : *json_object) {
        std::shared_ptr<JSON::StringNode> string_node(JSON::JSONNode::CastToStringNodeOrDie(key_and_value.first, key_and_value.second));
        fields[key_and_value.first] = string_node->getValue();
    }
    return fields;
}


std::shared_ptr<JSON::ObjectNode> Elasticsearch::query(const std::string &action, const REST::QueryType query_type, const std::shared_ptr<const JSON::JSONNode> &data) {
    Url url(host_.toString() + "/" + action);
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


void Elasticsearch::createDocument(const Document &document) {
    const std::shared_ptr<JSON::ObjectNode> tree_root(FieldsToJSON(document.fields_));
    const std::string action(index_ + "/" + document_type_ + "/" + document.id_ + "?op_type=create");
    query(action, REST::QueryType::PUT, tree_root);
}


void Elasticsearch::createIndex() {
    query(index_, REST::QueryType::PUT);
}


void Elasticsearch::deleteDocument(const std::string &id) {
    const std::string action(index_ + "/" + document_type_ + "/" + id);
    query(action, REST::QueryType::DELETE);
}


void Elasticsearch::deleteIndex() {
    query(index_, REST::QueryType::DELETE);
}


Elasticsearch::Document Elasticsearch::getDocument(const std::string &id) {
    const std::string action(index_ + "/" + document_type_ + "/" + id);
    std::shared_ptr<JSON::ObjectNode> result(query(action, REST::QueryType::GET));
    bool found(result->getOptionalBooleanValue("found", false));
    if (not found)
        throw std::runtime_error("in Elasticsearch::getDocument: document not found!" + result->toString());

    Document document;
    document.id_ = id;
    document.fields_ = JSONToFields(result->getObjectNode("_source"));
    return document;
}


std::vector<std::string> Elasticsearch::getIndexList() {
    const std::string action("_cluster/health?level=indices");
    const std::shared_ptr<const JSON::ObjectNode> result_node(query(action, REST::QueryType::GET));

    if (not result_node->hasNode("indices"))
        throw std::runtime_error("in Elasticsearch::getIndexList: indices key not found in result: " + result_node->toString());

    const std::shared_ptr<const JSON::ObjectNode> index_list_node(result_node->getObjectNode("indices"));
    std::vector<std::string> index_list;
    for (const auto &key_and_node : *index_list_node)
        index_list.push_back(key_and_node.first);

    return index_list;
}


Elasticsearch::IndexStatistics Elasticsearch::getIndexStatistics() {
    const std::string action(index_ + "/_stats");
    const std::shared_ptr<const JSON::ObjectNode> result_object(query(action, REST::QueryType::GET));
    const std::shared_ptr<const JSON::ObjectNode> indices_object(result_object->getObjectNode("indices"));
    const std::shared_ptr<const JSON::ObjectNode> index_object(indices_object->getObjectNode(index_));
    const std::shared_ptr<const JSON::ObjectNode> total_object(index_object->getObjectNode("total"));
    const std::shared_ptr<const JSON::ObjectNode> docs_object(total_object->getObjectNode("docs"));
    IndexStatistics stats;
    stats.document_count_ = static_cast<unsigned>(docs_object->getIntegerValue("count"));
    return stats;
}


bool Elasticsearch::hasDocument(const std::string &id) {
    const std::string action(index_ + "/" + document_type_ + "/" + id);
    const std::shared_ptr<const JSON::ObjectNode> result(query(action, REST::QueryType::GET));
    return result->getOptionalBooleanValue("found", false);
}


Elasticsearch::Documents Elasticsearch::searchAllDocuments() {
    const std::string action(index_ + "/_search");

    const std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    std::shared_ptr<JSON::ObjectNode> query_node(new JSON::ObjectNode);
    tree_root->insert("query", query_node);
    std::shared_ptr<JSON::ObjectNode> match_all_node(new JSON::ObjectNode);
    query_node->insert("match_all", match_all_node);

    const std::shared_ptr<const JSON::ObjectNode> result_node(query(action, REST::QueryType::GET, tree_root));

    Documents documents;
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


void Elasticsearch::updateDocument(const Document &document) {
    std::shared_ptr<JSON::ObjectNode> doc_node(FieldsToJSON(document.fields_));
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    tree_root->insert("doc", doc_node);

    const std::string action(index_ + "/" + document_type_ + "/" + document.id_ + "/_update");
    query(action, REST::QueryType::POST, tree_root);
}


void Elasticsearch::updateOrInsertDocument(const Document &document) {
    std::shared_ptr<JSON::ObjectNode> tree_root(new JSON::ObjectNode);
    std::shared_ptr<JSON::ObjectNode> doc_node(FieldsToJSON(document.fields_));
    tree_root->insert("doc", doc_node);
    std::shared_ptr<JSON::BooleanNode> doc_as_upsert_node(new JSON::BooleanNode(true));
    tree_root->insert("doc_as_upsert", doc_as_upsert_node);

    const std::string action(index_ + "/" + document_type_ + "/" + document.id_ + "/_update");
    query(action, REST::QueryType::POST, tree_root);
}
