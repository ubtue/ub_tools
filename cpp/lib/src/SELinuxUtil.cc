/** \file   SelinuxUtil.cc
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

#include "SELinuxUtil.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"


namespace SELinuxUtil {


void AddFileContextRecord(const std::string &type, const std::string &file_spec) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("semanage"), { "fcontext", "-a", "-t", type, file_spec });
}


void AddFileContextRecordIfMissing(const std::string &path, const std::string &type, const std::string &file_spec) {
    if (not HasFileContextType(path, type)) {
        AddFileContextRecord(type, file_spec);
        ApplyChanges(path);
    }

    if (not HasFileContextType(path, type)) {
        throw new std::runtime_error("in " + std::string(__func__) +": "
                                     + "could not set context \"" + type + "\" for \"" + path + "\" using " + file_spec);
    }
}


void ApplyChanges(const std::string &path) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("restorecon"), { "-R", "-v", path });
}


void AssertEnabled(const std::string &caller) {
    if (not IsEnabled())
        throw new std::runtime_error("in " + caller +": SElinux is disabled!");
}


void AssertFileHasContextType(const std::string &path, const std::string &type) {
    if (not HasFileContextType(path, type)) {
        throw new std::runtime_error("in " + std::string(__func__) +": "
                                     + "file " + " doesn't have context type " + type);
    }
}


FileUtil::SELinuxFileContext GetFileContextOrDie(const std::string &path) {
    AssertEnabled(std::string(__func__));
    return FileUtil::SELinuxFileContext(path);
}


Mode GetMode() {
    const std::string getenforce_binary(ExecUtil::Which("getenforce"));
    std::string getenforce;
    if (not getenforce_binary.empty()) {
        if (ExecUtil::ExecSubcommandAndCaptureStdout(getenforce_binary, &getenforce)) {
            StringUtil::TrimWhite(&getenforce);

            if (getenforce == "Enforcing")
                return ENFORCING;
            else if (getenforce == "Permissive")
                return PERMISSIVE;
            else if (getenforce == "Disabled")
                return DISABLED;
        }
    }

    throw new std::runtime_error("in " + std::string(__func__) +": "
                                 + " could not detemine mode via getenforce ");
}


bool HasFileContextType(const std::string &path, const std::string &type) {
    FileUtil::SELinuxFileContext context(path);
    return (context.getType() == type);
}


bool IsAvailable() {
    return (ExecUtil::Which("getenforce") != "");
}


bool IsEnabled() {
    return (IsAvailable() && (GetMode() != DISABLED));
}


} // namespace SELinuxUtil
