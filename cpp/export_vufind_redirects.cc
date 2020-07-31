/** \brief Tool to export all datasets from VuFind redirect table to CSV file.
 *  \author Mario Trojan
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "FileUtil.h"
#include "TextUtil.h"
#include "VuFind.h"
#include "util.h"


int Main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << ::progname << " export_file\n";
        std::exit(EXIT_FAILURE);
    }
    
    const std::string export_file(argv[1]);
    
    auto db_connection(VuFind::GetDbConnection());
    db_connection->queryOrDie("SELECT * FROM tuefind_redirect");
    auto result_set(db_connection->getLastResultSet());
    
    std::string csv;
    csv += "url;group;timestamp\n";
    while (const auto &row = result_set.getNextRow()) {
        csv += TextUtil::CSVEscape(row["url"]) + ";";
        csv += TextUtil::CSVEscape(row["group_name"]) + ";";
        csv += TextUtil::CSVEscape(row["timestamp"]) + "\n";
    }
    
    FileUtil::WriteStringOrDie(export_file, csv);
    return EXIT_SUCCESS;
}
