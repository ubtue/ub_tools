/** \brief Test program for interfacing to the VuFind MySQL tables.
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
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <cstring>
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <File.h>
#include <RegexMatcher.h>
#include <StringUtil.h>
#include <util.h>
#include <UrlUtil.h>
#include <VuFind.h>


void Usage() {
    std::cerr << "usage: " << ::progname << '\n';
    std::exit(EXIT_FAILURE);
}


/** \brief Attemps to parse something like 'database = "mysql://ruschein:xfgYu8z@localhost:3345/vufind"' */
void GetAuthentisationCredentialsHostAndDbName(const std::string &mysql_url, std::string * const user, std::string * const passwd,
					       std::string * host, unsigned * const port, std::string * const db_name)
{
    static const RegexMatcher * const mysql_url_matcher(
        RegexMatcher::RegexMatcherFactory("mysql://([^:]+):([^@]+)@([^:/]+)(\\d+:)?/(.+)"));
    std::string err_msg;
    if (not mysql_url_matcher->matched(mysql_url, &err_msg))
	throw std::runtime_error("\"" + mysql_url + "\" does not look like an expected MySQL URL! (" + err_msg + ")");

    *user    = (*mysql_url_matcher)[1];
    *passwd  = (*mysql_url_matcher)[2];
    *host    = (*mysql_url_matcher)[3];
    *db_name = (*mysql_url_matcher)[5];

    const std::string port_plus_colon((*mysql_url_matcher)[4]);
    if (port_plus_colon.empty())
	*port = MYSQL_PORT;
    else
	*port = StringUtil::ToUnsigned(port_plus_colon.substr(0, port_plus_colon.length() - 1));
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 1)
	Usage();

//    try {
	const std::string database_conf_filename(VuFind::VUFIND_HOME + "/" + VuFind::DATABASE_CONF);
	File database_conf(database_conf_filename, "r", File::THROW_ON_ERROR);
	const std::string line(database_conf.getline());
	const size_t schema_pos(line.find("mysql://"));
	if (schema_pos == std::string::npos)
	    Error("MySQL schema not found in \"" + database_conf_filename + "\"!");
	std::string mysql_url(StringUtil::RightTrim(line.substr(schema_pos)));
	mysql_url.resize(mysql_url.size() - 1); // Remove trailing double quote.

	std::string user, passwd, host, db_name;
	unsigned port;
	GetAuthentisationCredentialsHostAndDbName(mysql_url, &user, &passwd, &host, &port, &db_name);

	std::cout << "user=" << user << '\n';
	std::cout << "passwd=" << passwd << '\n';
	std::cout << "host=" << host << '\n';
	std::cout << "port=" << port << '\n';
	std::cout << "db_name=" << db_name << '\n';
/*
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));	
    }
*/
}
