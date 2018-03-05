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
public:
    class Configuration {
    public:
        Url host_;
        std::string index_;
        std::string document_type_;

        /** \brief return Configuration by parsing global "Elasticsearch.conf" config file. */
        static std::shared_ptr<Elasticsearch::Configuration> FactoryByConfigFile();
    };

    typedef std::unordered_map<std::string, std::string> Fields;

    struct Document {
        std::string id_;
        std::string type_;
        Fields fields_;
    };

    struct IndexStatistics {
        unsigned document_count_;
    };

    typedef std::unordered_map<std::string, Document> IdToDocumentMap;

private:
    /** \brief Executes REST Query to Elasticsearch server
    *   \throws std::runtime_error on REST error or if API reports an error
    */
    static std::shared_ptr<JSON::ObjectNode> Query(const Url &host, const std::string &action, const REST::QueryType query_type, const std::shared_ptr<const JSON::JSONNode> &data = nullptr);
public:
    /** \brief Creates a new document.
    *   \throws std::runtime_error (see Query)
    */
    static void InsertDocument(const Url &host, const std::string &index, const Document &document);

    /** \brief Creates a new index
    *   \throws std::runtime_error (see Query)
    */
    static void CreateIndex(const Url &host, const std::string &index);

    /** \brief Deletes an existing document.
    *   \throws std::runtime_error (see Query)
    */
    static void DeleteDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id);

    /** \brief Deletes the given index.
    *   \throws std::runtime_error (see Query)
    */
    static void DeleteIndex(const Url &host, const std::string &index);

    /** \brief Gets an existing document
    *   \throws std::runtime_error (see Query)
    */
    static Document GetDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id);

    /** \brief Get IDs of all existing indices
    *   \throws std::runtime_error (see Query)
    */
    static std::vector<std::string> GetIndexList(const Url &host);

    /** \brief Get statistics for the given index
    *   \throws std::runtime_error (see Query)
    */
    static IndexStatistics GetIndexStatistics(const Url &host, const std::string &index);

    /** \brief Check if the given index has an ID with the given document type
    *   \throws std::runtime_error (see Query)
    */
    static bool HasDocument(const Url &host, const std::string &index, const std::string &type, const std::string &id);

    /** \brief copy all documents from source index to target index
    *   \throws std::runtime_error (see Query)
    */
    static void Reindex(const Url &host, const std::string &source_index, const std::string &target_index);

    /** \brief Search for all documents in the given index. If fields are given, documents need to match all specified fields.
    *   \throws std::runtime_error (see Query)
    */
    static IdToDocumentMap GetDocuments(const Url &host, const std::string &index, const Fields &fields = Fields());

    /** \brief Only provided fields will be overwritten (non-provided fields will NOT be deleted).
    *   \throws std::runtime_error (see Query)
    */
    static void UpdateDocument(const Url &host, const std::string &index, const Document &document);

    /** \brief Insert document if not exists, else update. On update, only given fields will be updated.
    *   \throws std::runtime_error (see Query)
    */
    static void UpdateOrInsertDocument(const Url &host, const std::string &index, const Document &document);

    class Index {
        Url host_;
        std::string index_;
    public:
        Index(const Url &host, const std::string &index) : host_(host), index_(index) {}

        void insertDocument(const Document &document) { return InsertDocument(host_, index_, document); }
        void deleteDocument(const std::string &type, const std::string &id) { return DeleteDocument(host_, index_, type, id); }
        Document getDocument(const std::string &type, const std::string &id) { return GetDocument(host_, index_, type, id); }
        IndexStatistics getStatistics() { return GetIndexStatistics(host_, index_); }
        bool hasDocument(const std::string &type, const std::string &id) { return HasDocument(host_, index_, type, id); }
        IdToDocumentMap getDocuments(const Fields &fields = Fields()) { return GetDocuments(host_, index_, fields); }
        void updateDocument(const Document &document) { return UpdateDocument(host_, index_, document); }
        void updateOrInsertDocument(const Document &document) { return UpdateOrInsertDocument(host_, index_, document); }
    };
}; // class Elasticsearch


#endif // ifndef ELASTICSEARCH_H
