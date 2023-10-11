/** \file   IssnLookup.cc
 *  \brief   The Utility for extracting issn information from https://portal.issn.org/
 *  \author  Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2023 Tübingen University Library.  All rights reserved.
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
#include "IssnLookup.h"


namespace IssnLookup {

std::string TitleNormalization(const std::string &title) {
    std::string new_title;
    /*
     * Remove non-readable characters used by the librarian to annotate the unused string/word in the title to mark non-filing characters.
     * The non-readable characters are start string code (FFFFFFC2 and FFFFFF98) and string delimiter code (FFFFFFC2 and FFFFFF9C).
     */
    for (const char &c : title) {
        unsigned str_hex_code = unsigned(c);

        if (!((str_hex_code == 0xFFFFFFC2) || (str_hex_code == 0xFFFFFF98) || (str_hex_code == 0xFFFFFF9C)))
            new_title += c;
    }

    return new_title;
}

void ExtractingData(const std::string &issn, const nlohmann::json &issn_info_json, ISSNInfo * const issn_info) {
    const std::string issn_uri("resource/ISSN/" + issn), issn_title_uri("resource/ISSN/" + issn + "#KeyTitle");
    if (issn_info_json.at("@graph").is_structured()) {
        for (const auto &ar : issn_info_json.at("@graph")) {
            if (ar.is_structured()) {
                if (ar.at("@id") == issn_uri) {
                    issn_info->issn_ = issn;
                    for (const auto &[key, val] : ar.items()) {
                        if (key == "mainTitle") {
                            if (val.is_structured())
                                for (const auto &val_item : val)
                                    issn_info->main_titles_.emplace_back(TitleNormalization(val_item));
                            else
                                issn_info->main_titles_.emplace_back(TitleNormalization(val));
                        }

                        if (key == "format")
                            issn_info->format_ = val;
                        if (key == "identifier")
                            issn_info->identifier_ = val;
                        if (key == "type")
                            issn_info->type_ = val;

                        if (key == "isPartOf") {
                            if (val.is_structured())
                                for (const auto &val_item : val)
                                    issn_info->is_part_of_.emplace_back(val_item);
                            else
                                issn_info->is_part_of_.emplace_back(val);
                        }

                        if (key == "publication")
                            issn_info->publication_ = val;

                        if (key == "url") {
                            if (val.is_structured())
                                for (const auto &val_item : val)
                                    issn_info->urls_.emplace_back(val_item);
                            else
                                issn_info->urls_.emplace_back(val);
                        }

                        if (key == "name") {
                            if (val.is_structured())
                                for (const auto &val_item : val)
                                    issn_info->names_.emplace_back(TitleNormalization(val_item));
                            else
                                issn_info->names_.emplace_back(TitleNormalization(val));
                        }
                    }
                }
                if (ar.at("@id") == issn_title_uri) {
                    for (auto &[key, val] : ar.items()) {
                        if (key == "value")
                            if (val.is_structured())
                                for (const auto &val_item : val)
                                    issn_info->titles_.emplace_back(val_item);
                            else
                                issn_info->titles_.emplace_back(val);
                    }
                }
            }
        }
    }
}

bool GetISSNInfo(const std::string &issn, ISSNInfo * const issn_info) {
    const std::string issn_url("https://portal.issn.org/resource/ISSN/" + issn + "?format=json");

    Downloader downloader(issn_url, Downloader::Params());

    if (downloader.anErrorOccurred()) {
        LOG_WARNING("Error while downloading data for ISSN " + issn + ": " + downloader.getLastErrorMessage());
        return false;
    }

    // Check for rate limiting and error status codes:
    const HttpHeader http_header(downloader.getMessageHeader());
    if (http_header.getStatusCode() != 200) {
        LOG_WARNING("IssnLookup returned HTTP status code " + std::to_string(http_header.getStatusCode()) + "! for ISSN: " + issn);
        return false;
    }

    const std::string content_type(http_header.getContentType());
    if (content_type.find("application/json") == std::string::npos) {
        // Unfortunately if the ISSN doesnt exist, the page will
        // return status code 200 OK, but HTML instead of JSON, so we need to
        // detect this case manually.
        LOG_WARNING("IssnLookup returned no JSON (maybe invalid ISSN) for ISSN: " + issn);
        return false;
    }

    std::string issn_info_json(downloader.getMessageBody());
    nlohmann::json issn_info_json_tree;

    try {
        issn_info_json_tree = nlohmann::json::parse(issn_info_json);
    } catch (nlohmann::json::parse_error &ex) {
        std::string err(ex.what());
        LOG_ERROR("Failed to parse JSON! " + err);
        return false;
    }

    ExtractingData(issn, issn_info_json_tree, issn_info);

    return true;
}

void ISSNInfo::PrettyPrint() {
    std::cout << "mainTitle: " << std::endl;
    for (const auto &main_title : main_titles_)
        std::cout << main_title << std::endl;

    std::cout << "title(s): " << std::endl;
    for (const auto &title : titles_)
        std::cout << title << std::endl;

    std::cout << "format: " << format_ << std::endl;
    std::cout << "identifier: " << identifier_ << std::endl;
    std::cout << "type: " << type_ << std::endl;
    std::cout << "ISSN: " << issn_ << std::endl;

    std::cout << "isPartOf: " << std::endl;
    for (const auto &is_o : is_part_of_)
        std::cout << is_o << std::endl;

    std::cout << "publication: " << publication_ << std::endl;

    std::cout << "url: " << std::endl;
    for (const auto &url : urls_)
        std::cout << url << std::endl;

    std::cout << "name: " << std::endl;
    for (const auto &name : names_)
        std::cout << name << std::endl;
}

} // namespace IssnLookup
