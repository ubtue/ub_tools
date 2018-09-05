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
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const std::string systemd_executable("systemctl");


bool SystemdUtil::IsAvailable() {
    return not ExecUtil::Which(systemd_executable).empty();
}


void SystemdUtil::DisableUnit(const std::string unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(systemd_executable), { "disable", unit });
}


void SystemdUtil::EnableUnit(const std::string unit) {
    ExecUtil::ExecOrDie(ExecUtil::Which(systemd_executable), { "enable", unit });
}


bool SystemdUtil::IsUnitAvailable(const std::string unit) {
    std::string out, err;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which(systemd_executable), { "--all", "list-unit-files" }, &out, &err);

    std::string regex_pattern("^" + unit + "\\.service"), regex_err;
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(regex_pattern, &regex_err, RegexMatcher::MULTILINE));
    if (matcher == nullptr)
        LOG_ERROR("Failed to compile pattern \"" + regex_pattern + "\": " + regex_err);

    return matcher->matched(out);
}


bool SystemdUtil::IsUnitEnabled(const std::string unit) {
    std::string out, err;
    ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which(systemd_executable), { "is-enabled", unit }, &out, &err);
    return StringUtil::StartsWith(out, "enabled", /* ignore_case */ true);
}
