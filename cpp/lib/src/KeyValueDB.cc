/** \file    KeyValueDB.cc
 *  \brief   Implementation of class KeyValueDB.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2019-2020 Library of the University of Tübingen
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
#include "FileUtil.h"
#include "util.h"


KeyValueDB::KeyValueDB(const std::string &path) {
    int retcode;
    if ((retcode = ::db_create(&db_, nullptr, 0)) != 0)
        LOG_ERROR("db_create on \"" + path + "\" failed! (" + std::string(db_strerror(retcode)) + ")");
    if ((retcode =
             db_->open(db_, /* txnid = */ nullptr, path.c_str(), /* database = */ nullptr, DB_HASH, /* flags = */ 0, /* mode = */ 0600))
        != 0)
        LOG_ERROR("DB->open on \"" + path + "\" failed! (" + std::string(db_strerror(retcode)) + ")");
}


KeyValueDB::~KeyValueDB() {
    int retcode;
    if ((retcode = db_->close(db_, /* flags = */ 0)) != 0)
        LOG_ERROR("DB->close()() failed! (" + std::string(db_strerror(retcode)) + ")");
    db_ = nullptr; // paranoia
}


size_t KeyValueDB::size() const {
    DB_HASH_STAT stats;
    int retcode;
    if ((retcode = db_->stat(db_, nullptr, reinterpret_cast<void *>(&stats), /* flags = */ 0)) != 0)
        LOG_ERROR("DB->stat() failed! (" + std::string(db_strerror(retcode)) + ")");

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
    return db_->exists(db_, nullptr, &dbt, /* flags = */ 0) != DB_NOTFOUND;
}


void KeyValueDB::addOrReplace(const std::string &key, const std::string &value) {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    DBT value_struct;
    InitDBTFromString(&value_struct, value);

    int retcode;
    if ((retcode = db_->put(db_, /* txnid = */ nullptr, &key_struct, &value_struct, /* flags = */ 0)) != 0)
        LOG_ERROR("DB->put() failed! (" + std::string(db_strerror(retcode)) + ")");
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

    int retcode;
    if ((retcode = db_->get(db_, /* txnid = */ nullptr, &key_struct, &data_struct, /* flags = */ 0)) != 0)
        LOG_ERROR("DB->get() failed! (" + std::string(db_strerror(retcode)) + ")");
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

    int retcode;
    if ((retcode = db_->get(db_, /* txnid = */ nullptr, &key_struct, &data_struct, /* flags = */ 0)) != 0)
        LOG_ERROR("DB->get() failed! (" + std::string(db_strerror(retcode)) + ")");

    return (data_struct.size == 0) ? default_value : std::string(reinterpret_cast<char *>(data_struct.data), data_struct.size);
}


void KeyValueDB::remove(const std::string &key) {
    DBT key_struct;
    InitDBTFromString(&key_struct, key);

    int retcode;
    if ((retcode = db_->del(db_, /* txnid = */ nullptr, &key_struct, /* flags = */ 0)) != 0)
        LOG_ERROR("DB->err() failed! (" + std::string(db_strerror(retcode)) + ")");
}


void KeyValueDB::Create(const std::string &path) {
    DB *db;
    int retcode;
    if ((retcode = ::db_create(&db, nullptr, 0)) != 0)
        LOG_ERROR("db_create() failed! (" + std::string(db_strerror(retcode)) + ")");

    if ((retcode = db->open(db, /* txnid = */ nullptr, path.c_str(), /* database = */ nullptr, DB_HASH, DB_CREATE, /* mode = */ 0600)) != 0)
        LOG_ERROR("DB->open() failed! (" + std::string(db_strerror(retcode)) + ")");
}


std::unique_ptr<KeyValueDB> OpenKeyValueDBOrDie(const std::string &db_path) {
    if (not FileUtil::Exists(db_path))
        LOG_ERROR(db_path + " does not exist!");

    return std::unique_ptr<KeyValueDB>(new KeyValueDB(db_path));
}
