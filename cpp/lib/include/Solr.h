/** \file   Solr.h
 *  \brief  Various utility functions relating to Apache Solr.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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


namespace Solr {


constexpr unsigned DEFAULT_TIMEOUT(10); // in s
constexpr unsigned JAVA_INT_MAX(2147483647);
const std::string DEFAULT_HOST("localhost");
const unsigned DEFAULT_PORT(8983);


enum QueryResultFormat { XML, JSON };

/** brief Sends a query to a Solr instance
 *  \param url        Complete query url
 *  \param  xml_or_json_result  Here the returned XML will be stored
 *  \param  err_msg             An error message will be stored here if anything goes.
 */
bool Query(const std::string &url, std::string * const xml_or_json_result, std::string * const err_msg,
           const unsigned timeout = DEFAULT_TIMEOUT, const QueryResultFormat query_result_format = XML);

/** \brief Sends a query to a Solr instance.
 *  \param  query               The search query.
 *  \param  fields              The result fields that we want to get back.
 *  \param  start_row           Row offset.
 *  \param  no_of_rows          Row count.
 *  \param  xml_or_json_result  Here the returned XML will be stored.
 *  \param  err_msg             An error message will be stored here if anything goes.
 *  \param  host                Where we want to contact a Solr instance.
 *  \param  port                Where we want to contact a Solr instance.
 *  \param  timeout             Up to how long, in seconds, we're willing to wait for a response.
 *  \return True if we got a valid response, else false.
 */
bool Query(const std::string &query, const std::string &fields, const unsigned start_row, const unsigned no_of_rows,
           std::string * const xml_or_json_result, std::string * const err_msg, const std::string &host = DEFAULT_HOST,
           const unsigned port = DEFAULT_PORT, const unsigned timeout = DEFAULT_TIMEOUT, const QueryResultFormat query_result_format = XML);


/** \brief Sends a query to a Solr instance.
 *  \param  query               The search query.
 *  \param  fields              The result fields that we want to get back.
 *  \param  xml_or_json_result  Here the returned XML will be stored.
 *  \param  err_msg             An error message will be stored here if anything goes.
 *  \param  host                Where we want to contact a Solr instance.
 *  \param  port                Where we want to contact a Solr instance.
 *  \param  timeout             Up to how long, in seconds, we're willing to wait for a response.
 *  \return True if we got a valid response, else false.
 */
inline bool Query(const std::string &query, const std::string &fields, std::string * const xml_or_json_result,
                  std::string * const err_msg, const std::string &host = DEFAULT_HOST, const unsigned port = DEFAULT_PORT,
                  const unsigned timeout = DEFAULT_TIMEOUT, const QueryResultFormat query_result_format = XML,
                  const unsigned max_no_of_rows = JAVA_INT_MAX)
{
    return Query(query, fields, /* start_row = */0, max_no_of_rows, xml_or_json_result, err_msg, host, port, timeout,
                 query_result_format);
}


bool Query(const std::string &query, const std::string &fields, std::string * const xml_or_json_result,
           std::string * const err_msg, const std::string &host_and_port,
           const unsigned timeout, const QueryResultFormat query_result_format = XML,
           const std::string &additional_raw_parameters = "");

} // namespace Solr
