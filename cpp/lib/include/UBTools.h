/** \file   UBTools.h
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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


namespace UBTools {


inline std::string GetUBWebProxyURL() {
    return "http://wwwproxy.uni-tuebingen.de:3128";
}


// \return A slash-terminated absolute path.
inline std::string GetTuelibPath() {
    return "/usr/local/var/lib/tuelib/";
}

// \return A slash-terminated absolute path.
inline std::string GetTueFindLogPath() {
    return "/usr/local/var/log/tuefind/";
}

// \return  A slash-terminated absolute path.
inline std::string GetTueLocalTmpPath() {
    return "/usr/local/var/tmp/";
}


} // namespace UBTools
