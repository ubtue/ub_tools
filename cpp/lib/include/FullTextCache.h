/** \file   FullTextCache.h
 *  \brief  Anything relating to our full-text cache.
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
#ifndef FullTextCache_H
#define FullTextCache_H


#include <string>
#include "DbConnection.h"


namespace FullTextCache {


struct Entry {
    std::string id_;
    std::string url_;
    std::string expiration_;
    std::string data_;
    std::string status_;
    std::string error_message_;
};


bool GetEntry(DbConnection * const db_connection, const std::string &id, Entry &entry);


/** \brief Test whether an entry in the cache has expired or not.
 *  \return True if we find "id" in the database and the entry is older than now-CACHE_EXPIRE_TIME_DELTA or if "id"
 *          is not found in the database, else false.
 *  \note Deletes expired entries and associated data in the key/value database found at "full_text_db_path".
 */
bool CacheEntryExpired(DbConnection * const db_connection, const std::string &full_text_db_path,
                       const std::string &key);


/* \note If "data" is empty only an entry will be made in the SQL database but not in the key/value store.  Also
 *       either "data" must be non-empty or "error_message" must be non-empty.
 */
void InsertIntoCache(DbConnection * const db_connection, const std::string &full_text_db_path,
                     const std::string &id, const std::string &url, const std::string &data,
                     const std::string &error_message);


} // namespace FullTextCache


#endif // ifndef FullTextCache_H
