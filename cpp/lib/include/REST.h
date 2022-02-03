/** \file    REST.h
 *  \brief   Various utility functions for REST API's.
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
#pragma once


#include <string>
#include "Downloader.h"
#include "JSON.h"
#include "Url.h"


namespace REST {


enum QueryType { GET, PUT, POST, DELETE };


/** \brief  Executes a REST operation of a given type by using Downloader
 *  \return The response body.
 *  \throws std::runtime_error if an error occurred.
 */
std::string Query(const Url &url, const QueryType query_type, const std::string &data = "",
                  const Downloader::Params &params = Downloader::Params());


/** \brief  Same as "Query", but takes JSON as request body and delivers JSON result object.
 *  \return The response body as parsed JSONNode.
 *  \throws std::runtime_error if an error occurred
 *          (e.g. Downloader problems, or response could not be parsed as JSON)
 */
std::shared_ptr<JSON::JSONNode> QueryJSON(const Url &url, const QueryType query_type, const JSON::JSONNode * const data = nullptr,
                                          const Downloader::Params &params = Downloader::Params());


} // namespace REST
