/** \file   LocalDataDB.cc
 *  \brief  Implementation of the LocalDataDB class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include "LocalDataDB.h"
#include "Compiler.h"
#include "DbConnection.h"
#include "StringUtil.h"
#include "UBTools.h"


LocalDataDB::LocalDataDB(const OpenMode open_mode) {
    db_connection_ = new DbConnection(UBTools::GetTuelibPath() + "local_data.sq3",
                                      (open_mode == READ_WRITE) ? DbConnection::READWRITE : DbConnection::READONLY);
    if (open_mode == READ_ONLY)
        return;

    db_connection_->queryOrDie("CREATE TABLE IF NOT EXISTS local_data ("
                               "    title_ppn TEXT PRIMARY KEY,"
                               "    local_fields BLOB NOT NULL"
                               ") WITHOUT ROWID");

    db_connection_->queryOrDie("CREATE TABLE IF NOT EXISTS local_ppns_to_title_ppns_map ("
                               "    local_ppn TEXT PRIMARY KEY,"
                               "    title_ppn TEXT NOT NULL"
                               ") WITHOUT ROWID");
}


LocalDataDB::~LocalDataDB() {
    delete db_connection_;
}


void LocalDataDB::clear() {
    db_connection_->queryOrDie("DELETE * FROM local_data");
    db_connection_->queryOrDie("DELETE * FROM local_ppns_to_title_ppns_map");
}


static std::vector<std::string> BlobToLocalFieldsVector(const std::string &local_fields_blob, const std::string &title_ppn) {
    std::vector<std::string> local_fields;

    size_t processed_size(0);
    do {
        // Convert the 4 character hex string to the size of the following field contents:
        const size_t field_contents_size(StringUtil::ToUnsignedLong(local_fields_blob.substr(processed_size, 4), 16));
        processed_size += 4;

        // Sanity check:
        if (unlikely(processed_size + field_contents_size > local_fields_blob.size()))
            LOG_ERROR("inconsistent blob length for record with PPN " + title_ppn + " (1)");

        local_fields.emplace_back(local_fields_blob.substr(processed_size, field_contents_size));
        processed_size += field_contents_size;
    } while (processed_size < local_fields_blob.size());

    // Sanity check:
    if (unlikely(processed_size != local_fields_blob.size()))
        LOG_ERROR("inconsistent blob length for record with PPN " + title_ppn + " (2))");

    return local_fields;
}


std::vector<std::string> ExtractLocalPPNsFromLocalFieldsVector(const std::vector<std::string> &local_fields) {
    std::vector<std::string> local_ppns;

    for (const auto &local_field : local_fields) {
        if (StringUtil::StartsWith(local_field, "001 "))
            local_ppns.emplace_back(local_field.substr(__builtin_strlen("001 ")));
    }

    return local_ppns;
}


static std::string ConvertLocalFieldsVectorToBlob(const std::vector<std::string> &local_fields) {
    std::string local_fields_blob;
    for (const auto &local_field : local_fields) {
        local_fields_blob += StringUtil::ToString(local_field.length(), /* radix = */16,
                                                  /* width = */4, /* padding_char = */'0');
        local_fields_blob += local_field;
    }

    return local_fields_blob;
}


void LocalDataDB::insertOrReplace(const std::string &title_ppn, const std::vector<std::string> &local_fields) {
    // 1. Clear out any local PPNs associated with "title_ppn":
    db_connection_->queryOrDie("SELECT local_fields FROM local_data WHERE title_ppn = "
                               + db_connection_->escapeAndQuoteString(title_ppn));
    auto result_set(db_connection_->getLastResultSet());
    if (not result_set.empty()) {
        const auto row(result_set.getNextRow());
        const auto previous_local_fields(BlobToLocalFieldsVector(row["local_fields"], title_ppn));
        const auto local_ppns(ExtractLocalPPNsFromLocalFieldsVector(previous_local_fields));
        for (const auto &local_ppn : local_ppns)
            db_connection_->queryOrDie("DELETE * FROM local_ppns_to_title_ppns_map WHERE local_ppn="
                                       + db_connection_->escapeAndQuoteString(local_ppn));
    }

    // 2. Replace or insert the local data keyed by the title PPN's:
    db_connection_->queryOrDie("REPLACE INTO local_data (title_ppn, local_fields) VALUES("
                               + db_connection_->escapeAndQuoteString(title_ppn) + ","
                               + db_connection_->sqliteEscapeBlobData(ConvertLocalFieldsVectorToBlob(local_fields)) + ")");

    // 3. Insert the mappings from the local PPN's to the title PPN's:
    for (const auto &local_ppn : ExtractLocalPPNsFromLocalFieldsVector(local_fields)) {
        db_connection_->queryOrDie("INSERT INTO local_ppns_to_title_ppns_map (local_ppn, title_ppn) VALUES("
                                   + db_connection_->escapeAndQuoteString(local_ppn) + ","
                                   + db_connection_->escapeAndQuoteString(title_ppn) + ")");
    }
}


std::vector<std::string> LocalDataDB::getLocalFields(const std::string &title_ppn) {
    db_connection_->queryOrDie("SELECT local_fields FROM local_data WHERE title_ppn = "
                               + db_connection_->escapeAndQuoteString(title_ppn));
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return {}; // empty vector

    const auto row(result_set.getNextRow());
    return BlobToLocalFieldsVector(row["local_fields"], title_ppn);
}


// Removes all fields from "local_fields" that are associated w/ "local_ppn".
static std::vector<std::string> RemoveLocalDataSet(const std::string &local_ppn,
                                                   const std::vector<std::string> &local_fields)
{
    std::vector<std::string> filtered_local_fields;
    filtered_local_fields.reserve(local_fields.size());

    bool skipping(false);
    for (const auto &local_field : local_fields) {
        if (StringUtil::StartsWith(local_field, "001 "))
            skipping = local_field.substr(__builtin_strlen("001 ")) == local_ppn;
        if (not skipping)
            filtered_local_fields.emplace_back(local_field);
    }

    return filtered_local_fields;
}


bool LocalDataDB::removeLocalDataSet(const std::string &local_ppn) {
    // 1. Determine the title PPN associated w/ the local PPN:
    db_connection_->queryOrDie("SELECT title_ppn FROM local_ppns_to_title_ppns_map WHERE local_ppn="
                               + db_connection_->escapeAndQuoteString(local_ppn));
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;
    const auto row(result_set.getNextRow());
    const auto title_ppn(row["title_ppn"]);

    // 2. Retrieve the local data associated w/ the title PPN:
    auto local_fields(getLocalFields(title_ppn));

    // 3. Remove the local data associsted w/ the local PPN:
    const auto filtered_local_fields(RemoveLocalDataSet(local_ppn, local_fields));

    // 4. Update our SQL tables:
    db_connection_->queryOrDie("DELETE FROM local_ppns_to_title_ppns_map WHERE ocal_ppn="
                               + db_connection_->escapeAndQuoteString(local_ppn));
    if (filtered_local_fields.empty())
        db_connection_->queryOrDie("DELETE * FROM local_fields WHERE title_ppn="
                                   + db_connection_->escapeAndQuoteString(title_ppn));
    else
        db_connection_->queryOrDie("REPLACE INTO local_data (titel_ppn, local_fields) VALUES("
                                   + db_connection_->escapeAndQuoteString(title_ppn) + ","
                                   + db_connection_->sqliteEscapeBlobData(ConvertLocalFieldsVectorToBlob(local_fields)) + ")");

    return true;
}
