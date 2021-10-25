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
#include <iostream>
#include "DbConnection.h"
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

    const auto version_number_string(StringUtil::TrimWhite(FileUtil::ReadStringOrDie(VERSION_PATH)));

    unsigned version;
    if (not StringUtil::ToUnsigned(version_number_string, &version))
        LOG_ERROR("can't convert the contents of \"" + VERSION_PATH + "\" to an unsigned number! (\"" + version_number_string + "\")");

    return version;
}


unsigned GetVersionFromScriptName(const std::string &script_name) {
    const auto basename(FileUtil::GetBasename(script_name));
    if (StringUtil::EndsWith(script_name, ".sh"))
        return StringUtil::ToUnsignedOrDie(script_name.substr(0, script_name.length() - 3 /* ".sh" */));
    else if (StringUtil::EndsWith(script_name, ".sql")) {
        return StringUtil::ToUnsignedOrDie(script_name.substr(0, script_name.find('.') /* ".xxx.sql" */));
    }
    else
        LOG_ERROR("unexpected script name: \"" + script_name + "\"!");
}


// \return True if the version number of "script_name1" is smaller then the version number
//         of "script_name2".
inline bool ScriptLessThan(const std::string &script_name1, const std::string &script_name2) {
    return GetVersionFromScriptName(script_name1) < GetVersionFromScriptName(script_name2);
}


void SplitIntoDatabaseAndVersion(const std::string &update_filename, std::string * const database, unsigned * const version) {
    const auto first_dot_pos(update_filename.find('.'));
    if (first_dot_pos == std::string::npos or first_dot_pos == 0 or first_dot_pos == update_filename.length() - 1)
        LOG_ERROR("invalid update filename \"" + update_filename + "\"!");

    if (not StringUtil::ToUnsigned(update_filename.substr(0, first_dot_pos), version))
        LOG_ERROR("bad or missing version in update filename \"" + update_filename + "\"!");
    *database = update_filename.substr(first_dot_pos + 1, update_filename.find_last_of('.') - (first_dot_pos + 1));
}


void ApplyUpdate(DbConnection * const db_connection, const std::string &update_directory_path,
                 const std::string &update_filename, std::string * const current_schema, const std::string &last_schema)
{
    std::string database;
    unsigned update_version;
    SplitIntoDatabaseAndVersion(update_filename, &database, &update_version);
    *current_schema = database;

    if (not db_connection->mySQLDatabaseExists(database)) {
        LOG_ERROR("database \"" + database + "\" does not exist, skipping file " + update_filename);
        return;
    }

    if (last_schema.empty() or database != last_schema) {
        LOG_INFO("switching to database: " + database);
        db_connection->queryOrDie("USE " + database);
    }

    LOG_INFO("applying update " + std::to_string(update_version) + " to database \"" + database + "\".");
    db_connection->queryFileOrDie(update_directory_path + "/" + update_filename);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("path_to_update_scripts");

    bool dry_run = false;

    DbConnection db_connection(DbConnection::UBToolsFactory());
    const auto current_version(GetCurrentVersion());

    std::vector<std::string> script_names;
    const std::string SYSTEM_UPDATES_DIR(argv[1]);
    FileUtil::Directory system_updates_dir(SYSTEM_UPDATES_DIR, "(^\\d+.sh$|\\d+.(?:ixtheo|ub_tools|vufind|krimdok).sql)");
    for (const auto &entry : system_updates_dir) {
        if (GetVersionFromScriptName(entry.getName()) > current_version)
            script_names.emplace_back(entry.getName());
    }
    if (script_names.empty()) {
        LOG_INFO("nothing to be done!");
        return EXIT_SUCCESS;
    }

    std::sort(script_names.begin(), script_names.end(), ScriptLessThan);
    std::string last_schema, current_schema;

    for (const auto &script_name : script_names) {
        LOG_INFO("Running " + script_name);
        if (script_name.ends_with(".sh")) {
            if (dry_run)
                std::cout << SYSTEM_UPDATES_DIR << "---" << script_name << std::endl;
            else
                ExecUtil::ExecOrDie(SYSTEM_UPDATES_DIR + "/" + script_name);
        }
        else if (script_name.ends_with(".sql")) {
            if (dry_run)
                std::cout << SYSTEM_UPDATES_DIR << " " << script_name << std::endl;
            else
                ApplyUpdate(&db_connection, SYSTEM_UPDATES_DIR, script_name, &current_schema, last_schema);
            last_schema = current_schema;
        }
        else
            continue;

        // We want to write the version number after each script
        // in case anything goes wrong, to avoid double execution
        // of successfully run scripts
        const unsigned version_number(GetVersionFromScriptName(script_name));
        FileUtil::WriteStringOrDie(VERSION_PATH, std::to_string(version_number));
    }

    return EXIT_SUCCESS;
}
