/** \file   AppArmorUtil.h
 *  \brief  Various utility functions related to AppArmor
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <utility>
#include <vector>


namespace AppArmorUtil {


enum ProfileMode { AUDIT, COMPLAIN, ENFORCE };


/** \brief  Check if AppArmor is enabled (installed+active) on the current system. */
bool IsEnabled();

/** \brief  Return a list of all profile ids and their mode. */
std::vector<std::pair<std::string, ProfileMode>> GetProfiles();

/** \brief  Return the mode for the given profile. */
ProfileMode GetProfileMode(const std::string &profile_id);

/** \brief  Install the profile from the given path as a local profile.
 *          The filename of the profile_path will later be used as the profile_id.
 */
void InstallLocalProfile(const std::string &profile_path);

/** \brief  Set the mode for the given local profile. */
void SetLocalProfileMode(const std::string &profile_id, const ProfileMode profile_mode);


} // namespace AppArmorUtil
