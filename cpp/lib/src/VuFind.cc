/** \file   VuFind.cc
 *  \brief  VuFind-related constants and utility functions.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "VuFind.h"
#include <stdexcept>
#include "File.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"


namespace VuFind {


static const std::string DATABASE_CONF("local/config/vufind/local_overrides/database.conf");
static const std::string DATABASE_CONF_ALTERNATIVE("local/tufind/local_overrides/database.conf");


std::string GetDefaultDatabaseConf() {
    const std::string VUFIND_HOME(MiscUtil::GetEnv("VUFIND_HOME"));
    const std::string database_conf(VUFIND_HOME + "/" + VuFind::DATABASE_CONF);
    return FileUtil::Exists(database_conf)  ? database_conf : VUFIND_HOME + "/" + VuFind::DATABASE_CONF_ALTERNATIVE;
}


void GetMysqlURL(std::string * const mysql_url, const std::string &vufind_config_file_path)
        throw(std::exception)
{
    const std::string database_conf_filename(vufind_config_file_path.empty()
                                             ? GetDefaultDatabaseConf() : vufind_config_file_path);
    File database_conf(database_conf_filename, "r", File::THROW_ON_ERROR);
    const std::string line(database_conf.getline());
    const size_t schema_pos(line.find("mysql://"));
    if (schema_pos == std::string::npos)
        throw std::runtime_error("MySQL schema not found in \"" + database_conf_filename + "\"!");
    *mysql_url = StringUtil::RightTrim(line.substr(schema_pos));
    mysql_url->resize(mysql_url->size() - 1); // Remove trailing double quote.
}


} // namespace VuFind
