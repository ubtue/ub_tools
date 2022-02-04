/** \file   Solr.h
 *  \brief  Utility functions for extracting common fields from a Solr JSON response
 *  \author Johannes Riedl
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once

#include <string>
#include <vector>
#include "JSON.h"


namespace SolrJSON {


std::shared_ptr<const JSON::ArrayNode> ParseTreeAndGetDocs(const std::string &json_results);
std::string GetId(const std::shared_ptr<const JSON::ObjectNode> &doc_obj);
std::string GetTitle(const std::shared_ptr<const JSON::ObjectNode> &doc_obj);
std::vector<std::string> GetAuthors(const std::shared_ptr<const JSON::ObjectNode> &doc_obj);
std::string GetFirstPublishDate(const std::shared_ptr<const JSON::ObjectNode> &doc_obj);


} // namespace SolrJSON
