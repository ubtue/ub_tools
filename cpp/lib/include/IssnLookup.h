/** \file   IssnLookup.h
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

#pragma once

#include <iostream>
#include <nlohmann/json.hpp>


namespace IssnLookup {


/**
 * \brief Data structure for storing ISSN info taken from https://portal.issn.org/
 */
struct ISSNInfo {
    std::string main_title_, // mainTitle
        title_,              // taken the value from @id with hash #KeyTitle
        format_,             // format
        identifier_,         // identifier
        type_,               // type
        issn_,               // issn
        is_part_of_,         // isPartOf
        publication_,        // publication
        url_;                // url
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


/**
 * \brief Get the ISSN info from https://portal.issn.org/
 * \param issn  single ISSN
 * \param issn_info is used to save the raw issn info
 */
bool GetISSNInfo(const std::string issn, nlohmann::json &issn_info);


/**
 * \brief Extracting data from the raw issn info and store it to the ISSNInfo structure
 * \param issn_info is used to store the extracted issn info in the ISSNInfo' structure
 * \param issn_info_json the raw issn info
 * \param issn single ISSN
 */
void ExtractingData(ISSNInfo * const issn_info, const nlohmann::json &issn_info_json, const std::string issn);


/**
 * \brief Show the content of ISSNInfo structure as debuging purpose
 * \param issn_info the variable of ISSNInfo
 */
void PrettyPrintISSNInfo(const ISSNInfo &issn_info);

} // namespace IssnLookup