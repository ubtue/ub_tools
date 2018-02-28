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
#ifndef ELASTICSEARCH_H
#define ELASTICSEARCH_H


#include <string>
#include <vector>
#include "JSON.h"
#include "REST.h"
#include "Url.h"


class Elasticsearch {
    Url host_;
    std::string index_;
    std::string document_type_;
public:
    struct Credentials {
        Url host_;
        std::string index_;
        std::string document_type_;
    };

    Elasticsearch(const Url &host, const std::string &index, const std::string &document_type) :
                  host_(host), index_(index), document_type_(document_type) {}
    Elasticsearch(const Credentials credentials) : host_(credentials.host_),
                  index_(credentials.index_), document_type_(credentials.document_type_) {}

    /** \brief return Elasticsearch using settings from global "Elasticsearch.conf" config file. */
    static std::unique_ptr<Elasticsearch> FactoryByConfigFile();

    typedef std::unordered_map<std::string, std::string> Fields;

    struct Document {
        std::string id_;
        Fields fields_;
    };

    struct IndexStatistics {
        unsigned document_count_;
    };

    typedef std::unordered_map<std::string, Document> Documents;
private:
    static std::shared_ptr<JSON::ObjectNode> FieldsToJSON(const Fields &fields);
    static Fields JSONToFields(const std::shared_ptr<const JSON::ObjectNode> &json_object);

    /** \brief Sends REST queries to Elasticsearch server
     *  \throws std::runtime_error on REST error or if the JSON response contained an error flag.
     */
    std::shared_ptr<JSON::ObjectNode> query(const std::string &action, const REST::QueryType query_type, const std::shared_ptr<const JSON::JSONNode> &data = nullptr);
public:
    /** \brief Creates a new document.
     *  \throws error (see "query"), or if a document with this id already exists
     *          for the current index and document type
     */
    void createDocument(const Document &document);

    /** \brief Creates the current index
     *  \throws error (see "query"), or if the index already exists
     */
    void createIndex();

    /** \brief Deletes an existing document.
     *  \throws error (see "query"), or if the document doesn't exist
     */
    void deleteDocument(const std::string &id);

    /** \brief Deletes the current index.
     *  \throws error (see "query"), or if the index doesn't exist
     */
    void deleteIndex();

    /** \brief Gets an existing document
     *  \throws error (see "query"), or if the document doesn't exist
     */
    Document getDocument(const std::string &id);

    /** \brief Get IDs of all existing indices
     *  \throws error (see "query")
     */
    std::vector<std::string> getIndexList();

    /** \brief Get statistics for the current index
     *  \throws error (see "query"), or if the index doesn't exist
     */
    IndexStatistics getIndexStatistics();

    /** \brief Check if the current index has an id with the current document type
     *  \throws error (see "query")
     */
    bool hasDocument(const std::string &id);

    /** \brief copy all documents from source index to target index
     *  \throws error (see "query"), or if one of the indices doesn't exist
     */
    void reindex(const std::string &source_index, const std::string &target_index);

    /** \brief Search for all documents of the current type in the current index
     *  \throws error (see "query")
     */
    Documents searchAllDocuments();

    /** \brief Only provided fields will be overwritten (non-provided fields will NOT be deleted).
     *  \throws error (see "query"), or if the document doesn't exist
     */
    void updateDocument(const Document &document);

    /** \brief Insert document if not exists, else update. On update, only given fields will be updated.
     *  \throws error (see "query")
     */
    void updateOrInsertDocument(const Document &document);
}; // class Elasticsearch


#endif // ifndef ELASTICSEARCH_H
