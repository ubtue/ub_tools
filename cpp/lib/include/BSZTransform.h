/*  \brief Utility Functions for Transforming Data in accordance with BSZ Requirements
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
#pragma once


#include <string>
#include <unordered_map>
#include "UBTools.h"


namespace BSZTransform {


const std::string ISSN_TO_MISC_BITS_MAP_PATH_LOCAL(UBTools::GetTuelibPath() + "issn_to_misc_bits.map");
const std::string ISSN_TO_MISC_BITS_MAP_DIR_REMOTE("/mnt/ZE020150/FID-Entwicklung/issn_to_misc_bits");


class PPNandTitle {
    std::string PPN_;
    std::string title_;
public:
    PPNandTitle(const std::string &PPN, const std::string &title): PPN_(PPN), title_(title) { }
    PPNandTitle() = default;
    PPNandTitle(const PPNandTitle &other) = default;

    inline const std::string &getPPN() const { return PPN_; }
    inline const std::string &getTitle() const { return title_; }
};


struct AugmentMaps {
    std::unordered_map<std::string, std::string> ISSN_to_SSG_map_;
    std::unordered_map<std::string, std::string> ISSN_to_keyword_field_map_;
    std::unordered_map<std::string, std::string> ISSN_to_language_code_map_;
    std::unordered_map<std::string, std::string> ISSN_to_licence_map_;
    std::unordered_map<std::string, std::string> ISSN_to_volume_map_;
    std::unordered_map<std::string, std::string> language_to_language_code_map_;
    std::unordered_map<std::string, PPNandTitle> ISSN_to_superior_ppn_and_title_map_;
public:
    explicit AugmentMaps(const std::string &map_directory_path);
};


std::string DownloadAuthorPPN(const std::string &author, const std::string &author_download_base_url);


class BSZTransform {
public:
      AugmentMaps augment_maps_;
      BSZTransform(const std::string &map_directory_path);
      BSZTransform(const AugmentMaps &augment_maps);
      void DetermineKeywordOutputFieldFromISSN(const std::string &issn, std::string * const tag, char * const subfield);
};


const std::string CATHOLIC_ORDERS_ABBR_PATH(UBTools::GetTuelibPath() + "abbr_catholic_orders.txt");


/* \brief   Strips abbreviations of catholic orders from the author's (last)name.
 * \return  true if names were changed.
 * \note    Author names in the metadata returned by Zotero sometimes contain abbreviations of catholic orders,
 *          amongst other extraneous information. In such cases, the abbreviation is incorrectly interpreted as
 *          the last name (and the full name as the first name).
*/
bool StripCatholicOrdersFromAuthorName(std::string * const first_name, std::string * const last_name);


} // end namespace BSZTransform
