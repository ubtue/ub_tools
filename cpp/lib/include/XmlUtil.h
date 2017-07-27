/** \file   XmlUtil.h
 *  \brief  XML-related utility functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef XML_UTIL_H
#define XML_UTIL_H


#include <string>


namespace XmlUtil {


// Replaces entities in-place and resizes "data", if necessary.
bool DecodeEntities(std::string * const data);


/** \brief  Replaces entities in "data".
 *  \param  data A string containing zero or more XML entities.
 *  \return The decoded string.
 *  \throws std::runtime_error if there was a decoding error.
 */
std::string DecodeEntities(std::string data);


} // namespace XmlUtil


#endif // ifndef XML_UTIL_H
