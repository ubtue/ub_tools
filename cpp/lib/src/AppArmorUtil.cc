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
#include "JSON.h"


namespace AppArmorUtil {


PROFILE_MODE ParseProfileMode(const std::string &profile_mode) {
    if (profile_mode == "audit")
        return AUDIT;
    if (profile_mode == "complain")
        return COMPLAIN;
    if (profile_mode == "enforce")
        return ENFORCE;

    LOG_ERROR("invalid profile mode: " + profile_mode);
}


std::shared_ptr<JSON::ObjectNode> GetStatusAsJSON() {
    std::string json,stderr;
    if (not ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::Which("aa-status"), { "--json" }, &json, &stderr))
        LOG_ERROR("unable to execute \"aa-status --json\"");

    JSON::Parser parser(json);
    std::shared_ptr<JSON::JSONNode> json_root;
    if (not parser.parse(&json_root))
        LOG_ERROR("unable to parse JSON result from \"aa-status --json\": " + json);

    return JSON::JSONNode::CastToObjectNodeOrDie("root", json_root);
}


std::vector<std::pair<std::string,PROFILE_MODE>> GetProfiles() {
    const auto json_root(GetStatusAsJSON());
    const auto json_profiles(json_root->getObjectNode("profiles"));
    std::vector<std::pair<std::string,PROFILE_MODE>> profiles;
    for (auto &profile_and_mode : *json_profiles) {
        std::shared_ptr<JSON::StringNode> profile_mode_node(JSON::JSONNode::CastToStringNodeOrDie("profile", profile_and_mode.second));
        profiles.emplace_back(std::make_pair(profile_and_mode.first, ParseProfileMode(profile_mode_node->getValue())));
    }
    return profiles;
}


PROFILE_MODE GetProfileMode(const std::string &profile_id) {
    const auto profiles(GetProfiles());
    for (const auto &profile : profiles) {
        if (profile_id == profile.first)
            return profile.second;
    }
    LOG_ERROR("Profile not found: " + profile_id);
}


bool IsAvailable() {
    return (ExecUtil::Which("aa-status") != "");
}


void SetProfileMode(const std::string &profile_id, const PROFILE_MODE profile_mode) {
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
    ExecUtil::ExecOrDie(ExecUtil::Which(executable), { profile_id });
}


}
