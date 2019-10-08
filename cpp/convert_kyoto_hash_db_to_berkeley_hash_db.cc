/** \file    convert_kyoto_hash_db_to_berkeley_hash_db.cc
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2019 Library of the University of Tübingen
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

#include <kchashdb.h>
#include "FileUtil.h"
#include "KeyValueDB.h"
#include "MediaTypeUtil.h"
#include "util.h"


int Main(int argc, char **argv) {
    if (argc != 3)
        ::Usage("kyoto_hash_db_filename berkeley_hash_db_filename");

    const std::string kyoto_hash_db_filename(argv[1]);
    const std::string berkeley_hash_db_filename(argv[2]);

    if (kyoto_hash_db_filename == berkeley_hash_db_filename)
        LOG_ERROR("source and target filenames are identical!");

    if (FileUtil::Exists(berkeley_hash_db_filename))
        LOG_ERROR("target filename exists!");

    if (MediaTypeUtil::GetFileMediaType(kyoto_hash_db_filename) != "application/kyotocabinet")
        LOG_ERROR("supposed kyotocabinet file is most likely not a kyotocabinet database!");


    kyotocabinet::HashDB kyoto_hash_db;
    if (not kyoto_hash_db.open(kyoto_hash_db_filename, kyotocabinet::HashDB::OREADER))
        LOG_ERROR("Failed to open  \"" + kyoto_hash_db_filename + "\" for reading (" + std::string(kyoto_hash_db.error().message()) + ")!");

    KeyValueDB::Create(berkeley_hash_db_filename);
    KeyValueDB berkeley_hash_db(berkeley_hash_db_filename);

    kyotocabinet::HashDB::Cursor *kyoto_hash_db_cursor(kyoto_hash_db.cursor());
    kyoto_hash_db_cursor->jump();
    unsigned count(0);
    std::string key, value;
    while (kyoto_hash_db_cursor->get(&key, &value, /* move the cursor to the next record = */true)) {
        berkeley_hash_db.addOrReplace(key, value);
        ++count;
    }
    delete kyoto_hash_db_cursor;

    LOG_INFO("Converted " + std::to_string(count) + " key/value pairs.");

    return EXIT_SUCCESS;
}
