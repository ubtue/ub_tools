/** \file    Elasticsearch.cc
 *  \brief   Implementation of the Elasticsearch class.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Mario Trojan
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
#include "Elasticsearch.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "REST.h"
#include "UBTools.h"
#include "UrlUtil.h"


const std::string DEFAULT_CONFIG_FILE_PATH(UBTools::GetTuelibPath() + "Elasticsearch.conf");


static void LoadIniParameters(const std::string &config_file_path, std::string * const host, std::string * const index,
                              std::string * const username, std::string * const password, bool * const ignore_ssl_certificates)
{
    if (not FileUtil::Exists(config_file_path))
        LOG_ERROR("Elasticsearch config file missing: " + config_file_path);

    const IniFile ini_file(config_file_path);

    *host                    = ini_file.getString("Elasticsearch", "host");
    *index                   = ini_file.getString("Elasticsearch", "index");
    *username                = ini_file.getString("Elasticsearch", "username", "");
    *password                = ini_file.getString("Elasticsearch", "password", "");
    *ignore_ssl_certificates = ini_file.getBool("Elasticsearch", "ignore_ssl_certificates", false);
}


Elasticsearch::Elasticsearch(const std::string &ini_file_path) {
    LoadIniParameters(ini_file_path.empty() ? DEFAULT_CONFIG_FILE_PATH : ini_file_path, &host_, &index_, &username_,
                      &password_, &ignore_ssl_certificates_);
}


// A general comment as to the strategy we use in this function:
//
//    We know that there is an _update API endpoint, but as we insert a bunch of chunks, the number of which can change,
//    we have to use a "wildcard" strategy to first delete possibly existing chunks as the number of new chunks may be greater or
//    fewer than what may already be stored.
void Elasticsearch::insertDocument(const std::string &document_id, const std::vector<std::string> &document_chunks) {
    Downloader::Params downloader_params;
    downloader_params.authentication_username_ = username_;
    downloader_params.authentication_password_ = password_;
    downloader_params.ignore_ssl_certificates_ = ignore_ssl_certificates_;
    downloader_params.additional_headers_.push_back("Content-Type: application/json");

    const Url delete_url(host_ + "/" + index_ + "/_delete_by_query");
    const JSON::ObjectNode match_node("{ \"query\": { \"match\": { \"document_id\": \"" + JSON::EscapeString(document_id) + "\" } } }");

    std::shared_ptr<JSON::JSONNode> result(REST::QueryJSON(delete_url, REST::POST, &match_node, downloader_params));
    std::shared_ptr<JSON::ObjectNode> result_object(JSON::JSONNode::CastToObjectNodeOrDie("Elasticsearch result", result));
    if (result_object->hasNode("error"))
        LOG_ERROR("Elasticsearch delete_by_query failed: " + result_object->getNode("error")->toString());

    const Url insert_url(host_ + "/" + index_ + "/_insert");
    for (const auto &document_chunk : document_chunks) {
        const JSON::ObjectNode payload(JSON::ObjectNode(std::unordered_map<std::string, std::string>{ { "doc", document_chunk } }));
        result = REST::QueryJSON(insert_url, REST::PUT, &payload, downloader_params);
    }
}
