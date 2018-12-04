/** \file    Elasticsearch.h
 *  \brief   Various utility functions relating to Elasticsearch.
 *  \author  Mario Trojan
 *  \see     https://www.elastic.co/guide/en/elasticsearch/reference/current/docs.html
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
#include "JSON.h"
#include "REST.h"
#include "Url.h"


namespace Elasticsearch {


class Configuration {
    static const std::string GLOBAL_CONFIG_FILE_PATH;
public:
    Url host_;
    std::string index_;
    std::string document_type_;
    std::string username_;
    std::string password_;
    bool ignore_ssl_certificates_;
public:
    Configuration(const std::string &config_file_path);
public:
    /** \brief return Configuration by parsing global "Elasticsearch.conf" config file. */
    static Configuration GlobalConfig() {
        return Configuration(GLOBAL_CONFIG_FILE_PATH);
    }
};


struct Document {
    std::string id_;
    std::string type_;
    std::unordered_map<std::string, std::string> fields_;
};


struct IndexStatistics {
    unsigned document_count_;
};


class Connection {
    Url host_;
    std::string username_;
    std::string password_;
    bool ignore_ssl_certificates_;

/** \brief Executes REST Query to Elasticsearch server
    *   \throws std::runtime_error on REST error or if API reports an error
    */
    std::shared_ptr<JSON::ObjectNode> query(const std::string &action, const REST::QueryType query_type,
                                            const std::shared_ptr<const JSON::JSONNode> &data = nullptr);
public:
    Connection(const Url &host, const std::string &username = "", const std::string &password = "", bool ignore_ssl_certificates = false)
        : host_(host), username_(username), password_(password), ignore_ssl_certificates_(ignore_ssl_certificates) {}
public:
    /** \brief Creates a new document.
     *  \throws std::runtime_error (\see Query)
     */
    void insertDocument(const std::string &index, const Document &document);

    /** \brief Creates a new index
     *  \throws std::runtime_error (\see Query)
     */
    void createIndex(const std::string &index);

    /** \brief Deletes an existing document.
     *  \throws std::runtime_error (\see Query)
     */
    void deleteDocument(const std::string &index, const std::string &type, const std::string &id);

    /** \brief Deletes the given index.
     *  \throws std::runtime_error (\see Query)
     */
    void deleteIndex(const std::string &index);

    /** \brief Gets an existing document
     *  \throws std::runtime_error (\see Query)
     */
    Document getDocument(const std::string &index, const std::string &type, const std::string &id);

    /** \brief Get IDs of all existing indices
     *  \throws std::runtime_error (\see Query)
     */
    std::vector<std::string> getIndexList();

    /** \brief Get statistics for the given index
     *  \throws std::runtime_error (\see Query)
     */
    IndexStatistics getIndexStatistics(const std::string &index);

    /** \brief Check if the given index has an ID with the given document type
     *  \throws std::runtime_error (\see Query)
    */
    bool hasDocument(const std::string &index, const std::string &type, const std::string &id);

    /** \brief copy all documents from source index to target index
     *  \throws std::runtime_error (\see Query)
     */
    void reindex(const std::string &source_index, const std::string &target_index);

    /** \brief Search for all documents in the given index. If fields are given, documents need to match all specified fields.
     *  \throws std::runtime_error (\see Query)
     */
    std::unordered_map<std::string, Document> getDocuments(const std::string &index,
                                                           const std::unordered_map<std::string, std::string> &fields
                                                               = std::unordered_map<std::string, std::string>());

    /** \brief Only provided fields will be overwritten (non-provided fields will NOT be deleted).
     *  \throws std::runtime_error (\see Query)
     */
    void updateDocument(const std::string &index, const Document &document);

    /** \brief Insert document if not exists, else update. On update, only given fields will be updated.
     *  \throws std::runtime_error (\see Query)
    */
    void updateOrInsertDocument(const std::string &index, const Document &document);
};


} // namespace Elasticsearch
