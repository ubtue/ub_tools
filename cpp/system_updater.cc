/** \file   system_updater.cc
 *  \brief  Runs scripts from /usr/local/ub_tools/data/system_updates
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <cstdlib>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


const std::string VERSION_PATH(UBTools::GetTuelibPath() + "system_version");


unsigned GetCurrentVersion() {
    if (not FileUtil::Exists(VERSION_PATH))
        return 0;

    const auto version_number_string(FileUtil::ReadStringOrDie(VERSION_PATH));

    unsigned version;
    if (not StringUtil::ToUnsigned(version_number_string, &version))
        LOG_ERROR("can't convert the contents of \"" + VERSION_PATH + "\" to an unsigned number!");

    return version;
}


unsigned GetVersionFromScriptName(const std::string &script_name) {
    const auto basename(FileUtil::GetBasename(script_name));
    if (not StringUtil::EndsWith(script_name, ".sh"))
        LOG_ERROR("unexpected script name: \"" + script_name + "\"!");
    return StringUtil::ToUnsignedOrDie(script_name.substr(0, script_name.length() - 3 /* ".sh" */));
}


// \return True if the version number of "script_name1" is smaller then the version number
//         of "script_name2".
inline bool ScriptLessThan(const std::string &script_name1, const std::string &script_name2) {
    return GetVersionFromScriptName(script_name1) < GetVersionFromScriptName(script_name2);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("path_to_update_scripts");

    const auto current_version(GetCurrentVersion());

    std::vector<std::string> script_names;
    const std::string SYSTEM_UPDATES_DIR(argv[1]);
    FileUtil::Directory system_updates_dir(SYSTEM_UPDATES_DIR, "^\\d+.sh$");
    for (const auto &entry : system_updates_dir) {
        if (GetVersionFromScriptName(entry.getName()) > current_version)
            script_names.emplace_back(entry.getName());
    }
    if (script_names.empty()) {
        LOG_INFO("nothing to be done!");
        return EXIT_SUCCESS;
    }

    std::sort(script_names.begin(), script_names.end(), ScriptLessThan);

    for (const auto &script_name : script_names) {
        LOG_INFO("Running " + script_name);
        ExecUtil::ExecOrDie(SYSTEM_UPDATES_DIR + "/" + script_name);

        // We want to write the version number after each script
        // in case anything goes wrong, to avoid double execution
        // of successfully run scripts
        const auto version_number(script_name.substr(0, script_name.size() - 3 /* ".sh" */));
        FileUtil::WriteStringOrDie(VERSION_PATH, version_number);
    }

    return EXIT_SUCCESS;
}
