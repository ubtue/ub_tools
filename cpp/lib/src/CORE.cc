/** \file   CORE.cc
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
#include <chrono>
#include <thread>
#include "CORE.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"


namespace CORE {


const std::string API_BASE_URL("https://api.core.ac.uk/v3/");


const std::string GetAPIKey() {
    static std::string key(StringUtil::TrimWhite(FileUtil::ReadStringOrDie(UBTools::GetTuelibPath() + "CORE-API.key")));
    return key;
}


const std::string GetEndpointForEntityType(const EntityType type) {
    switch (type) {
    case WORK:
        return "works";
    case OUTPUT:
        return "outputs";
    case DATA_PROVIDER:
        return "data-providers";
    case JOURNAL:
        return "journals";
    }
}


Author::Author(const nlohmann::json &json_obj) {
    if (json_obj["name"].is_string())
        name_ = json_obj["name"];
}


Journal::Journal(const nlohmann::json &json_obj) {
    if (json_obj["title"].is_string())
        title_ = json_obj["title"];
    if (json_obj["identifiers"].is_array()) {
        for (const auto &identifier : json_obj["identifiers"]) {
            identifiers_.emplace_back(identifier);
        }
    }
}


Language::Language(const nlohmann::json &json_obj) {
    if (json_obj["code"].is_string())
        code_ = json_obj["code"];
    if (json_obj["name"].is_string())
        name_ = json_obj["name"];
}


std::string Entity::getFilteredReason() const {
    return getStringOrDefault("filteredReason");
}


void Entity::setFilteredReason(const std::string &reason) {
    json_["filteredReason"] = reason;
}


std::string Entity::getStringOrDefault(const std::string &json_key) const {
    if (json_[json_key].is_string())
        return json_[json_key];
    return "";
}


std::string DataProvider::getCreatedDate() const {
    return getStringOrDefault("createdDate");
}


std::string DataProvider::getEmail() const {
    return getStringOrDefault("email");
}


std::string DataProvider::getHomepageUrl() const {
    return getStringOrDefault("homepageUrl");
}


unsigned long DataProvider::getId() const {
    return json_["id"];
}


std::string DataProvider::getMetadataFormat() const {
    return getStringOrDefault("metadataFormat");
}


std::string DataProvider::getName() const {
    return getStringOrDefault("name");
}


std::string DataProvider::getType() const {
    return getStringOrDefault("type");
}


std::string Work::getAbstract() const {
    return getStringOrDefault("abstract");
}


std::vector<Author> Work::getAuthors() const {
    std::vector<Author> result;
    const auto authors(json_["authors"]);
    if (authors.is_array()) {
        for (const auto &author_obj : authors) {
            Author author(author_obj);
            if (not author.name_.empty())
                result.emplace_back(author);
        }
    }
    return result;
}


std::set<unsigned long> Work::getDataProviderIds() const {
    std::set<unsigned long> ids;

    const auto data_providers(json_["dataProviders"]);
    if (data_providers.is_array()) {
        for (const auto &data_provider : data_providers) {
            ids.emplace(data_provider["id"]);
        }
    }

    return ids;
}
std::vector<nlohmann::json> Work::getDataProviders() {
    std::vector<nlohmann::json> new_data_providers;
    const auto data_providers(json_["dataProviders"]);
    if (data_providers.is_array()) {
        for (const auto &data_providers_item : data_providers) {
            new_data_providers.emplace_back(data_providers_item);
        }
    }
    return new_data_providers;
}

void Work::setDataProviders(const std::vector<nlohmann::json> &new_dp_content) {
    unsigned counter(0);
    json_["dataProviders"].clear();

    for (const auto &new_dp_content_item : new_dp_content) {
        json_["dataProviders"][counter] = new_dp_content_item;
        ++counter;
    }
}

void Work::removeDataProviders(const std::set<unsigned long> &data_provider_ids) {
    std::vector<nlohmann::json> new_data_provider;
    const auto data_providers = getDataProviders();
    for (const auto &data_provider : data_providers) {
        if (std::find(data_provider_ids.begin(), data_provider_ids.end(), data_provider["id"]) != data_provider_ids.end()) {
            new_data_provider.emplace_back(data_provider);
        }
    }
    setDataProviders(new_data_provider);
}


std::string Work::getDocumentType() const {
    return getStringOrDefault("documentType");
}


std::string Work::getDownloadUrl() const {
    return getStringOrDefault("downloadUrl");
}


std::string Work::getFieldOfStudy() const {
    return getStringOrDefault("fieldOfStudy");
}


unsigned long Work::getId() const {
    return json_["id"];
}


std::vector<Journal> Work::getJournals() const {
    std::vector<Journal> result;
    const auto journals(json_["journals"]);
    if (journals.is_array()) {
        for (const auto &journal_obj : journals) {
            result.emplace_back(Journal(journal_obj));
        }
    }
    return result;
}


Language Work::getLanguage() const {
    if (json_["language"].is_object())
        return Language(json_["language"]);
    Language default_language;
    return default_language;
}


std::string Work::getPublisher() const {
    return getStringOrDefault("publisher");
}


std::string Work::getTitle() const {
    return getStringOrDefault("title");
}


unsigned Work::getYearPublished() const {
    if (json_["yearPublished"].is_number())
        return json_["yearPublished"];
    return 0;
}


std::string Download(const std::string &url) {
    Downloader::Params downloader_params;
    downloader_params.additional_headers_.push_back("Authorization: Bearer " + GetAPIKey());
    downloader_params.fail_on_error_ = false; // needed to be able to parse headers even after failing
    // downloader_params.debugging_ = true;

    static Downloader downloader(downloader_params);
    downloader.newUrl(url);

    if (downloader.anErrorOccurred())
        LOG_ERROR(downloader.getLastErrorMessage() + ", curlcode: " + std::to_string(downloader.getLastErrorCode()));
    else if (downloader.getResponseCode() == 429) {
        const auto header(downloader.getMessageHeaderObject());
        if (header.getXRatelimitRemaining() == 0) {
            const time_t sleep_until_time(header.getXRatelimitRetryAfter("%Y-%m-%dT%H:%M:%S%z"));
            if (sleep_until_time != TimeUtil::BAD_TIME_T) {
                const std::string sleep_until_string(TimeUtil::TimeTToLocalTimeString(sleep_until_time));

                // We're allowed to perform 150 requests within a 5 minute window due to our license.
                // However, in practical terms responses might get a penalty e.g. when using the scroll mechanism,
                // so it's more likely to be 75.
                LOG_WARNING("Rate limiting active + too many requests! Sleeping until " + sleep_until_string);
                std::this_thread::sleep_until(std::chrono::system_clock::from_time_t(sleep_until_time));

                // sleep another 60 seconds just in case we have a system time difference between client and server
                // (even if timezone is already considered)
                const unsigned sleep_offset(60);
                LOG_WARNING("Sleeping another " + std::to_string(sleep_offset) + "s due to possible client/server system time difference");
                ::sleep(sleep_offset);
            } else {
                const unsigned sleep_seconds(60);
                LOG_WARNING("Rate limiting active + too many requests! Could not determine retry_after timestamp! Sleeping for "
                            + std::to_string(sleep_seconds) + "s");
                ::sleep(sleep_seconds);
            }
        }

        // try again after sleep
        return Download(url);
    } else if (downloader.getResponseCode() >= 400)
        LOG_ERROR("HTTP Error Code " + std::to_string(downloader.getResponseCode()) + "! Full header:" + downloader.getMessageHeader());
    else
        return downloader.getMessageBody();
}


void DownloadWork(const unsigned id, const std::string &output_file) {
    const std::string url(API_BASE_URL + GetEndpointForEntityType(WORK) + "/" + std::to_string(id));
    FileUtil::WriteStringOrDie(output_file, Download(url));
}


const std::string SearchParams::buildUrl() const {
    std::string url = API_BASE_URL + "search/" + UrlUtil::UrlEncode(GetEndpointForEntityType(entity_type_));
    url += "?q=" + UrlUtil::UrlEncode(q_);

    if (not scroll_id_.empty())
        url += "&scrollId=" + UrlUtil::UrlEncode(scroll_id_);
    else if (scroll_)
        url += "&scroll=true";
    else if (offset_ > 0)
        url += "&offset=" + std::to_string(offset_);

    if (limit_ > 0)
        url += "&limit=" + std::to_string(limit_);
    if (not entity_id_.empty())
        url += "&entity_id=" + UrlUtil::UrlEncode(entity_id_);
    if (stats_)
        url += "&stats";
    if (raw_stats_)
        url += "&raw_stats";
    if (exclude_.size() == 1) {
        url += "&exclude=" + UrlUtil::UrlEncode(exclude_[0]);
    } else {
        for (const auto &exclude : exclude_) {
            url += "&exclude[]=" + UrlUtil::UrlEncode(exclude);
        }
    }
    for (const auto &sort : sort_) {
        url += "&sort[]=" + UrlUtil::UrlEncode(sort);
    }
    if (not accept_.empty())
        url += "&accept=" + UrlUtil::UrlEncode(accept_);
    if (measure_)
        url += "&measure";

    return url;
}


// CORE switches, sometimes values are int, sometimes string (e.g. offset 0 and "100")
unsigned GetJsonUnsignedValue(const nlohmann::json &json, const std::string &label) {
    if (not json.contains(label))
        LOG_WARNING("Missing key: " + label + "\n" + json.dump());

    const auto child(json[label]);
    if (child.is_string())
        return StringUtil::ToUnsigned(child);
    else
        return child;
}


SearchResponse::SearchResponse(const std::string &json) {
    const auto json_obj(nlohmann::json::parse(json));

    total_hits_ = GetJsonUnsignedValue(json_obj, "totalHits");
    limit_ = GetJsonUnsignedValue(json_obj, "limit");
    offset_ = GetJsonUnsignedValue(json_obj, "offset");

    if (json_obj["scrollId"] != nullptr)
        scroll_id_ = json_obj["scrollId"];

    for (const auto &result : json_obj["results"]) {
        results_.emplace_back(result);
    }

    // TODO:
    // tooks
    // es_took
}


SearchResponseDataProviders::SearchResponseDataProviders(const SearchResponse &response) {
    total_hits_ = response.total_hits_;
    limit_ = response.limit_;
    offset_ = response.offset_;
    scroll_id_ = response.scroll_id_;
    tooks_ = response.tooks_;
    es_took_ = response.es_took_;

    for (const auto &result : response.results_) {
        results_.emplace_back(DataProvider(result.getJson()));
    }
}


SearchResponseWorks::SearchResponseWorks(const SearchResponse &response) {
    total_hits_ = response.total_hits_;
    limit_ = response.limit_;
    offset_ = response.offset_;
    scroll_id_ = response.scroll_id_;
    tooks_ = response.tooks_;
    es_took_ = response.es_took_;

    for (const auto &result : response.results_) {
        results_.emplace_back(Work(result.getJson()));
    }
}


std::string SearchRaw(const SearchParams &params) {
    const std::string url(params.buildUrl());
    return Download(url);
}


SearchResponse Search(const SearchParams &params) {
    const std::string json(SearchRaw(params));
    return SearchResponse(json);
}


SearchResponseDataProviders SearchDataProviders(const SearchParamsDataProviders &params) {
    const auto response_raw(Search(params));
    SearchResponseDataProviders response(response_raw);
    return response;
}


SearchResponseWorks SearchWorks(const SearchParamsWorks &params) {
    const auto response_raw(Search(params));
    SearchResponseWorks response(response_raw);
    return response;
}


void SearchBatchHelper(const SearchParams &params, const std::string &output_dir, std::vector<Entity> * const output_list,
                       const unsigned limit) {
    if (output_dir.empty() and output_list == nullptr)
        LOG_ERROR("You must either specify output_directory or output_list (or both)!");

    if (not output_dir.empty() and not FileUtil::IsDirectory(output_dir))
        FileUtil::MakeDirectoryOrDie(output_dir, /*recursive=*/true);
    SearchParams current_params = params;

    // Always enable scrolling. This is mandatory if we have > 10.000 results.
    current_params.scroll_ = true;

    if (limit > 0 && limit < current_params.limit_) {
        current_params.limit_ = limit;
    }

    std::string response_json(SearchRaw(current_params));
    SearchResponse response(response_json);

    int i(1);
    LOG_INFO("Downloaded entities " + std::to_string(response.offset_) + "-" + std::to_string(response.offset_ + response.limit_) + "/"
             + std::to_string(response.total_hits_) + ")");
    if (not output_dir.empty()) {
        const std::string output_file(output_dir + "/" + std::to_string(i) + ".json");
        FileUtil::WriteStringOrDie(output_file, response_json);
    }
    if (output_list != nullptr) {
        for (const auto &entity : response.results_) {
            output_list->emplace_back(entity);
        }
    }

    unsigned max_offset(response.total_hits_);
    if (limit > 0 && limit < max_offset)
        max_offset = limit;

    while (current_params.offset_ + current_params.limit_ < max_offset) {
        current_params.offset_ += current_params.limit_;
        if (current_params.offset_ + current_params.limit_ > max_offset)
            current_params.limit_ = max_offset - current_params.offset_;

        ++i;

        if (not response.scroll_id_.empty())
            current_params.scroll_id_ = response.scroll_id_;

        response_json = SearchRaw(current_params);
        response = SearchResponse(response_json);

        LOG_INFO("Downloaded entities " + std::to_string(current_params.offset_) + "-"
                 + std::to_string(current_params.offset_ + current_params.limit_) + "/" + std::to_string(response.total_hits_) + ")");
        if (not output_dir.empty()) {
            const std::string output_file(output_dir + "/" + std::to_string(i) + ".json");
            FileUtil::WriteStringOrDie(output_file, response_json);
        }
        if (output_list != nullptr) {
            for (const auto &entity : response.results_) {
                output_list->emplace_back(entity);
            }
        }
    }
}


void SearchBatch(const SearchParams &params, const std::string &output_dir, const unsigned limit) {
    SearchBatchHelper(params, output_dir, /* output_list=*/nullptr, limit);
}


std::vector<Entity> SearchBatch(const SearchParams &params, const unsigned limit) {
    std::vector<Entity> output_list;
    SearchBatchHelper(params, /* output_dir=*/"", &output_list, limit);
    return output_list;
}


std::vector<Work> SearchBatch(const SearchParamsWorks &params, const unsigned limit) {
    const SearchParams entity_params((SearchParams)params);
    auto entities(SearchBatch(entity_params, limit));

    std::vector<Work> works;
    for (auto &entity : entities)
        works.emplace_back(entity.getJson());
    return works;
}


std::vector<DataProvider> SearchBatch(const SearchParamsDataProviders &params, const unsigned limit) {
    const SearchParams entity_params((SearchParams)params);
    auto entities(SearchBatch(entity_params, limit));

    std::vector<DataProvider> data_providers;
    for (auto &entity : entities)
        data_providers.emplace_back(entity.getJson());
    return data_providers;
}


nlohmann::json ParseFile(const std::string &file) {
    std::ifstream input(file);
    nlohmann::json json;
    input >> json;
    return json;
}


std::vector<Entity> GetEntitiesFromFile(const std::string &file) {
    const auto json(ParseFile(file));
    nlohmann::json json_array;
    if (json.is_array())
        json_array = json;
    else if (json.is_object()) {
        json_array = json["results"];
    } else {
        throw std::runtime_error("could not get CORE results from JSON file: " + file);
    }

    std::vector<Entity> entities;
    for (const auto &json_entity : json_array) {
        entities.emplace_back(json_entity);
    }

    return entities;
}


std::vector<Work> GetWorksFromFile(const std::string &file) {
    const auto entities(GetEntitiesFromFile(file));
    std::vector<Work> works;
    for (const auto &entity : entities) {
        const Work work(entity.getJson());
        works.emplace_back(work);
    }
    return works;
}


void OutputFileStart(const std::string &path) {
    FileUtil::MakeParentDirectoryOrDie(path, /*recursive=*/true);
    FileUtil::WriteStringOrDie(path, "[\n");
}


void OutputFileAppend(const std::string &path, const Entity &entity, const bool first) {
    if (not first)
        FileUtil::AppendStringOrDie(path, ",\n");
    FileUtil::AppendStringOrDie(path, entity.getJson().dump());
}


void OutputFileEnd(const std::string &path) {
    FileUtil::AppendStringOrDie(path, "\n]");
}


} // namespace CORE
