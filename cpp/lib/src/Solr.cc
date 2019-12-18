/** \file    Solr.cc
 *  \brief   Implementation of utility functions relating to Apache Solr.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2018, Library of the University of Tübingen

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
#include "Solr.h"
#include "Downloader.h"
#include "HttpHeader.h"
#include "JSON.h"
#include "UrlUtil.h"


namespace Solr {


std::string JSONError(const std::string &json_response) {
    try {
        JSON::Parser parser(json_response);
        std::shared_ptr<JSON::JSONNode> tree_root;
        if (not parser.parse(&tree_root) or tree_root->getType() != JSON::JSONNode::OBJECT_NODE)
            return "";
        const std::shared_ptr<const JSON::ObjectNode> top_level_object(JSON::JSONNode::CastToObjectNodeOrDie("tree_root",
                                                                                                             tree_root));
        const std::shared_ptr<const JSON::ObjectNode> error_object(top_level_object->getOptionalObjectNode("error"));
        return (error_object == nullptr) ? "" : error_object->toString();
    } catch (...) {
        return "";
    }
}


std::string XMLError(const std::string &xml_response) {
    // We can do better but for now we won't bother.
    return xml_response;
}


bool Query(const std::string &query, const std::string &fields, const unsigned start_row, const unsigned no_of_rows,
           std::string * const xml_or_json_result, std::string * const err_msg, const std::string &host_and_port, const unsigned timeout,
           const QueryResultFormat query_result_format)
{
    err_msg->clear();
    const std::string url("http://" + host_and_port + "/solr/biblio/select?q=" + UrlUtil::UrlEncode(query)
                          + "&wt=" + std::string(query_result_format == XML ? "xml" : "json")
                          + (fields.empty() ? "" : "&fl=" + fields) + "&rows=" + std::to_string(no_of_rows)
                          + (start_row == 0 ? "" : "&start=" + std::to_string(start_row)));

    Downloader downloader(url, Downloader::Params(), timeout * 1000);
    if (downloader.anErrorOccurred()) {
        *err_msg = downloader.getLastErrorMessage();
        return false;
    }
    *xml_or_json_result = downloader.getMessageBody();

    const HttpHeader header(downloader.getMessageHeader());
    if (header.getStatusCode() >= 200 and header.getStatusCode() <= 299)
        return  true;

    if (not xml_or_json_result->empty())
        *err_msg = (query_result_format == JSON) ? JSONError(*xml_or_json_result) : XMLError(*xml_or_json_result);
    return false;
}


} // namespace Solr
