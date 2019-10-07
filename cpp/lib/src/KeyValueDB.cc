/** \file    KeyValueDB.cc
 *  \brief   Implementation of class KeyValueDB.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2019 Library of the University of Tübingen
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
#include "KeyValueDB.h"
#include <db.h>
#include <strings.h>
#include "util.h"


KeyValueDB::KeyValueDB(const std::string &path){
    int errno;
    if ((errno = ::db_create(&db_, nullptr, 0)) != 0)
        LOG_ERROR("db_create() failed! (" + std::string(db_strerror(errno)) + ")");
    if ((errno  = db_->open(db_, /* txnid = */nullptr, path.c_str(), /* database = */nullptr, DB_HASH, /* flags = */0, /* mode = */0600))
        != 0)
        LOG_ERROR("DB->open()() failed! (" + std::string(db_strerror(errno)) + ")");
}


KeyValueDB::~KeyValueDB() {
    int errno;
    if ((errno = db_->close(db_, /* flags = */0)) != 0)
        LOG_ERROR("DB->close()() failed! (" + std::string(db_strerror(errno)) + ")");
    db_ = nullptr; // paranoia
}


size_t KeyValueDB::size() const {
    DB_HASH_STAT stats;
    int errno;
    if ((errno = db_->stat(db_, nullptr, reinterpret_cast<void *>(&stats), /* flags = */0)) != 0)
        LOG_ERROR("DB->stat() failed! (" + std::string(db_strerror(errno)) + ")");

    return stats.hash_ndata;
}


inline void InitDBTFromString(DBT * const dbt, const std::string &data) {
    ::bzero(reinterpret_cast<void *>(dbt), sizeof(DBT));
    dbt->data = (void *)data.data();
    dbt->size = data.size();
}


bool KeyValueDB::keyIsPresent(const std::string &key) {
    DBT dbt;
    InitDBTFromString(&dbt, key);
    return db_->exists(db_, nullptr, &dbt, /* flags = */0) != DB_NOTFOUND;
}


void KeyValueDB::addOrReplace(const std::string &key, const std::string &value) {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    DBT value_struct;
    InitDBTFromString(&value_struct, value);

    int errno;
    if ((errno = db_->put(db_, /* txnid = */nullptr, &key_struct, &value_struct, /* flags = */0)) != 0)
        LOG_ERROR("DB->put() failed! (" + std::string(db_strerror(errno)) + ")");
}


std::string KeyValueDB::getValue(const std::string &key) const {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    char buffer[4096];
    DBT data_struct;
    ::bzero(reinterpret_cast<void *>(&data_struct), sizeof(DBT));
    data_struct.data = (void *)buffer;
    data_struct.ulen = sizeof(buffer);
    data_struct.flags = DB_DBT_USERMEM;

    int errno;
    if ((errno = db_->get(db_, /* txnid = */nullptr, &key_struct, &data_struct, /* flags = */0)) != 0)
        LOG_ERROR("DB->get() failed! (" + std::string(db_strerror(errno)) + ")");
    if (data_struct.size == 0)
        LOG_ERROR("DB->get() failed! (key \"" + key + "\" not found!");

    return std::string(reinterpret_cast<char *>(data_struct.data), data_struct.size);
}


std::string KeyValueDB::getValue(const std::string &key, const std::string &default_value) const {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    char buffer[4096];
    DBT data_struct;
    ::bzero(reinterpret_cast<void *>(&data_struct), sizeof(DBT));
    data_struct.data = (void *)buffer;
    data_struct.ulen = sizeof(buffer);
    data_struct.flags = DB_DBT_USERMEM;

    int errno;
    if ((errno = db_->get(db_, /* txnid = */nullptr, &key_struct, &data_struct, /* flags = */0)) != 0)
        LOG_ERROR("DB->get() failed! (" + std::string(db_strerror(errno)) + ")");

    return (data_struct.size == 0) ? default_value : std::string(reinterpret_cast<char *>(data_struct.data), data_struct.size);
}


void KeyValueDB::remove(const std::string &key) {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    int errno;
    if ((errno = db_->del(db_, /* txnid = */nullptr, &key_struct, /* flags = */0)) != 0)
        LOG_ERROR("DB->err() failed! (" + std::string(db_strerror(errno)) + ")");
}


void KeyValueDB::Create(const std::string &path) {
    DB *db;
    int errno;
    if ((errno = ::db_create(&db, nullptr, 0)) != 0)
        LOG_ERROR("db_create() failed! (" + std::string(db_strerror(errno)) + ")");

    if ((errno = db->open(db, /* txnid = */nullptr, path.c_str(), /* database = */nullptr, DB_HASH, DB_CREATE, /* mode = */0600)) != 0)
        LOG_ERROR("DB->open() failed! (" + std::string(db_strerror(errno)) + ")");
}
