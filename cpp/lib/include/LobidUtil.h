/*  \brief Utility Functions for Lobid API (HBZ) to get title data, norm data (GND) and organisational data
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
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


#include <string>
#include <vector>


namespace LobidUtil {


/** \note  additional_query_params (if necessary) are meant to be passed in Lucene query syntax,
 *         e.g. "dateOfBirth:19* AND professionOrOccupation.label:theolog*"
 *  \see   - http://lobid.org/gnd/api
 *         - http://lobid.org/organisations/api/de
 *         - http://lobid.org/resources/api
 */
std::string GetAuthorGNDNumber(const std::string &author, const std::string &additional_query_params = "");
std::string GetAuthorGNDNumber(const std::string &author_surname, const std::string &author_firstname,
                                        const std::string &additional_query_params);
std::vector<std::string> GetAuthorProfessions(const std::string &author, const std::string &additional_query_params = "");
std::string GetOrganisationISIL(const std::string &organisation, const std::string &additional_query_params = "");
std::string GetTitleDOI(const std::string &title, const std::string &additional_query_params = "");


} // namespace LobidUtil
