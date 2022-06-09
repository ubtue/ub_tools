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


#include <string>
#include "Downloader.h"
#include "JSON.h"


namespace CORE {


enum EntityType { WORK, OUTPUT, DATA_PROVIDER, JOURNAL };


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


class Entity : public JSON::ObjectNode {
    using ObjectNode::ObjectNode;
};


class Work : public Entity {
public:
    std::string getAbstract() const;
    std::vector<Author> getAuthors() const;
    std::string getDocumentType() const;
    std::string getDownloadUrl() const;
    std::string getFieldOfStudy() const;
    unsigned long getId() const;
    std::vector<Journal> getJournals() const;
    Language getLanguage() const;
    std::string getPublisher() const;
    std::string getTitle() const;
    unsigned getYearPublished() const;

    bool isArticle() const { return getJournals().empty(); }

    using Entity::Entity;
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
    std::vector<Entity> results_;
    std::vector<std::string> tooks_;
    unsigned es_took_;

    SearchResponse() = default;
    SearchResponse(const std::string &json);
};


struct SearchResponseWorks : public SearchResponse {
    std::vector<Work> results_;

    SearchResponseWorks(const SearchResponse &response);
};


void DownloadWork(const unsigned id, const std::string &output_file);


/** \brief will search from offset_ to limit_ (only once). */
SearchResponseWorks SearchWorks(const SearchParamsWorks &params);


/** \brief will search from offset_ to end in multiple searches
 *         and write JSON files to output dir.
 */
void SearchBatch(const SearchParams &params, const std::string &output_dir);


std::shared_ptr<JSON::ArrayNode> GetResultsFromFile(const std::string &file);
std::vector<Work> GetWorksFromFile(const std::string &file);


void OutputFileStart(const std::string &path);
void OutputFileAppend(const std::string &path, const Entity &entity, const bool first);
void OutputFileEnd(const std::string &path);


} // namespace CORE
