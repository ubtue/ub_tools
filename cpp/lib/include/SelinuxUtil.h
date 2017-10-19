/** \file   SelinuxUtil.h
 *  \brief  Various utility functions related to SElinux
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
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

#ifndef SELINUXUTIL_H
#define SELINUXUTIL_H


#include <cctype>
#include <string>
#include <vector>


namespace SelinuxUtil {


enum Mode { ENFORCING, PERMISSIVE, DISABLED };


void AddFileContext(const std::string &context, const std::string &file_spec);


void AddFileContextIfMissing(const std::string &path, const std::string &context, const std::string &file_spec);


void ApplyChanges(const std::string &path);


void AssertEnabled(const std::string &caller);


void AssertFileHasContext(const std::string &path, const std::string &context);


std::vector<std::string> GetFileContexts(const std::string &path);


Mode GetMode();


bool HasFileContext(const std::string &path, const std::string &context);


} // namespace SelinuxUtil


#endif /* SELINUXUTIL_H */

