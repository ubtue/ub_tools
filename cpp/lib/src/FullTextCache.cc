/** \file   FullTextCache.cc
 *  \brief  Implementation of functions relating to our full-text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FullTextCache.h"
#include "SqlUtil.h"
#include <ctime>
#include <kchashdb.h>
#include "Compiler.h"
#include "DbRow.h"
#include "Random.h"
#include "util.h"


namespace FullTextCache {


const unsigned MIN_CACHE_EXPIRE_TIME(84600 *  60); // About 2 months in seconds.
const unsigned MAX_CACHE_EXPIRE_TIME(84600 * 120); // About 4 months in seconds.


bool CacheExpired(DbConnection * const db_connection, const std::string &full_text_db_path, const std::string &id) {
    db_connection->queryOrDie("SELECT expiration FROM full_text_cache WHERE id=\"" + db_connection->escapeString(id)
                              + "\"");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return true;

    const DbRow row(result_set.getNextRow());
    const time_t expiration(SqlUtil::DatetimeToTimeT(row["expiration"]));
    const time_t now(std::time(nullptr));

    if (expiration < now)
        return false;

    kyotocabinet::HashDB db;
    if (not db.open(full_text_db_path, kyotocabinet::HashDB::OCREATE | kyotocabinet::HashDB::OWRITER))
        Error("in FullTextCache::CacheExpired: Failed to open database \"" + full_text_db_path + "\" for writing ("
              + std::string(db.error().message()) + ")!");
    db.remove(id);

    db_connection->queryOrDie("DELETE FROM full_text_cache WHERE id=\"" + db_connection->escapeString(id) + "\"");
    return true;
}


void InsertIntoCache(DbConnection * const db_connection, const std::string &full_text_db_path,
                     const std::string &key, const std::string &data)
{
    if (not data.empty()) {
        kyotocabinet::HashDB db;
        if (not db.open(full_text_db_path, kyotocabinet::HashDB::OCREATE | kyotocabinet::HashDB::OWRITER))
            Error("in FullTextCache::InsertIntoCache: Failed to open database \"" + full_text_db_path
                  + "\" for writing (" + std::string(db.error().message()) + ")!");
        if (unlikely(not db.set(key, data)))
            Error("in FullTextCache::InsertIntoCache: Failed to insert into database \"" + full_text_db_path + "\" ("
                  + std::string(db.error().message()) + ")!");
    }

    const time_t now(std::time(nullptr));
    Random::Rand rand(now);
    const time_t expiration(now + MIN_CACHE_EXPIRE_TIME + rand(MAX_CACHE_EXPIRE_TIME - MIN_CACHE_EXPIRE_TIME));
    db_connection->queryOrDie("REPLACE INTO full_text_cache SET id=\"" + db_connection->escapeString(key)
                              + "\",expiration=\"" + SqlUtil::TimeTToDatetime(expiration) + "\"");
}


} // namespace FullTextCache
