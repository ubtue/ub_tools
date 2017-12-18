/** \file    Solr.cc
 *  \brief   Implementation of utility functions relating to Apache Solr.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include "UrlUtil.h"


namespace Solr {


bool Query(const std::string &query, const std::string &fields, std::string * const xml_result,
           const std::string &host_and_port, const unsigned timeout, const QueryResultFormat query_result_format)
{
    const std::string url("http://" + host_and_port + "/solr/biblio/select?q=" + UrlUtil::UrlEncode(query)
                          + "&wt=" + std::string(query_result_format == XML ? "xml" : "json")
                          + (fields.empty() ? "" : "&fl=" + fields) + "&rows=999999");
    return Download(url, timeout, xml_result) == 0;
}


} // namespace Solr
