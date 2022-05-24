/** \file   CORE.h
 *  \brief  Functions for downloading of web resources from CORE.
 *  \author Mario Trojan (mario trojan@uni-tuebingen.de)
 *
 *  \copyright 2022 Tübingen University Library.
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


/** \class  CORE
 *  \brief  Implement CORE API, see https://api.core.ac.uk/docs/v3
 */
class CORE {
    static const std::string API_BASE_URL;
    static const std::string GetAPIKey();
public:
    enum EntityType { WORK, OUTPUT, DATA_PROVIDER, JOURNAL };
    static const std::string GetEndpointForEntityType(const EntityType type);

    struct Author {
        std::string name_;

        Author() = default;
        Author(const std::shared_ptr<const JSON::ObjectNode> json_obj);
    };

    struct Journal {
        std::string title_;
        std::vector<std::string> identifiers_;

        Journal() = default;
        Journal(const std::shared_ptr<const JSON::ObjectNode> json_obj);
    };

    struct Language {
        std::string code_;
        std::string name_;

        Language() = default;
        Language(const std::shared_ptr<const JSON::ObjectNode> json_obj);
    };

    struct Entity {};

    struct Work : Entity {
    public:
        std::string abstract_;
        std::vector<Author> authors_;
        std::string document_type_;
        std::string download_url_;
        std::string field_of_study_;
        unsigned long id_ = 0;
        std::vector<Journal> journals_;
        Language language_;
        std::string publisher_;
        std::string title_;
        unsigned year_published_ = 0;

        Work(const std::shared_ptr<const JSON::ObjectNode> json_obj);

        bool isArticle() const { return journals_.empty(); }
    };

    struct SearchParams {
        std::string q_;
        bool scroll_ = false;
        unsigned offset_ = 0;
        unsigned limit_ = 10;
        std::string scroll_id_;
        std::string entity_id_;
        EntityType entity_type_;
        bool stats_ = false;
        bool raw_stats_ = false;
        std::vector<std::string> exclude_ = {};
        std::vector<std::string> sort_ = {};
        std::string accept_;
        bool measure_ = false;

        const std::string buildUrl() const;
    };

    struct SearchParamsWorks : public SearchParams {
        EntityType entity_type_ = WORK;
    };

    struct SearchResponse {
        unsigned total_hits_;
        unsigned limit_;
        unsigned offset_;
        std::string scroll_id_;
        std::vector<std::shared_ptr<JSON::ObjectNode>> results_;
        std::vector<std::string> tooks_;
        unsigned es_took_;

        SearchResponse() = default;
        SearchResponse(const std::string &json);
    };

    struct SearchResponseWorks : public SearchResponse {
        std::vector<Work> results_;

        SearchResponseWorks(const SearchResponse &response);
    };

private:
    std::string download(const std::string &url);
    std::string searchRaw(const SearchParams &params);
    SearchResponse search(const SearchParams &params);

public:
    /** \brief will search from offset_ to limit_ (only once). */
    SearchResponseWorks searchWorks(const SearchParamsWorks &params);

    /** \brief will search from offset_ to end in multiple searches
     *         and write JSON files to output dir.
     */
    void searchBatch(const SearchParams &params, const std::string &output_dir);

    static std::shared_ptr<JSON::ArrayNode> GetResultsFromFile(const std::string &file);
    static std::vector<Work> GetWorksFromFile(const std::string &file);
};
