/** \brief Tool to export all datasets from VuFind redirect table to CSV file.
 *  \author Mario Trojan
 *
 *  \copyright 2020-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "DbConnection.h"
#include "FileUtil.h"
#include "TextUtil.h"
#include "util.h"


int Main(int argc, char **argv) {
    if (argc != 2)
        ::Usage("export_file");

    auto db_connection(DbConnection::VuFindMySQLFactory());
    db_connection.queryOrDie("SELECT * FROM tuefind_redirect");
    auto result_set(db_connection.getLastResultSet());

    const auto csv_file(FileUtil::OpenOutputFileOrDie(argv[1]));
    csv_file->write("url;group;timestamp\n");
    while (const auto db_row = result_set.getNextRow()) {
        csv_file->write(TextUtil::CSVEscape(db_row["url"]) + ";");
        csv_file->write(TextUtil::CSVEscape(db_row["group_name"]) + ";");
        csv_file->write(TextUtil::CSVEscape(db_row["timestamp"]) + "\n");
    }

    return EXIT_SUCCESS;
}
