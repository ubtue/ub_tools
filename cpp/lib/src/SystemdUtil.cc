/** \file   SystemdUtil.cc
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
#include "SystemdUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const std::string SYSTEMD_EXECUTABLE("systemctl");
const std::string SYSTEMD_SERVICE_DIRECTORY("/etc/systemd/system/");


bool SystemdUtil::IsAvailable() {
    if (ExecUtil::Which(SYSTEMD_EXECUTABLE).empty())
        return false;

    std::unordered_set<unsigned> pids(ExecUtil::FindActivePrograms("systemd"));
    return pids.find(1) != pids.end();
}


void SystemdUtil::Reload() {
    ExecUtil::ExecOrDie(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "daemon-reload"});
}


void SystemdUtil::DisableUnit(const std::string &unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "disable", unit });
}


void SystemdUtil::EnableUnit(const std::string &unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "enable", unit });
}


void SystemdUtil::InstallUnit(const std::string &service_file_path) {
    std::string service_file_dirname, service_file_basename;
    FileUtil::DirnameAndBasename(service_file_path, &service_file_dirname, &service_file_basename);

    ExecUtil::ExecOrDie(ExecUtil::Which("mkdir"), { "-p", SYSTEMD_SERVICE_DIRECTORY });
    FileUtil::CopyOrDie(service_file_path, SYSTEMD_SERVICE_DIRECTORY + service_file_basename);
    SystemdUtil::Reload();
}


bool SystemdUtil::IsUnitAvailable(const std::string &unit) {
    std::string out, err;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "--all", "list-unit-files" }, &out, &err);
    return RegexMatcher::Matched("^" + unit + "\\.service", out, RegexMatcher::MULTILINE);
}


bool SystemdUtil::IsUnitEnabled(const std::string &unit) {
    std::string out, err;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "is-enabled", unit }, &out, &err);
    return StringUtil::StartsWith(out, "enabled", /* ignore_case */ true);
}


bool SystemdUtil::IsUnitRunning(const std::string &unit) {
    std::string out, err;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "status", unit }, &out, &err);
    return RegexMatcher::Matched("(running)", out);
}


void SystemdUtil::RestartUnit(const std::string &unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "restart", unit });
}


void SystemdUtil::StartUnit(const std::string &unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(SYSTEMD_EXECUTABLE), { "start", unit });
}
