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
#include <memory>
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
    static std::shared_ptr<JSON::ObjectNode> FieldsToJSON(const Fields fields);
    static Fields JSONToFields(const JSON::ObjectNode * const json_object);
    JSON::ObjectNode * query(const std::string &action, const REST::QueryType query_type, const std::shared_ptr<JSON::JSONNode> &data = nullptr);
public:
    void createDocument(const Document &document);
    void createIndex();
    void deleteDocument(const std::string &id);
    void deleteIndex();
    Document getDocument(const std::string &id);
    std::vector<std::string> getIndexList();
    IndexStatistics getIndexStatistics();
    bool hasDocument(const std::string &id);
    Documents searchAllDocuments();

    /** \brief Only provided fields will be overwritten (non-provided fields will NOT be deleted).*/
    void updateDocument(const Document &document);

}; // class Elasticsearch


#endif // ifndef ELASTICSEARCH_H
