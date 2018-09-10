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
#include "BSZTransform.h"
#include "MiscUtil.h"


namespace BSZTransform {

void LoadISSNToPPNMap(std::unordered_map<std::string, PPNandTitle> * const ISSN_to_superior_ppn_map) {
     enum ISSN_TO_PPN_OFFSET {ISSN_OFFSET = 0, PPN_OFFSET = 1, TITLE_OFFSET = 4};
     std::vector<std::vector<std::string>> parsed_issn_to_superior_content;
     TextUtil::ParseCSVFileOrDie(ISSN_TO_MISC_BITS_MAP_PATH_LOCAL, &parsed_issn_to_superior_content, ',', (char) 0x00);
     for (const auto parsed_line : parsed_issn_to_superior_content) {
         const std::string ISSN(parsed_line[ISSN_OFFSET]);
         const std::string PPN(parsed_line[PPN_OFFSET]);
         const std::string title(StringUtil::RightTrim(" \t", parsed_line[TITLE_OFFSET]));
         ISSN_to_superior_ppn_map->emplace(ISSN, PPNandTitle(PPN, title));
     }
}


AugmentMaps::AugmentMaps(const std::string &map_directory_path) {
    MiscUtil::LoadMapFile(map_directory_path + "language_to_language_code.map", &language_to_language_code_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_language_code.map", &ISSN_to_language_code_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_licence.map", &ISSN_to_licence_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_keyword_field.map", &ISSN_to_keyword_field_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_physical_form.map", &ISSN_to_physical_form_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_volume.map", &ISSN_to_volume_map_);
    MiscUtil::LoadMapFile(map_directory_path + "ISSN_to_SSG.map", &ISSN_to_SSG_map_);
    LoadISSNToPPNMap(&ISSN_to_superior_ppn_and_title_map_);
}

} // end namespace BSZTransform

