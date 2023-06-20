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
#include <vector>


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
        publication_;        // publication

    std::vector<std::string> names_, urls_;

    ISSNInfo() = default;

    /**
     * \brief Show the content of ISSNInfo structure as debuging purpose
     */
    void PrettyPrint();
};


/**
 * \brief Get the ISSN info from https://portal.issn.org/
 * \param issn  single ISSN
 * \param issn_info is used to store ISSNInfo
 */
bool GetISSNInfo(const std::string &issn, ISSNInfo * const issn_info);

} // namespace IssnLookup