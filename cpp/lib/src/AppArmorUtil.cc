/** \file   AppArmorUtil.cc
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

#include "AppArmorUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "JSON.h"


namespace AppArmorUtil {


ProfileMode ParseProfileMode(const std::string &profile_mode) {
    if (profile_mode == "audit")
        return AUDIT;
    if (profile_mode == "complain")
        return COMPLAIN;
    if (profile_mode == "enforce")
        return ENFORCE;

    LOG_ERROR("invalid profile mode: " + profile_mode);
}


std::shared_ptr<JSON::ObjectNode> GetStatusAsJSON() {
    std::string json, stderr;
    if (not ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which("aa-status"), { "--json" }, &json, &stderr))
        LOG_ERROR("unable to execute \"aa-status --json\"");

    JSON::Parser parser(json);
    std::shared_ptr<JSON::JSONNode> json_root;
    if (not parser.parse(&json_root))
        LOG_ERROR("unable to parse JSON result from \"aa-status --json\": " + json);

    return JSON::JSONNode::CastToObjectNodeOrDie("root", json_root);
}


std::vector<std::pair<std::string, ProfileMode>> GetProfiles() {
    const auto json_root(GetStatusAsJSON());
    const auto json_profiles(json_root->getObjectNode("profiles"));
    std::vector<std::pair<std::string, ProfileMode>> profiles;
    for (auto &profile_and_mode : *json_profiles) {
        const std::shared_ptr<const JSON::StringNode> profile_mode_node(
            JSON::JSONNode::CastToStringNodeOrDie("profile", profile_and_mode.second));
        profiles.emplace_back(std::make_pair(profile_and_mode.first, ParseProfileMode(profile_mode_node->getValue())));
    }
    return profiles;
}


ProfileMode GetProfileMode(const std::string &profile_id) {
    const auto profiles(GetProfiles());
    for (const auto &profile : profiles) {
        if (profile_id == profile.first)
            return profile.second;
    }
    LOG_ERROR("Profile not found: " + profile_id);
}


static const std::string PROFILES_DIR("/etc/apparmor.d/local");


void InstallLocalProfile(const std::string &profile_path) {
    const std::string target_path(PROFILES_DIR + "/" + FileUtil::GetBasename(profile_path));
    FileUtil::CopyOrDie(profile_path, target_path);
}


bool IsEnabled() {
    const std::string executable(ExecUtil::Which("aa-enabled"));
    if (executable.empty())
        return false;
    return (ExecUtil::Exec(executable, { "--quiet" }) == 0);
}


void SetLocalProfileMode(const std::string &profile_id, const ProfileMode profile_mode) {
    std::string executable;
    switch (profile_mode) {
    case AUDIT:
        executable = "aa-audit";
        break;
    case COMPLAIN:
        executable = "aa-complain";
        break;
    case ENFORCE:
        executable = "aa-enforce";
        break;
    }
    ExecUtil::ExecOrDie(ExecUtil::Which(executable), { PROFILES_DIR + "/" + profile_id });
}


} // namespace AppArmorUtil
