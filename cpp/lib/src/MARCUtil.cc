/** \file   MarcUtil.cc
 *  \brief  Implementation of various utility functions related to the processing of MARC-21 and MARC-XML records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Johannes Riedl (johannes.riedl@uni-tuebingen.de)
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

#include "MARC.h"
#include "MARCUtil.h"


namespace MARC {
namespace Util {

bool GetGNDCode(const Record &record, std::string * const gnd_code) {
    gnd_code->clear();
    for (auto _035_field : record.getTagRange("035")) {
        const Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_field(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_field, "(DE-588)")) {
            *gnd_code = _035a_field.substr(8);
            return not gnd_code->empty();
        }
    }
    return false;
}

} // namespace MARC
} // namespace Util
