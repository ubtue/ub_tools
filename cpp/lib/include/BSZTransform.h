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


const std::string AUTHOR_NAME_BLACKLIST(UBTools::GetTuelibPath() + "zotero_author_name_blacklist.txt");


// Performs various operations such as blacklisted-token removal, title detection on the author name.
void PostProcessAuthorName(std::string * const first_name, std::string * const last_name, std::string * const title,
                           std::string * const affix);


enum SSGNType { INVALID, FG_0, FG_1, FG_01, FG_21 };


SSGNType GetSSGNTypeFromString(const std::string &ssgn_string);


} // end namespace BSZTransform
