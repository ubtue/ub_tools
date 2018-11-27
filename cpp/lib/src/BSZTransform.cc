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
#include "Downloader.h"
#include "MapIO.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


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
    MapIO::DeserialiseMap(map_directory_path + "language_to_language_code.map", &language_to_language_code_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_language_code.map", &ISSN_to_language_code_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_licence.map", &ISSN_to_licence_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_keyword_field.map", &ISSN_to_keyword_field_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_physical_form.map", &ISSN_to_physical_form_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_volume.map", &ISSN_to_volume_map_);
    MapIO::DeserialiseMap(map_directory_path + "ISSN_to_SSG.map", &ISSN_to_SSG_map_);
    LoadISSNToPPNMap(&ISSN_to_superior_ppn_and_title_map_);
}


// "author" must be in the lastname,firstname format. Returns the empty string if no PPN was found.
std::string DownloadAuthorPPN(const std::string &author, const std::string &author_lookup_base_url) {
    const std::string LOOKUP_URL(author_lookup_base_url + UrlUtil::UrlEncode(author));
    static std::unordered_map<std::string, std::string> url_to_lookup_result_cache;
    const auto url_and_lookup_result(url_to_lookup_result_cache.find(LOOKUP_URL));
    if (url_and_lookup_result == url_to_lookup_result_cache.end()) {
        static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("<SMALL>PPN</SMALL>.*<div><SMALL>([0-9X]+)"));
        Downloader downloader(LOOKUP_URL);
        if (downloader.anErrorOccurred())
            LOG_ERROR(downloader.getLastErrorMessage());
        else if (matcher->matched(downloader.getMessageBody())) {
            url_to_lookup_result_cache.emplace(LOOKUP_URL, (*matcher)[1]);
            return (*matcher)[1];
        } else
            url_to_lookup_result_cache.emplace(LOOKUP_URL, "");
    } else
        return url_and_lookup_result->second;
    return "";
}


BSZTransform::BSZTransform(const std::string &map_directory_path): augment_maps_(AugmentMaps(map_directory_path)) {}
BSZTransform::BSZTransform(const AugmentMaps &augment_maps): augment_maps_(augment_maps) {}


void BSZTransform::DetermineKeywordOutputFieldFromISSN(const std::string &issn, std::string * const tag, char * const subfield) {
    if (not issn.empty()) {
        const auto issn_and_field_tag_and_subfield_code(augment_maps_.ISSN_to_keyword_field_map_.find(issn));
        if (issn_and_field_tag_and_subfield_code != augment_maps_.ISSN_to_keyword_field_map_.end()) {
            if (unlikely(issn_and_field_tag_and_subfield_code->second.length() != 3 + 1))
                LOG_ERROR("\"" + issn_and_field_tag_and_subfield_code->second
                          + "\" is not a valid MARC tag + subfield code! (Error in \"ISSN_to_keyword_field.map\"!)");
            *tag = issn_and_field_tag_and_subfield_code->second.substr(0, 3);
            *subfield = issn_and_field_tag_and_subfield_code->second[3];
            return;
        }
    }
    *tag = "650";
    *subfield = 'a';
}


} // end namespace BSZTransform
