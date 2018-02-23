/** \file    Elasticsearch.cc
 *  \brief   Implementation of utility functions relating to Elasticsearch.
 *  \author  Mario Trojan
 *
 *  \copyright 2018 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "Downloader.h"


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
        fields.push_back({ key_and_value.first, key_and_value.second->toString()});
    return fields;
}


JSON::ObjectNode * Elasticsearch::query(const std::string &action, const REST::QueryType query_type, std::shared_ptr<JSON::JSONNode> data) {
    Url url(host_.toString() + "/" + action);
    Downloader::Params params;
    params.additional_headers_.push_back("Content-Type: application/json");
    JSON::JSONNode * result(REST::Query(url, query_type, data, params));
    JSON::ObjectNode * result_object(JSON::JSONNode::CastToObjectNodeOrDie("Elasticsearch result", result));
    if (result_object->hasNode("error"))
        throw std::runtime_error(result_object->getNode("error")->toString());

    return result_object;
}


void Elasticsearch::addDocument(const Document &document) {
    const std::shared_ptr<JSON::ObjectNode> tree_root(FieldsToJSON(document.fields_));
    std::string action(index_ + "/" + document_type_ + "/" + document.id_);
    query(action, REST::QueryType::PUT, tree_root);
}


Elasticsearch::Document Elasticsearch::getDocument(const std::string &id) {
    std::string action(index_ + "/" + document_type_ + "/" + id);
    const JSON::ObjectNode * const result(query(action, REST::QueryType::GET, nullptr));
    INFO(result->toString());
    Document document;
    document.id_ = id;
    document.fields_ = JSONToFields(result->getObjectNode("_source"));
    return document;
}
