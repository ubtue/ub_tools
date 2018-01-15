/** \file   MarcUtil.h
 *  \brief  Various utility functions related to the processing of MARC-21 records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de
 *
 *  \copyright 2014-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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

#ifndef MARC_UTIL_NEW_H
#define MARC_UTIL_NEW_H


#include <unordered_map>
#include "MARC.h"


namespace MARC {
namespace Util {

/** \brief True if a GND code was found in 035$a else false. */
bool GetGNDCode(const MARC::Record &record, std::string *const gnd_code);

} // namespace MARC
} // namespace Util

#endif //MARC_UTIL_NEW_H
