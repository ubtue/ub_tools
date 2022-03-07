/** \file   SELinuxUtil.cc
 *  \brief  Various utility functions related to SELinux
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2017-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"


namespace SELinuxUtil {


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

    throw std::runtime_error("in " + std::string(__func__) + ": " + " could not detemine mode via getenforce ");
}


bool IsAvailable() {
    return (not ExecUtil::Which("getenforce").empty());
}


bool IsEnabled() {
    return (IsAvailable() && (GetMode() != DISABLED));
}


void AssertEnabled(const std::string &caller) {
    if (not IsEnabled())
        throw std::runtime_error("in " + caller + ": SElinux is disabled!");
}


namespace Boolean {


static inline std::string Bool2String(const bool value) {
    return value ? "on" : "off";
}


void Set(const std::string &name, const bool value, const bool permanent) {
    AssertEnabled(std::string(__func__));

    std::vector<std::string> args;
    if (permanent)
        args = { name, "-P", Bool2String(value) };
    else
        args = { name, Bool2String(value) };

    ExecUtil::Exec(ExecUtil::Which("setsebool"), args);
}


} // namespace Boolean


namespace FileContext {


void AddRecord(const std::string &type, const std::string &file_spec) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("semanage"), { "fcontext", "-a", "-t", type, file_spec });
}


void AddRecordIfMissing(const std::string &path, const std::string &type, const std::string &file_spec) {
    if (not HasFileType(path, type)) {
        AddRecord(type, file_spec);
        ApplyChanges(path);
    }

    if (not HasFileType(path, type)) {
        throw std::runtime_error("in " + std::string(__func__) + ": " + "could not set context \"" + type + "\" for \"" + path + "\" using "
                                 + file_spec);
    }
}


void ApplyChanges(const std::string &path) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("restorecon"), { "-R", "-v", path });
}


void AssertFileHasType(const std::string &path, const std::string &type) {
    if (not HasFileType(path, type)) {
        throw std::runtime_error("in " + std::string(__func__) + ": " + "file " + " doesn't have context type " + type);
    }
}


FileUtil::SELinuxFileContext GetOrDie(const std::string &path) {
    AssertEnabled(std::string(__func__));
    return FileUtil::SELinuxFileContext(path);
}


bool HasFileType(const std::string &path, const std::string &type) {
    FileUtil::SELinuxFileContext context(path);
    return (context.getType() == type);
}


} // namespace FileContext


namespace Port {


void AddRecord(const std::string &type, const std::string &protocol, const uint16_t port) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("semanage"), { "port", "-a", "-t", type, "-p", protocol, std::to_string(port) });
}


void AddRecordIfMissing(const std::string &type, const std::string &protocol, const uint16_t port) {
    AssertEnabled(std::string(__func__));
    if (not HasPortType(type, protocol, port))
        AddRecord(type, protocol, port);
}


bool HasPortType(const std::string &type, const std::string &protocol, const uint16_t port) {
    AssertEnabled(std::string(__func__));
    std::string semanage_output, semanage_error;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which("semanage"), { "port", "-l" }, &semanage_output, &semanage_error);

    static RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory(type + "\\s+" + protocol + ".*" + "\\b" + std::to_string(port) + "\\b"));
    return matcher->matched(semanage_output);
}


} // namespace Port


} // namespace SELinuxUtil
