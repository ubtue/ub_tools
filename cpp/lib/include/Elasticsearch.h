/** \file    Elasticsearch.h
 *  \brief   Interface for the Elasticsearch class.
 *  \author  Dr. Johannes Ruscheinski
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
#include <vector>


class Elasticsearch {
    std::string host_, index_, type_, username_, password_;
    bool ignore_ssl_certificates_;
public:
    /* \param  If provided, the config file must contain a section name "Elasticsearch" w/ entries name "host", "index", "type",
     *         "username" (optional), "password" (optional) and "ignore_ssl_certificates" (optional, defaults to "false").
     */
    explicit Elasticsearch(const std::string &ini_file_path = "");

    /** \brief Inserts a logical document into the Elasticsearch index.
     *  \param document_id      An ID that must be unique per document, e.g. a MARC control number.
     *  \param document_chunks  Text blobs that make up the contents of a document.
     *  \note  If a document w/ "document_id" already exists, it will be replaced.
     */
    void insertDocument(const std::string &document_id, const std::vector<std::string> &document_chunks);

    /** \brief Inserts a logical document into the Elasticsearch index.
     *  \param document_id      An ID that must be unique per document, e.g. a MARC control number.
     *  \param document         A text blob.
     *  \note  If a document w/ "document_id" already exists, it will be replaced.
     */
    inline void insertDocument(const std::string &document_id, const std::string &document) {
        insertDocument(document_id, std::vector<std::string>{ document });
    }
};
