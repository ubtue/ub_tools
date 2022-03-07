/** \file   SystemdUtil.h
 *  \brief  Helper functions to use with systemd
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


namespace SystemdUtil {


// Systemd-based functions
bool IsAvailable();
void Reload();

// Unit-based functions
void DisableUnit(const std::string &unit);
void EnableUnit(const std::string &unit);
void InstallUnit(const std::string &service_file_path);
bool IsUnitAvailable(const std::string &unit);
bool IsUnitEnabled(const std::string &unit);
bool IsUnitRunning(const std::string &unit);
void RestartUnit(const std::string &unit);
void StartUnit(const std::string &unit);
void StopUnit(const std::string &unit);


} // namespace SystemdUtil
