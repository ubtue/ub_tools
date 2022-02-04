/** \file    SolrJSON.cc
 *  \brief   Utility functions for extracting common fields from a Solr JSON response
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2019 Library of the University of Tübingen

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

#include <string>
#include <vector>
#include "Compiler.h"
#include "JSON.h"
#include "SolrJSON.h"
#include "util.h"


namespace SolrJSON {


std::shared_ptr<const JSON::ArrayNode> ParseTreeAndGetDocs(const std::string &json_results) {
    JSON::Parser parser(json_results);
    std::shared_ptr<JSON::JSONNode> tree;
    if (not parser.parse(&tree))
        LOG_ERROR("JSON parser failed: " + parser.getErrorMessage());

    const std::shared_ptr<const JSON::ObjectNode> tree_obj(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", tree));
    const std::shared_ptr<const JSON::ObjectNode> response(tree_obj->getObjectNode("response"));
    return response->getArrayNode("docs");
}


std::string GetId(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string id(JSON::LookupString("/id", doc_obj, /* default_value = */ ""));
    if (unlikely(id.empty()))
        LOG_ERROR("Did not find 'id' node in JSON tree!");

    return id;
}


std::string GetTitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    const std::string NO_AVAILABLE_TITLE("*No available title*");
    const auto title(JSON::LookupString("/title", doc_obj, /* default_value = */ NO_AVAILABLE_TITLE));
    if (unlikely(title == NO_AVAILABLE_TITLE))
        LOG_WARNING("No title found for ID " + GetId(doc_obj) + "!");
    return title;
}


std::vector<std::string> GetAuthors(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    std::shared_ptr<const JSON::JSONNode> author(doc_obj->getNode("author"));
    // Try author2 if we did not succedd
    if (author == nullptr)
        author = doc_obj->getNode("author2");

    if (author == nullptr) {
        LOG_WARNING("\"author\" is null");
        return std::vector<std::string>();
    }

    const std::shared_ptr<const JSON::ArrayNode> author_array(JSON::JSONNode::CastToArrayNodeOrDie("author", author));

    if (author_array->empty()) {
        LOG_WARNING("\"author\" is empty");
        return std::vector<std::string>();
    }

    std::vector<std::string> authors;
    for (const auto &array_entry : *author_array) {
        const std::shared_ptr<const JSON::StringNode> author_string(JSON::JSONNode::CastToStringNodeOrDie("author string", array_entry));
        authors.emplace_back(author_string->getValue());
    }

    return authors;
}


std::string GetFirstPublishDate(const std::shared_ptr<const JSON::ObjectNode> &doc_obj) {
    std::shared_ptr<const JSON::JSONNode> publish_date(doc_obj->getNode("publishDate"));
    if (publish_date == nullptr) {
        LOG_WARNING("No publishDate found for ID " + GetId(doc_obj) + "!");
        return "";
    }

    const std::shared_ptr<const JSON::ArrayNode> publish_date_array(JSON::JSONNode::CastToArrayNodeOrDie("publishDate", publish_date));
    if (unlikely(publish_date_array->empty())) {
        LOG_WARNING("\"publishDate\" is empty");
        return "";
    }

    const std::shared_ptr<const JSON::StringNode> first_publish_date_string(
        JSON::JSONNode::CastToStringNodeOrDie("first_publish_date_string", *(publish_date_array->begin())));
    return first_publish_date_string->getValue();
}

} // namespace SolrJSON
