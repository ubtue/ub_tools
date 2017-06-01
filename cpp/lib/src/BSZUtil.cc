/** \file   BSZUtil.h
 *  \brief  Various utility functions related to data etc. having to do w/ the BSZ.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
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

#include "BSZUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace BSZUtil {


// Use the following indicators to select whether to fully delete a record or remove its local data
// For a description of indicators
// c.f. https://wiki.bsz-bw.de/doku.php?id=v-team:daten:datendienste:sekkor (20160426)
const char FULL_RECORD_DELETE_INDICATORS[] = { 'A', 'B', 'C', 'D', 'E' };
const char LOCAL_DATA_DELETE_INDICATORS[] = { '3', '4', '5', '9' };
const size_t MIN_LINE_LENGTH = 21;


void ExtractDeletionIds(File * const deletion_list, std::unordered_set <std::string> * const delete_full_record_ids,
                        std::unordered_set <std::string> * const local_deletion_ids)
{
    const size_t PPN_LENGTH(9);
    const size_t PPN_START_INDEX(12);
    const size_t SEPARATOR_INDEX(PPN_START_INDEX - 1);

    unsigned line_no(0);
    while (not deletion_list->eof()) {
        const std::string line(StringUtil::Trim(deletion_list->getline()));
        ++line_no;
        if (unlikely(line.empty())) // Ignore empty lines.
            continue;
        if (line.length() < PPN_START_INDEX)
            Error("short line " + std::to_string(line_no) + " in deletion list file \"" + deletion_list->getPath()
                  + "\": \"" + line + "\"!");
        for (const char indicator : FULL_RECORD_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                delete_full_record_ids->insert(line.substr(PPN_START_INDEX)); // extract PPN
                continue;
            }
        }
        for (const char indicator : LOCAL_DATA_DELETE_INDICATORS) {
            if (line[SEPARATOR_INDEX] == indicator) {
                if (line.length() < MIN_LINE_LENGTH)
                    Error("unexpected line length " + std::to_string(line.length()) + " for local entry on line "
                          + std::to_string(line_no) + " in deletion list file \"" + deletion_list->getPath() + "\"!");
                local_deletion_ids->insert(line.substr(PPN_START_INDEX, PPN_LENGTH)); // extract ELN
                continue;
            }
        }
        Warning("in \"" + deletion_list->getPath() + " \" on line #" + std::to_string(line_no)
                + " unknown indicator: '" + line.substr(SEPARATOR_INDEX, 1) + "'!");
    }
}


} // namespace BSZUtil
