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
#include "FileUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"


const std::string CORE::API_BASE_URL = "https://api.core.ac.uk/v3/";


const std::string CORE::GetAPIKey() {
    static std::string key(StringUtil::TrimWhite(FileUtil::ReadStringOrDie(UBTools::GetTuelibPath() + "CORE-API.key")));
    return key;
}

const std::string CORE::GetEndpointForEntityType(const EntityType type) {
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


CORE::Author::Author(const std::shared_ptr<const JSON::ObjectNode> json_obj) {
    name_ = json_obj->getStringValue("name");
}


CORE::Journal::Journal(const std::shared_ptr<const JSON::ObjectNode> json_obj) {
    title_ = json_obj->getOptionalStringValue("title");
    const auto identifiers(json_obj->getArrayNode("identifiers"));
    for (const auto &identifier_node : *identifiers) {
        const std::shared_ptr<const JSON::StringNode> identifier_string(JSON::JSONNode::CastToStringNodeOrDie("journal identifier", identifier_node));
        identifiers_.emplace_back(identifier_string->getValue());
    }
}


CORE::Language::Language(const std::shared_ptr<const JSON::ObjectNode> json_obj) {
    code_ = json_obj->getStringValue("code");
    name_ = json_obj->getStringValue("name");
}


CORE::Work::Work(const std::shared_ptr<const JSON::ObjectNode> json_obj) {
    abstract_ = json_obj->getOptionalStringValue("abstract");

    const auto authors = json_obj->getArrayNode("authors");
    if (authors != nullptr) {
        for (const auto &author_node : *authors) {
            const std::shared_ptr<const JSON::ObjectNode> author_obj(JSON::JSONNode::CastToObjectNodeOrDie("author JSON entity", author_node));
            authors_.emplace_back(Author(author_obj));
        }
    }

    document_type_ = json_obj->getOptionalStringValue("documentType");
    download_url_ = json_obj->getStringValue("downloadUrl");
    field_of_study_ = json_obj->getOptionalStringValue("fieldOfStudy");
    id_ = json_obj->getIntegerValue("id");

    const auto journals = json_obj->getArrayNode("journals");
    if (journals != nullptr) {
        for (const auto &journal_node : *journals) {
            const std::shared_ptr<const JSON::ObjectNode> journal_obj(JSON::JSONNode::CastToObjectNodeOrDie("journal JSON entity", journal_node));
            journals_.emplace_back(Journal(journal_obj));
        }

    }

    if (json_obj->hasNode("language") && not json_obj->isNullNode("language"))
        language_ = Language(json_obj->getObjectNode("language"));
    publisher_ = json_obj->getStringValue("publisher");
    title_ = json_obj->getStringValue("title");
    year_published_ = json_obj->getIntegerValue("yearPublished");
}


Downloader CORE::download(const std::string &url) {
    Downloader::Params downloader_params;
    downloader_params.additional_headers_.push_back("Authorization: Bearer " + GetAPIKey());
    //downloader_params.debugging_ = true;

    {
        Downloader downloader(downloader_params);
        downloader.newUrl(url);

        if (downloader.getResponseCode() == 429) {
            LOG_INFO(downloader.getMessageHeader());
            const auto header(downloader.getMessageHeaderObject());
            if (header.getXRatelimitRemaining() == 0) {
                // Conversion problems due to special 8601 format that is not supported yet by TimeUtil.
                // If we solve that, we might be able to ret the exact time to sleep from the response header.
                const time_t sleep_until_time(header.getXRatelimitRetryAfter("%Y-%m-%dT%H:%M:%S%z"));
                if (sleep_until_time != TimeUtil::BAD_TIME_T) {
                    const std::string sleep_until_string(TimeUtil::TimeTToLocalTimeString(sleep_until_time));

                    LOG_WARNING("Rate limiting active + too many requests! Sleeping until " + sleep_until_string);
                    std::this_thread::sleep_until (std::chrono::system_clock::from_time_t(sleep_until_time));
                } else {
                    const unsigned sleep_seconds(60);
                    LOG_WARNING("Rate limiting active + too many requests! Could not determine retry_after timestamp! Sleeping for " + std::to_string(sleep_seconds) + "s");
                    ::sleep(sleep_seconds);
                }
            }
        } else {

            if (downloader.anErrorOccurred())
                throw std::runtime_error(downloader.getLastErrorMessage());

            return downloader;
        }
    }

    // Retry if we ran into any errors.
    // The existing downloader object should be destroyed before doing this.
    return download(url);
}

const std::string CORE::SearchParams::buildUrl() const {
    std::string url = CORE::API_BASE_URL + "search/" + UrlUtil::UrlEncode(GetEndpointForEntityType(entity_type_));
    url += "?q=" + UrlUtil::UrlEncode(q_);

    if (scroll_)
        url += "&scroll";
    if (offset_ > 0)
        url += "&offset=" + std::to_string(offset_);
    if (limit_ > 0)
        url += "&limit=" + std::to_string(limit_);
    if (not scroll_id_.empty())
        url += "&scroll_id=" + UrlUtil::UrlEncode(scroll_id_);
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
unsigned GetJsonUnsignedValue(const std::shared_ptr<const JSON::ObjectNode> node, const std::string &label) {
    const auto child(node->getNode(label));
    if (child->getType() == JSON::JSONNode::Type::STRING_NODE) {
        const auto string_node = child->CastToStringNodeOrDie(label, child);
        return StringUtil::ToInt(string_node->getValue());
    } else {
        const auto int_node = child->CastToIntegerNodeOrDie(label, child);
        return int_node->getValue();
    }
}


CORE::SearchResponse::SearchResponse(const std::string &json) {
    JSON::Parser parser(json);
    std::shared_ptr<JSON::JSONNode> root_node;
    if (not parser.parse(&root_node))
        throw std::runtime_error("in CORE::SearchResponse: could not parse JSON response: " + json);

    const std::shared_ptr<const JSON::ObjectNode> root(JSON::JSONNode::CastToObjectNodeOrDie("top level JSON entity", root_node));

    total_hits_ = root->getIntegerValue("totalHits");
    limit_ = GetJsonUnsignedValue(root, "limit"); // For some reason JSON treats this as string instead of int
    offset_ = GetJsonUnsignedValue(root, "offset");

    if (root->hasNode("scrollId") and not root->isNullNode("scrollId"))
        scroll_id_ = root->getStringValue("scrollId");

    const std::shared_ptr<const JSON::ArrayNode> results(root->getArrayNode("results"));
    for (const auto &result_node : *results) {
        std::shared_ptr<JSON::ObjectNode> result(JSON::JSONNode::CastToObjectNodeOrDie("result JSON entity", result_node));
        results_.emplace_back(result);
    }

    // TODO:
    // tooks
    // es_took
}


CORE::SearchResponseWorks::SearchResponseWorks(const SearchResponse &response) {
    total_hits_ = response.total_hits_;
    limit_ = response.limit_;
    offset_ = response.offset_;
    scroll_id_ = response.scroll_id_;
    tooks_ = response.tooks_;
    es_took_ = response.es_took_;

    for (const auto &result : response.results_) {
        results_.emplace_back(Work(result));
    }
}


Downloader CORE::searchRaw(const SearchParams &params) {
    const std::string url(params.buildUrl());
    auto downloader(download(url));
    return downloader;
}


CORE::SearchResponse CORE::search(const SearchParams &params) {
    auto downloader(searchRaw(params));
    const std::string json(downloader.getMessageBody());
    return SearchResponse(json);
}


CORE::SearchResponseWorks CORE::searchWorks(const SearchParamsWorks &params) {
    const auto response_raw(search(params));
    SearchResponseWorks response(response_raw);
    return response;
}


void CORE::searchBatch(const SearchParams &params, const std::string &output_dir) {
    SearchParams current_params = params;

    // Always enable scrolling. This is mandatory if we have > 10.000 results.
    current_params.scroll_ = true;

    auto downloader(searchRaw(current_params));
    SearchResponse response(downloader.getMessageBody());

    int i(1);
    std::string output_file(output_dir + "/" + std::to_string(i) + ".json");
    LOG_INFO("Downloaded file: " + output_file + " (" + std::to_string(response.offset_) + "-" + std::to_string(response.offset_ + response.limit_) + "/" + std::to_string(response.total_hits_) + ")");

    FileUtil::WriteStringOrDie(output_file, downloader.getMessageBody());

    while (response.offset_ + response.limit_ < response.total_hits_) {
        current_params.offset_ += current_params.limit_;

        ++i;
        output_file = output_dir + "/" + std::to_string(i) + ".json";

        downloader = searchRaw(current_params);
        response = SearchResponse(downloader.getMessageBody());
        if (not response.scroll_id_.empty())
            current_params.scroll_id_ = response.scroll_id_;

        LOG_INFO("Downloaded file: " + output_file + " (" + std::to_string(response.offset_) + "-" + std::to_string(response.offset_ + response.limit_) + "/" + std::to_string(response.total_hits_) + ")");
        FileUtil::WriteStringOrDie(output_file, downloader.getMessageBody());
    }


}
