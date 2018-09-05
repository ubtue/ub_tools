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

#include "ZoteroTransformation.h"

namespace Zotero {

namespace ZoteroTransformation {

// Zotero values see https://raw.githubusercontent.com/zotero/zotero/master/test/tests/data/allTypesAndFields.js
// MARC21 values see https://www.loc.gov/marc/relators/relaterm.html
const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP{
    { "artist",             "art" },
    { "attorneyAgent",      "csl" },
    { "author",             "aut" },
    { "bookAuthor",         "edc" },
    { "cartographer",       "ctg" },
    { "castMember",         "act" },
    { "commenter",          "cwt" },
    { "composer",           "cmp" },
    { "contributor",        "ctb" },
    { "cosponsor",          "spn" },
    { "director",           "drt" },
    { "editor",             "edt" },
    { "guest",              "pan" },
    { "interviewee",        "ive" },
    { "inventor",           "inv" },
    { "performer",          "prf" },
    { "podcaster",          "brd" },
    { "presenter",          "pre" },
    { "producer",           "pro" },
    { "programmer",         "prg" },
    { "recipient",          "rcp" },
    { "reviewedAuthor",     "aut" },
    { "scriptwriter",       "aus" },
    { "seriesEditor",       "edt" },
    { "sponsor",            "spn" },
    { "translator",         "trl" },
    { "wordsBy",            "wam" },
};


const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type) {
    const auto creator_type_zotero_and_marc21(CREATOR_TYPES_TO_MARC21_MAP.find(zotero_creator_type));
    if (creator_type_zotero_and_marc21 == CREATOR_TYPES_TO_MARC21_MAP.end())
        LOG_ERROR("Zotero creatorType could not be mapped to MARC21: \"" + zotero_creator_type + "\"");
    return creator_type_zotero_and_marc21->second;
}


const std::map<std::string, MARC::Record::BibliographicLevel> ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP{
    { "book", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "bookSection", MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART },
    { "document", MARC::Record::BibliographicLevel::MONOGRAPH_OR_ITEM },
    { "journalArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "magazineArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "newspaperArticle", MARC::Record::BibliographicLevel::SERIAL_COMPONENT_PART },
    { "webpage", MARC::Record::BibliographicLevel::INTEGRATING_RESOURCE }
};


MARC::Record::BibliographicLevel MapBiblioLevel(const std::string item_type) {
    const auto bib_level_map_entry(ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.find(item_type));
    if (bib_level_map_entry == ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP.cend())
        LOG_ERROR("No bibliographic level mapping entry available for Zotero item type: " + item_type);
    return bib_level_map_entry->second;
}


bool IsValidItemType(const std::string item_type) {
    static std::vector<std::string> valid_item_types({ "journalArticle", "magazineArticle",  "newspaperArticle", "webpage" });
    return std::find(valid_item_types.begin(), valid_item_types.end(), item_type) != valid_item_types.end();
}


// "author" must be in the lastname,firstname format. Returns the empty string if no PPN was found.
std::string DownloadAuthorPPN(const std::string &author, const struct ::Zotero::SiteParams &site_params) {
    const std::string LOOKUP_URL(site_params.group_params_->author_lookup_url_ + UrlUtil::UrlEncode(author));

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


} // end ZoteroTransformation

} // end Zotero
