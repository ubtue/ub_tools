/** \brief Utility for updating hashes and URLs of MARC records in our delivery history database.
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
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
#include <iostream>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "DbConnection.h"
#include "GzStream.h"
#include "MARC.h"
#include "util.h"
#include "Zeder.h"
#include "ZoteroHarvesterConversion.h"
#include "ZoteroHarvesterUtil.h"
#include "ZoteroHarvesterZederInterop.h"


namespace {


MARC::Record ReconstructRecord(const std::string &compressed_record_blob) {
    const auto decompressed_blob(GzStream::DecompressString(compressed_record_blob, GzStream::GUNZIP));
    return MARC::Record(decompressed_blob.length(), decompressed_blob.data());
}


bool UpdateRecordHash(const std::string &record_id, const std::string &saved_hash, MARC::Record * const record,
                      DbConnection * const db_connection)
{
    const auto recalculated_hash(ZoteroHarvester::Conversion::CalculateMarcRecordHash(*record));
    if (saved_hash == recalculated_hash) {
        LOG_DEBUG("record " + record_id + " has the same hash. skipping...");
       return false;
    }

    // Update the hash in the control field
    auto control_field(record->findTag("001"));
    const auto control_field_value(control_field->getContents());
    const auto control_number_prefix(control_field_value.substr(0, control_field_value.rfind('#')));
    control_field->setContents(control_number_prefix + "#" + recalculated_hash);

    const auto updated_blob(GzStream::CompressString(record->toBinaryString(), GzStream::GZIP));
    db_connection->queryOrDie("UPDATE delivered_marc_records SET record=" + db_connection->escapeAndQuoteString(updated_blob)
                              + ", hash=" + db_connection->escapeAndQuoteString(recalculated_hash)
                              + " WHERE id=" + record_id);
    return true;
}


void SaveRecordUrls(const std::string &record_id, const MARC::Record &record, DbConnection * const db_connection) {
    const auto urls(ZoteroHarvester::Util::GetMarcRecordUrls(record));
    for (const auto &url : urls) {
        // This call will fail at least once for each record that has multiple URLs due to duplicates.
        // Failures of this kind are benign.
        db_connection->query("INSERT INTO delivered_marc_records_urls SET record_id=" + record_id
                             + ", url="
                             + db_connection->escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxIndexLength(url)));
    }
}


} // unnamed namespace


int Main(int /* argc */, char ** /* argv */) {
    DbConnection db_connection(DbConnection::UBToolsFactory());

    db_connection.queryOrDie("SELECT id, hash, record FROM delivered_marc_records");
    auto result_set(db_connection.getLastResultSet());
    unsigned read_records(0), updated_record_hashes(0);

    while (const DbRow row = result_set.getNextRow()) {
        const auto record_id(row["id"]);
        const auto compressed_record_blob(row["record"]);
        const auto saved_hash(row["hash"]);

        auto record(ReconstructRecord(compressed_record_blob));
        if (UpdateRecordHash(record_id, saved_hash, &record, &db_connection))
            ++updated_record_hashes;

        SaveRecordUrls(record_id, record, &db_connection);

        ++read_records;
    }

    LOG_INFO("Read " + std::to_string(read_records) + " MARC record(s).");
    LOG_INFO("Updated " + std::to_string(updated_record_hashes) + " MARC record(s) with new hashes.");

    return EXIT_SUCCESS;
}
