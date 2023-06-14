/**
 * \brief The Utility for extracting issn information from https://portal.issn.org/
 * \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 * \copyright 2023 Tübingen University Library.  All rights reserved.
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

#include <iostream>
#include <nlohmann/json.hpp>
#include "Downloader.h"


[[noreturn]] void Usage() {
    ::Usage(
        "\n\n"
        "issn\n");
}

const unsigned int TIMEOUT_IN_SECONDS(15);

struct ISSNInfo {
    std::string main_title_, title_, format_, identifier_, type_, issn_, is_part_of_, publication_, url_;
    std::vector<std::string> names_;

    ISSNInfo() {
        main_title_ = "";
        title_ = "";
        format_ = "";
        identifier_ = "";
        type_ = "";
        issn_ = "";
        is_part_of_ = "";
        publication_ = "";
        url_ = "";
    }
};

// issn information from https://portal.issn.org/
bool GetISSNInfo(const std::string issn, nlohmann::json &issn_info) {
    const std::string base_url("https://portal.issn.org/resource/ISSN/"), issn_file_format("json"),
        issn_url(base_url + issn + "?format=" + issn_file_format);

    Downloader downloader(issn_url, Downloader::Params(), TIMEOUT_IN_SECONDS * 1000);

    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for issn " + issn + ": " + downloader.getLastErrorMessage());
        return false;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("ISSN Lookup returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for ISSN: " + issn);
        return false;
    }

    std::string issn_info_json(downloader.getMessageBody());
    try {
        issn_info = nlohmann::json::parse(issn_info_json);
    } catch (nlohmann::json::parse_error &ex) {
        std::string err(ex.what());
        LOG_ERROR("failed to parse JSON (" + err + "), download URL was: " + issn_url);
        return false;
    }

    return true;
}

void ExtractingData(ISSNInfo * const issn_info, const nlohmann::json &issn_info_json, const std::string issn) {
    const std::string issn_uri("resource/ISSN/" + issn), issn_title_uri("resource/ISSN/" + issn + "#KeyTitle");
    if (issn_info_json.at("@graph").is_structured()) {
        for (auto ar : issn_info_json.at("@graph")) {
            if (ar.is_structured()) {
                if (ar.at("@id") == issn_uri) {
                    for (auto &[key, val] : ar.items()) {
                        if (key == "mainTitle")
                            issn_info->main_title_ = val;
                        if (key == "format")
                            issn_info->format_ = val;
                        if (key == "identifier")
                            issn_info->identifier_ = val;
                        if (key == "type")
                            issn_info->type_ = val;
                        if (key == "issn")
                            issn_info->issn_ = val;
                        if (key == "isPartOf")
                            issn_info->is_part_of_ = val;
                        if (key == "publication")
                            issn_info->publication_ = val;
                        if (key == "url")
                            issn_info->url_ = val;
                        if (key == "name") {
                            if (val.is_structured()) {
                                for (auto val_item : val)
                                    issn_info->names_.emplace_back(val_item);
                            } else {
                                issn_info->names_.emplace_back(val);
                            }
                        }
                    }
                }
                if (ar.at("@id") == issn_title_uri) {
                    for (auto &[key, val] : ar.items()) {
                        if (key == "value")
                            issn_info->title_ = val;
                    }
                }
            }
        }
    }
}

void PrettyPrintISSNInfo(const ISSNInfo &issn_info) {
    std::cout << "mainTitle: " << issn_info.main_title_ << std::endl;
    std::cout << "title: " << issn_info.title_ << std::endl;
    std::cout << "format: " << issn_info.format_ << std::endl;
    std::cout << "identifier: " << issn_info.identifier_ << std::endl;
    std::cout << "type: " << issn_info.type_ << std::endl;
    std::cout << "issn: " << issn_info.issn_ << std::endl;
    std::cout << "isPartOf: " << issn_info.is_part_of_ << std::endl;
    std::cout << "publication: " << issn_info.publication_ << std::endl;
    std::cout << "url: " << issn_info.url_ << std::endl;
    std::cout << "name: " << std::endl;
    for (auto &name : issn_info.names_)
        std::cout << name << std::endl;
}
int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    nlohmann::json issn_info_json;
    bool processing_state(GetISSNInfo(argv[1], issn_info_json));

    if (processing_state) {
        ISSNInfo issn_info;
        ExtractingData(&issn_info, issn_info_json, argv[1]);
        PrettyPrintISSNInfo(issn_info);
    }

    return (processing_state ? EXIT_SUCCESS : EXIT_FAILURE);
}