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
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"


namespace Zotero {


namespace Transformation {


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


// If "key" is in "map", then return the mapped value, o/w return "key".
std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map) {
    const auto &key_and_value(map.find(key));
    return (key_and_value == map.cend()) ? key : key_and_value->second;
}


std::string NormalizeDate(const std::string date_raw, const std::string strptime_format) {

    struct tm tm(TimeUtil::StringToStructTm(date_raw, strptime_format));
    const std::string date_normalized(std::to_string(tm.tm_year + 1900) + "-"
                                      + StringUtil::ToString(tm.tm_mon + 1, 10, 2, '0') + "-"
                                      + StringUtil::ToString(tm.tm_mday, 10, 2, '0'));
    return date_normalized;
}

bool TestForUnknownZoteroKey(const std::shared_ptr<const JSON::ObjectNode> &object_node) {
    const std::string known_keys("^" + StringUtil::Join(known_zotero_keys, "|") + "$");
    static RegexMatcher * const known_keys_matcher(RegexMatcher::RegexMatcherFactory(known_keys));
    for (const auto &property : *object_node) {
        if (not known_keys_matcher->matched(property.first)) {
            LOG_ERROR("Unknown Zotero key \"" + property.first + "\"");
            return true;
        }
    }
    return false;
}


} // end ZoteroTransformation


} // end Zotero
