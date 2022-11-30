/** \file   CORE.h
 *  \brief  Functions for downloading of web resources from CORE.
 *          CORE API, see https://api.core.ac.uk/docs/v3
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


#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>


namespace CORE {


enum EntityType { WORK, OUTPUT, DATA_PROVIDER, JOURNAL };


struct Author {
    std::string name_;

    Author() = default;
    Author(const nlohmann::json &json_obj);
};


struct Journal {
    std::string title_;
    std::vector<std::string> identifiers_;

    Journal() = default;
    Journal(const nlohmann::json &json_obj);
};


struct Language {
    std::string code_;
    std::string name_;

    Language() = default;
    Language(const nlohmann::json &json_obj);
};


class Entity {
protected:
    nlohmann::json json_;

    std::string getStringOrDefault(const std::string &json_key) const;

public:
    Entity(const nlohmann::json &json) { json_ = json; }

    std::string getFilteredReason() const;
    void setFilteredReason(const std::string &reason);

    nlohmann::json getJson() const { return json_; }
};


class DataProvider : public Entity {
public:
    // Additional properties available, e.g. location, logo, OAI PMH information...
    std::string getCreatedDate() const;
    std::string getEmail() const;
    std::string getHomepageUrl() const;
    unsigned long getId() const;
    std::string getMetadataFormat() const;
    std::string getName() const;
    std::string getType() const;

    using Entity::Entity;
};


class Work : public Entity {
    std::vector<nlohmann::json> getDataProviders() const;
    void setDataProviders(const std::vector<nlohmann::json> &new_dp_content);

public:
    std::string getAbstract() const;
    std::vector<Author> getAuthors() const;
    std::set<unsigned long> getDataProviderIds() const; // JSON also contains name field, but that always seems to be empty
    std::string getDocumentType() const;
    std::string getDownloadUrl() const;
    std::string getFieldOfStudy() const;
    unsigned long getId() const;
    std::vector<Journal> getJournals() const;
    Language getLanguage() const;
    std::string getPublisher() const;
    std::string getTitle() const;
    unsigned getYearPublished() const;

    void purgeDataProviders(const std::set<unsigned long> &data_provider_ids_to_keep);
    void removeDataProviders(const std::set<unsigned long> &data_provider_ids_to_remove);

    bool isArticle() const { return getJournals().empty(); }

    using Entity::Entity;
};


struct SearchParams {
    std::string q_;
    bool scroll_ = false; // use this mechanism if you expect > 10.000 results.
    unsigned offset_ = 0;
    unsigned limit_ = 10; // can be 100 due to documentation, but in practical terms even 1000.
    std::string scroll_id_;
    std::string entity_id_;
    EntityType entity_type_;
    bool stats_ = false;
    bool raw_stats_ = false;
    std::vector<std::string> exclude_ = {}; // exclude e.g. "fullText" field from result for better performance.
    std::vector<std::string> sort_ = {};
    std::string accept_;
    bool measure_ = false;

    const std::string buildUrl() const;
};


struct SearchParamsDataProviders : public SearchParams {
    SearchParamsDataProviders() { entity_type_ = DATA_PROVIDER; }
};


struct SearchParamsWorks : public SearchParams {
    SearchParamsWorks() { entity_type_ = WORK; }
};


struct SearchResponse {
    unsigned total_hits_;
    unsigned limit_;
    unsigned offset_;
    std::string scroll_id_;
    std::vector<Entity> results_;
    std::vector<std::string> tooks_;
    unsigned es_took_;

    SearchResponse() = default;
    SearchResponse(const std::string &json);
};


struct SearchResponseDataProviders : public SearchResponse {
    std::vector<DataProvider> results_;

    SearchResponseDataProviders(const SearchResponse &response);
};


struct SearchResponseWorks : public SearchResponse {
    std::vector<Work> results_;

    SearchResponseWorks(const SearchResponse &response);
};


void DownloadWork(const unsigned id, const std::string &output_file);


/** \brief will search from offset_ to limit_ (only once). */
SearchResponseDataProviders SearchDataProviders(const SearchParamsDataProviders &params);
SearchResponseWorks SearchWorks(const SearchParamsWorks &params);
SearchResponse Search(const SearchParams &params);


/** \brief will search from offset_ to end in multiple searches
 *         and write JSON files to output dir.
 */
void SearchBatch(const SearchParams &params, const std::string &output_dir, const unsigned limit = 0);

/** \brief will search from offset_ to end in multiple searches
 *         and return a list with combined results.
 *         Rather use SearchBatch with output_dir if you expect big results.
 */
std::vector<Entity> SearchBatch(const SearchParams &params, const unsigned limit = 0);
std::vector<Work> SearchBatch(const SearchParamsWorks &params, const unsigned limit = 0);
std::vector<DataProvider> SearchBatch(const SearchParamsDataProviders &params, const unsigned limit = 0);

nlohmann::json ParseFile(const std::string &file);
std::vector<Entity> GetEntitiesFromFile(const std::string &file);
std::vector<Work> GetWorksFromFile(const std::string &file);


/** \brief Helper functions to create a JSON file with array of Entities. */
void OutputFileStart(const std::string &path);
void OutputFileAppend(const std::string &path, const Entity &entity, const bool first);
void OutputFileEnd(const std::string &path);


} // namespace CORE
