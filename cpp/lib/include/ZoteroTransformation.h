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


#include "JSON.h"
#include "MARC.h"


namespace Zotero {


namespace Transformation {


    static std::vector<std::string> known_zotero_keys( {
        "abstractNote",
        "accessDate",
        "archiveLocation",
        "creators",
        "date",
        "DOI",
        "extra",
        "ISSN",
        "issue",
        "itemType",
        "itemVersion",
        "journalAbbreviation",
        "journalAbbreviation",
        "journalArticle",
        "key",
        "language",
        "libraryCatalog",
        "magazineArticle",
        "newspaperArticle",
        "notes",
        "pages",
        "publicationTitle",
        "rights",
        "shortTitle",
        "tags",
        "title",
        "url",
        "version",
        "volume",
        "webpage",
        "websiteTitle",
        "websiteType",
        "series"
    });

    extern const std::map<std::string, std::string> CREATOR_TYPES_TO_MARC21_MAP;
    extern const std::map<std::string, MARC::Record::BibliographicLevel> ITEM_TYPE_TO_BIBLIOGRAPHIC_LEVEL_MAP;

    const std::string GetCreatorTypeForMarc21(const std::string &zotero_creator_type);
    MARC::Record::BibliographicLevel MapBiblioLevel(const std::string item_type);
    bool IsValidItemType(const std::string item_type);
    std::string OptionalMap(const std::string &key, const std::unordered_map<std::string, std::string> &map);
    std::string NormalizeDate(const std::string date_raw, const std::string strptime_format);
    bool TestForUnknownZoteroKey(const std::shared_ptr<const JSON::ObjectNode> &object_node);


} // end ZoteroTransformation


} // end Zotero
