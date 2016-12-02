/** \file   Solr.h
 *  \brief  Various utility functions relating to Apache Solr.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef SOLR_H
#define SOLR_H


#include <string>


namespace Solr {


enum QueryResultFormat { XML, JSON };


/** \brief Sends a query to a Solr instance.
 *  \param  query          The search query.
 *  \param  fields         The result fields that we want to get back.
 *  \param  xml_result     Here the returned XML will be stored.
 *  \param  host_and_port  Where we want to contact a Solr instance.
 *  \param  timeout        Up to how long, in seconds, we're willing to wait for a response.
 *  \return True if we got a valid response, else false.
 */
bool Query(const std::string &query, const std::string &fields, std::string * const xml_result,
           const std::string &host_and_port = "localhost:8080", const unsigned timeout = 10,
           const QueryResultFormat query_result_format = XML);


} // namespace Solr


#endif // ifndef SOLR_H
