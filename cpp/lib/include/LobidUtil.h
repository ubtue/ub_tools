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


std::string GetAuthorPPN(const std::string &author);
std::vector<std::string> GetAuthorProfessions(const std::string &author);
std::string GetOrganisationISIL(const std::string &organisation);
std::string GetTitleDOI(const std::string &title);


} // namespace LobidUtil
