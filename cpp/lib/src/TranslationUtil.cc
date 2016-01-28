/** \file   TranslationUtil.cc
 *  \brief  Implementation of the DbConnection class.
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
#include "TranslationUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace TranslationUtil {


std::string GetId(DbConnection * const connection, const std::string &german_text) {
    const std::string SELECT_EXISTING("SELECT id FROM translations WHERE text=\"" + connection->escapeString(german_text)
				      + "\" AND language_code=\"deu\"");
    if (not connection->query(SELECT_EXISTING))
        Error("in TranslationUtil::GetId: SELECT failed: " + SELECT_EXISTING + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet id_result_set(connection->getLastResultSet());
    std::string id;
    if (not id_result_set.empty())
	return id_result_set.getNextRow()["id"];
    else { // We don't have any entries for this German term yet.
	const std::string SELECT_MAX_ID("SELECT MAX(id) FROM translations");
	if (not connection->query(SELECT_MAX_ID))
	    Error("in TranslationUtil::GetId: SELECT failed: " + SELECT_MAX_ID + " (" + connection->getLastErrorMessage() + ")");
	DbResultSet max_id_result_set(connection->getLastResultSet());
	if (max_id_result_set.empty())
	    return "1";

	const std::string max_id(max_id_result_set.getNextRow()["MAX(id)"]); // Apparently SQL NULL can be  returned
	                                                                     // which leads to an empty string here.
	return std::to_string(max_id.empty() ? 1 : StringUtil::ToUnsigned(max_id) + 1);
    }
}


} // namespace TranslationUtil
