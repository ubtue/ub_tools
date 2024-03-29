/** \file    HtmlUtil.h
 *  \brief   Declarations of HTML-related utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *  Copyright 2016-2020 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once


#include <string>
#include <vector>
#include <cerrno>


namespace HtmlUtil {


bool DecodeEntityLatin1(const char *entity_string, char * const ch);

inline bool DecodeEntityLatin1(const std::string &entity_string, char * const ch) {
    return DecodeEntityLatin1(entity_string.c_str(), ch);
}


enum UnknownEntityMode { PASS_THROUGH_UNKNOWN_ENTITIES, DELETE_UNKNOWN_ENTITIES };


/** \brief  Replaces all HTML entities in "s" with the actual characters.
 *  \param  s                    The string that may contain optional HTML entities.
 *  \param  unknown_entity_mode  tells whether to remove unknown entities.
 *  \note   It is probably a good idea to call RemoveTags before calling this function.
 */
std::string &ReplaceEntitiesLatin1(std::string * const s, const UnknownEntityMode unknown_entity_mode = DELETE_UNKNOWN_ENTITIES);

inline std::string ReplaceEntitiesLatin1(const std::string &s, const UnknownEntityMode unknown_entity_mode = DELETE_UNKNOWN_ENTITIES) {
    std::string temp_s(s);
    ReplaceEntitiesLatin1(&temp_s, unknown_entity_mode);
    return temp_s;
}


/** \brief  Replaces all HTML entities in "s" with the actual characters.
 *  \param  s                    The string that may contain optional HTML entities.
 *  \param  unknown_entity_mode  If set to DELETE_UNKNOWN_ENTITIES, unknown entities will be replaced with the � character.
 *                               o/w "junk" will be passed through.
 *  \note   It is probably a good idea to call RemoveTags before calling this function.
 */
std::string &ReplaceEntitiesUTF8(std::string * const s, const UnknownEntityMode unknown_entity_mode = PASS_THROUGH_UNKNOWN_ENTITIES);

inline std::string ReplaceEntitiesUTF8(const std::string &s, const UnknownEntityMode unknown_entity_mode = PASS_THROUGH_UNKNOWN_ENTITIES) {
    std::string temp_s(s);
    ReplaceEntitiesUTF8(&temp_s, unknown_entity_mode);
    return temp_s;
}


/** \brief Replaces ampersands, less-than, greater than and double quote symbols with HTML entities. */
std::string HtmlEscape(const std::string &unescaped_text);


/** \brief Replaces ampersands, less-than, greater than and double quote symbols with HTML entities. */
inline std::string HtmlEscape(std::string * const unescaped_text) {
    return *unescaped_text = HtmlEscape(*unescaped_text);
}


/** \brief   Test whether a string is correctly HTML-escaped.
 *  \param   raw_text  The text to test.
 *  \return  True if the string appears to be fully HTML escaped, otherwise false.
 *
 *  This function tests for the presence of '&', '<', '>' and quotes which are not escaped with "&amp;", "&lt;", "&gt;" etc.
 */
bool IsHtmlEscaped(const std::string &raw_text);


/** \brief Extracts all links from an HTML document.
 *  \return The number of extracted hrefs
 */
size_t ExtractAllLinks(const std::string &html_document, std::vector<std::string> * const urls);


// \warning Assumes that "text_with_optional_tags" is UTF8 encoded!
std::string StripHtmlTags(const std::string &text_with_optional_tags, const bool replace_entities = true);


/** \brief Shorten a text but keep HTML structure. */
std::string ShortenText(const std::string &html_document, const size_t max_length);


} // namespace HtmlUtil
