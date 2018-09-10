/*  \brief Utility Functions for Normalizing and Augmenting Data obtained by Zotero
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

#include "MARC.h"
#include "UrlUtil.h"
#include "Zotero.h"


namespace Zotero {

namespace ZoteroTransformation {

    extern const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP;
    extern const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP;
    extern const std::map<std::string, MARC::Record::BibliographicLevel> ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP;

/*    class PPNandTitle {
        std::string PPN_;
        std::string title_;
    public:
        PPNandTitle(const std::string &PPN, const std::string &title): PPN_(PPN), title_(title) { }
        PPNandTitle() = default;
        PPNandTitle(const PPNandTitle &other) = default;
    
        inline const std::string &getPPN() const { return PPN_; }
        inline const std::string &getTitle() const { return title_; }
    };*/


/*    struct AugmentMaps {
        std::unordered_map<std::string, std::string> ISSN_to_SSG_map_;
        std::unordered_map<std::string, std::string> ISSN_to_keyword_field_map_;
        std::unordered_map<std::string, std::string> ISSN_to_language_code_map_;
        std::unordered_map<std::string, std::string> ISSN_to_licence_map_;
        std::unordered_map<std::string, std::string> ISSN_to_physical_form_map_;
        std::unordered_map<std::string, std::string> ISSN_to_volume_map_;
        std::unordered_map<std::string, std::string> language_to_language_code_map_;
        std::unordered_map<std::string, PPNandTitle> ISSN_to_superior_ppn_and_title_map_;
    public:
        explicit AugmentMaps(const std::string &map_directory_path);
    };*/

   const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type);
   MARC::Record::BibliographicLevel MapBiblioLevel(const std::string item_type);
   bool IsValidItemType(const std::string item_type);
//   std::string DownloadAuthorPPN(const std::string &author, const struct Zotero::SiteParams &site_params);
   std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map);

} // end ZoteroTransformation

} // end Zotero
