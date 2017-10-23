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

#ifndef SELINUX_UTIL_H
#define SELINUX_UTIL_H


#include <cctype>
#include <string>
#include <vector>


namespace SelinuxUtil {


enum Mode { ENFORCING, PERMISSIVE, DISABLED };


/** \brief  Add a file context rule. The change will only be active if ApplyChanges is called afterwards.
 *  \param  context             a context type, e.g. httpd_sys_rw_content_t
 *  \param  file_spec           a file specification pattern, e.g. "/var/www(.*)?"
 */
void AddFileContext(const std::string &context, const std::string &file_spec);


/** \brief  Add a file context rule, only if the file does not have the context yet.
 *          Also applies changes directly to the given path.
 *  \param  path                path of a file or directory to test
 *  \param  context             a context type, e.g. httpd_sys_rw_content_t
 *  \param  file_spec           a file specification pattern, e.g. "/var/www(.*)?"
 */
void AddFileContextIfMissing(const std::string &path, const std::string &context, const std::string &file_spec);


/** \brief  Apply all configured file contexts for the specified path
 *  \param  path                path of a file or directory
 */
void ApplyChanges(const std::string &path);


/** \brief  Make sure that Selinux is available and enabled, else throw error
 */
void AssertEnabled(const std::string &caller);


/** \brief  Make sure that the file or directory has the specified context, else throw error.
 *  \param  path                path of a file or directory to test
 *  \param  context             a context type, e.g. httpd_sys_rw_content_t
 */
void AssertFileHasContext(const std::string &path, const std::string &context);


/** \brief  Get all file contexts set for the specified path
 *  \param  path                path of a file or directory
 *  \return A list of all contexts for the path
 */
std::vector<std::string> GetFileContexts(const std::string &path);


/** \brief  Get the mode Selinux is currently running ("getenforce")
 *  \return Mode enum
 */
Mode GetMode();


/** \brief  Check if the path has a certain context
 *  \param  path                path of a file or directory to test
 *  \param  context             a context type, e.g. httpd_sys_rw_content_t
 *  \return bool
 */
bool HasFileContext(const std::string &path, const std::string &context);


/** \brief  Check if Selinux is available (installed) on the current system
 *  \return bool
 */
bool IsAvailable();


/** \brief  Check if Selinux is installed & enabled on the current system.
 *  \return bool
 */
bool IsEnabled();


} // namespace SelinuxUtil


#endif /* SELINUX_UTIL_H */

