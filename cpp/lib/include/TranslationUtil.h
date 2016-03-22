/** \file   TranslationUtil.h
 *  \brief  Utility functions used by our translation-related tools.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef TRANSLATION_UTIL_H
#define TRANSLATION_UTIL_H


#include <string>
#include "DbConnection.h"


namespace TranslationUtil {


/* \brief Get the ID corresponding to the German text.
 * \note  If the text is in our database, we return the ID associated w/ the text, o/w/ we return "MAX(id) + 1".
 */
std::string GetId(DbConnection * const connection, const std::string &german_text);


/** \note Aborts if "international_2letter_code" is unknown. */
std::string MapInternational2LetterCodeToGerman3LetterCode(const std::string &international_2letter_code);


/** \note Aborts if "german_3letter_code" is unknown. */
std::string MapGerman3LetterCodeToInternational2LetterCode(const std::string &german_3letter_code);


} // namespace TranslationUtil

   
#endif // ifndef TRANSLATION_UTIL_H
