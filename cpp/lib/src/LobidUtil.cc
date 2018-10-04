/*  \brief Utility Functions for Lobid API (HBZ) to get title data, norm data (GND) and organisational data
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
#include "LobidUtil.h"
#include <unordered_map>
#include "Downloader.h"
#include "JSON.h"
#include "UrlUtil.h"
#include "util.h"


namespace LobidUtil {


std::unordered_map<std::string, const std::shared_ptr<const JSON::ObjectNode>> url_to_lookup_result_cache;


/** \brief abstract Query base class for different APIs */
class Query {
    const std::string BASE_URL = "http://lobid.org/";
    virtual std::string toUrl() = 0;
public:
    bool multipleResultsAllowed_ = false;

    const std::shared_ptr<const JSON::ObjectNode> execute() {
        const std::string url(toUrl());
        LOG_DEBUG(url);
        const auto url_and_lookup_result(url_to_lookup_result_cache.find(url));
        if (url_and_lookup_result != url_to_lookup_result_cache.end())
            return url_and_lookup_result->second;

        Downloader downloader(url);
        if (downloader.anErrorOccurred())
            LOG_ERROR(downloader.getLastErrorMessage());

        std::shared_ptr<JSON::JSONNode> root_node(nullptr);
        JSON::Parser json_parser(downloader.getMessageBody());
        if (not (json_parser.parse(&root_node)))
           LOG_ERROR("failed to parse returned JSON: " + json_parser.getErrorMessage() + "(input was: "
                     + downloader.getMessageBody() + ")");

        const std::shared_ptr<const JSON::ObjectNode> root_object(JSON::JSONNode::CastToObjectNodeOrDie("root", root_node));
        url_to_lookup_result_cache.emplace(url, root_object);

        if (not multipleResultsAllowed_ and root_object->getIntegerValue("totalItems") > 1) {
            LOG_WARNING("found more than one result");
            return nullptr;
        }

        return root_object;
    }
};


/** \brief Query class for GND API */
class GNDQuery : public Query {
    const std::string BASE_URL = "http://lobid.org/gnd/search?format=json";

    std::string toUrl() {
        std::string url(BASE_URL);
        if (not preferredName_.empty())
            url += "&q=" + UrlUtil::UrlEncode(preferredName_);
        if (not type_.empty())
            url += "&filter=type:" + UrlUtil::UrlEncode(type_);

        return url;
    }
public:
    /** \brief the following class members are named 1:1 as available in the API. */
    std::string preferredName_;
    std::string type_;
};


/** \brief Query class for Organisation API */
class OrganisationQuery : public Query {
    const std::string BASE_URL = "http://lobid.org/organisations/search?format=json";

    std::string toUrl() {
        std::string url(BASE_URL);
        if (not name_.empty())
            url += "&q=" + UrlUtil::UrlEncode("name:\"" + name_ + "\"");

        return url;
    }
public:
    /** \brief the following class members are named 1:1 as available in the API. */
    std::string name_;
};


/** \brief Query class for Resource API */
class ResourceQuery : public Query {
    const std::string BASE_URL = "http://lobid.org/resources/search?format=json";
    std::string toUrl() {
        std::string url(BASE_URL);
        if (not title_.empty())
            url += "&q=" + UrlUtil::UrlEncode("title:\"" + title_ + "\"");

        return url;
    }
public:
    /** \brief the following class members are named 1:1 as available in the API. */
    std::string title_;
};


std::string GetAuthorPPN(const std::string &author) {
    GNDQuery query;
    query.preferredName_ = author;
    query.type_ = "DifferentiatedPerson";

    const auto root_object(query.execute());
    if (root_object == nullptr)
        return "";

    return JSON::LookupString("/member/0/gndIdentifier", root_object, "");
}


std::vector<std::string> GetAuthorProfessions(const std::string &author) {
    GNDQuery query;
    query.multipleResultsAllowed_ = true;
    query.preferredName_ = author;
    query.type_ = "DifferentiatedPerson";

    const auto root_object(query.execute());
    if (root_object == nullptr)
        return std::vector<std::string>{};

    return JSON::LookupStrings("/member/*/professionOrOccupation/*/label", root_object);
}


std::string GetOrganisationISIL(const std::string &organisation) {
    OrganisationQuery query;
    query.name_ = organisation;

    const auto root_object(query.execute());
    if (root_object == nullptr)
        return "";

    return JSON::LookupString("/member/0/isil", root_object, "");
}


std::string GetTitleDOI(const std::string &title) {
    ResourceQuery query;
    query.multipleResultsAllowed_ = true;
    query.title_ = title;

    const auto root_object(query.execute());
    if (root_object == nullptr)
        return "";

    return JSON::LookupString("/member/0/doi/0", root_object, "");
}


} // namespace LobidUtil
