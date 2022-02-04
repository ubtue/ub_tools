/** \brief Utility for safely backing up Sqlite databases.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "util.h"


namespace { } // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("sqlite_database sqlite_database_copy");

    const std::string original_database(argv[1]);
    const std::string copy_of_database(argv[2]);
    if (original_database == copy_of_database)
        LOG_ERROR("won't overwrite original database!");

    DbConnection db_connection(DbConnection::Sqlite3Factory(original_database, DbConnection::READONLY));
    db_connection.sqlite3BackupOrDie(copy_of_database);

    return EXIT_SUCCESS;
}
