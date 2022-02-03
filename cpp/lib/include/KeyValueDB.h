/** \file    KeyValueDB.h
 *  \brief   Declaration of class KeyValueDB.
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
#pragma once


#include <memory>
#include <string>
#include <db.h>


class KeyValueDB {
    struct __db *db_;

public:
    explicit KeyValueDB(const std::string &path);
    ~KeyValueDB();

    size_t size() const;
    bool keyIsPresent(const std::string &key);
    void addOrReplace(const std::string &key, const std::string &value);
    std::string getValue(const std::string &key) const;
    std::string getValue(const std::string &key, const std::string &default_value) const;
    void remove(const std::string &key);

    static void Create(const std::string &path);
};


std::unique_ptr<KeyValueDB> OpenKeyValueDBOrDie(const std::string &db_path);
