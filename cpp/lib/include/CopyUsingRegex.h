/** \file  CopyUsingRegex.h
 *  \brief  Helper functions for copy file(s) using regex for file name
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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


namespace CopyUsingRegex {

/** \brief   Copy using regex for file name
 *  \param   srcPath  The source folder path
 *  \param   desPath  The destination folder path
 *  \param   fName  Filename with or without regex
 *  \return  void
 */
void CopyFiles(const char* srcPath, const char* desPath, std::string fName);


}