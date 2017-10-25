/** \file   SELinuxUtil.h
 *  \brief  Various utility functions related to SELinux
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
#include <FileUtil.h>


namespace SELinuxUtil {


enum Mode { ENFORCING, PERMISSIVE, DISABLED };


/** \brief  Get the mode SELinux is currently running at
 *  \throws std::runtime_error if the mode could not be determined (via "getenforce")
 */
Mode GetMode();


/** \brief  Check if SELinux is available (installed) on the current system
 */
bool IsAvailable();


/** \brief  Check if SELinux is installed & enabled on the current system.
 *          (Enabled => mode is not set to DISABLED)
 */
bool IsEnabled();


/** \brief  Make sure that SELinux is available and enabled
 *  \throws std::runtime_error if SELinux is not enabled (or not installed)
 */
void AssertEnabled(const std::string &caller);


namespace FileContext {


/** \brief  Add a file context record. The change will only be active if ApplyChanges is called afterwards.
 *  \param  type                a file context type, e.g. httpd_sys_rw_content_t
 *  \param  file_spec           a file specification pattern, e.g. "/var/www(.*)?"
 */
void AddRecord(const std::string &type, const std::string &file_spec);


/** \brief  Add a file context rule, only if the file does not have the context yet.
 *          Also applies changes directly to the given path.
 *  \param  path                path of a file or directory to test
 *  \param  type                a context type, e.g. httpd_sys_rw_content_t
 *  \param  file_spec           a file specification pattern, e.g. "/var/www(.*)?"
 *  \throws std::runtime_error if the type could not be added
 */
void AddRecordIfMissing(const std::string &path, const std::string &type, const std::string &file_spec);


/** \brief  Apply all configured file contexts for the specified path
 *  \note   This also includes all other configurations for the path,
 *          even if they have not been made with this library.
 *  \param  path                path of a file or directory
 */
void ApplyChanges(const std::string &path);


/** \brief  Make sure that the file or directory has the specified context
 *  \param  path                path of a file or directory to test
 *  \param  type                a context type, e.g. httpd_sys_rw_content_t
 *  \throws std::runtime_error if the file doesn't have the context type
 */
void AssertFileHasType(const std::string &path, const std::string &type);


/** \brief  Get the file context of the specified path
 *  \param  path                path of a file or directory
 *  \throws std::runtime_error if the file contexts could not be determined
 */
FileUtil::SELinuxFileContext GetOrDie(const std::string &path);


/** \brief  Check if the path has a certain file context
 *  \param  path                path of a file or directory to test
 *  \param  type                a context type, e.g. httpd_sys_rw_content_t
 */
bool HasFileType(const std::string &path, const std::string &type);


} // namespace FileContext


} // namespace SELinuxUtil


#endif /* SELINUX_UTIL_H */

