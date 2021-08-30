/** \file   rss_feed_generator.cc
 *  \brief  A program that generates an RSS feed using items from a database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include <iostream>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] time_window [xml_output_path]\n"
              << "       \"time_window\", which is in hours, specifies how far back we go in secting items from the database.\n"
              << "       If \"xml_output_path\" has not been specified an HTTP header will be written and the\n"
              << "       generated XML will be written to stdout using CR\\LF line ends.\n\n";
    std::exit(EXIT_FAILURE);
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "rss_aggregator.conf");


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    unsigned time_window;
    if (not StringUtil::ToUnsigned(argv[1], &time_window) or time_window == 0)
        LOG_ERROR("bad time window \"" + std::string(argv[1]) + "\"!");

    bool cgi_mode;
    std::string output_filename;
    if (argc == 3) {
        output_filename = argv[2];
        cgi_mode        = false;
    } else {
        output_filename = "/proc/self/fd/1";
        cgi_mode        = true;
    }

    IniFile ini_file(CONF_FILE_PATH);
    DbConnection db_connection(DbConnection::MySQLFactory(ini_file));

    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(output_filename));

    if (cgi_mode)
        (*output) << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    const std::string LINE_END(cgi_mode ? "\r\n" :  "\n");

    const time_t now(std::time(nullptr));
    const time_t cutoff(now - time_window * 3600);
    db_connection.queryOrDie("SELECT * FROM rss_aggregator WHERE insertion_time >= '" + SqlUtil::TimeTToDatetime(cutoff) + "'");
    DbResultSet result_set(db_connection.getLastResultSet());

    (*output) << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" << LINE_END;
    (*output) << "<rss version=\"2.0\">" << LINE_END;
    (*output) << "<channel>" << LINE_END;
    (*output) << "  <title>" << ini_file.getString("CGI Params", "feed_title") << "</title>" << LINE_END;
    (*output) << "  <link>" << ini_file.getString("CGI Params", "feed_link") << "</link>" << LINE_END;
    (*output) << "  <description>" << ini_file.getString("CGI Params", "feed_description") << "</description>" << LINE_END;

    while (const DbRow db_row = result_set.getNextRow()) {
        (*output) << "  <item>" << LINE_END;
        (*output) << "    <title>" << db_row["serial_name"] << "</title>" << LINE_END;
        (*output) << "    <link>" << db_row["item_url"] << "</link>" << LINE_END;
        (*output) << "    <description>" << db_row["title_and_or_description"] << "</description>" << LINE_END;
        (*output) << "  </item>" << LINE_END;
    }

    (*output) << "</channel>" << LINE_END;
    (*output) << "</rss>" << LINE_END;

    return EXIT_SUCCESS;
}
