/** \brief Utility for dumping metadata out of the delivered_marc_records MySQL table in ub_tools.
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "DbConnection.h"
#include "FileUtil.h"
#include "GzStream.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--extended] zeder_journal_id csv_path\n"
            "\"--extended\" will add information from BLOBs as well, e.g. DOIs.");
}


MARC::Record GetTemporaryRecord(const std::string &blob) {
    const std::string decompressed_blob(GzStream::DecompressString(blob, GzStream::GUNZIP));
    const FileUtil::AutoTempFile tmp_file;
    FileUtil::WriteStringOrDie(tmp_file.getFilePath(), decompressed_blob);
    auto reader(MARC::Reader::Factory(tmp_file.getFilePath()));
    return reader->read();
}


void GetJournalDetailsFromDb(DbConnection * const db_connection, const std::string &zeder_journal_id, File * const csv_file) {
    db_connection->queryOrDie("SELECT * FROM zeder_journals WHERE id=" + db_connection->escapeAndQuoteString(zeder_journal_id));
    auto result(db_connection->getLastResultSet());
    if (result.size() != 1)
        LOG_ERROR("entry not found");

    while (DbRow journal = result.getNextRow()) {
        csv_file->writeln(journal["id"] + ";" + journal["zeder_id"] + ";" + journal["zeder_instance"] + ";" + TextUtil::CSVEscape(journal["journal_name"]));
        csv_file->writeln("");
        break;
    }
}


void GetJournalEntriesFromDb(DbConnection * const db_connection, const std::string &zeder_journal_id, File * const csv_file, const bool extended) {
    std::string query("SELECT * FROM delivered_marc_records WHERE zeder_journal_id="  + db_connection->escapeAndQuoteString(zeder_journal_id) + " ORDER BY delivered_at ASC");
    db_connection->queryOrDie(query);
    auto result(db_connection->getLastResultSet());
    while (DbRow row = result.getNextRow()) {
        std::string csv_row(row["id"] + ";" + row["hash"] + ";" + row["delivery_state"] + ";" + TextUtil::CSVEscape(row["error_message"]) + ";" + row["delivered_at"] + ";" + TextUtil::CSVEscape(row["main_title"]));
        if (extended and not row["record"].empty()) {
            const auto record(GetTemporaryRecord(row["record"]));

            const auto dois(record.getDOIs());
            csv_row += ";" + TextUtil::CSVEscape(StringUtil::Join(dois, '\n'));

            for (const auto &_936_field : record.getTagRange("936")) {
                csv_row += ";" + _936_field.getFirstSubfieldWithCode('j');
                csv_row += ";" + _936_field.getFirstSubfieldWithCode('d');
                csv_row += ";" + _936_field.getFirstSubfieldWithCode('e');
                break;
            }
        }

        csv_file->writeln(csv_row);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3 and argc != 4)
        Usage();

    bool extended(false);
    if (argc == 4 and StringUtil::StartsWith(argv[1], "--extended")) {
        extended = true;
        ++argv, --argc;
    }
    const std::string zeder_journal_id(argv[1]);
    const std::string csv_path(argv[2]);

    auto csv_file(FileUtil::OpenOutputFileOrDie(csv_path));

    DbConnection db_connection(DbConnection::UBToolsFactory());
    csv_file->writeln("zeder_journal_id;zeder_id;zeder_instance;journal_name");
    GetJournalDetailsFromDb(&db_connection, zeder_journal_id, csv_file.get());

    std::string entries_header("id;hash;delivery_state;error_message;delivered_at;main_title");
    if (extended)
        entries_header += ";DOIs;year;volume;issue";
    csv_file->writeln(entries_header);
    GetJournalEntriesFromDb(&db_connection, zeder_journal_id, csv_file.get(), extended);

    return EXIT_SUCCESS;
}
