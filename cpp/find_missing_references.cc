/** \brief  Utility for finding referenced PPN's that we should have, but that are missing.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include <cstdlib>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "Archive.h"
#include "DbConnection.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "MARC.h"
#include "UBTools.h"
#include "util.h"


namespace {


DbConnection OpenOrCreateDatabase() {
    const std::string DATABASE_PATH(UBTools::GetTuelibPath() + "previously_reported_missing_ppns.sq3");
    if (FileUtil::Exists(DATABASE_PATH))
        return DbConnection::Sqlite3Factory(DATABASE_PATH, DbConnection::READWRITE);

    DbConnection db_connection(DbConnection::Sqlite3Factory(DATABASE_PATH, DbConnection::CREATE));
    db_connection.queryOrDie("CREATE TABLE missing_references (ppn TEXT PRIMARY KEY) WITHOUT ROWID");
    return db_connection;
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input email_address");

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Writer> marc_writer(nullptr);
    const auto missing_references_log(FileUtil::OpenOutputFileOrDie(argv[2]));

    const std::string email_address(argv[2]);
    if (not EmailSender::IsValidEmailAddress(email_address))
        LOG_ERROR("\"" + email_address + "\" is not a valid email address!");

    DbConnection db_connection(OpenOrCreateDatabase());

    std::unordered_set<std::string> all_ppns;
    while (const auto record = marc_reader->read())
        all_ppns.emplace(record.getControlNumber());
    marc_reader->rewind();

    std::unordered_map<std::string, std::set<std::string>> missing_ppns_to_referers_map;
    while (const auto record = marc_reader->read()) {
        for (const auto &_787_field : record.getTagRange("787")) {
            if (not StringUtil::StartsWith(_787_field.getFirstSubfieldWithCode('i'), "Rezension"))
                continue;

            for (const auto &subfield : _787_field.getSubfields()) {
                if (subfield.code_ == 'w' and StringUtil::StartsWith(subfield.value_, "(DE-627)")) {
                    const auto referenced_ppn(subfield.value_.substr(__builtin_strlen("(DE-627)")));
                    if (all_ppns.find(referenced_ppn) == all_ppns.end()) {
                        auto missing_ppn_and_referers(missing_ppns_to_referers_map.find(referenced_ppn));
                        if (missing_ppn_and_referers != missing_ppns_to_referers_map.end())
                            missing_ppn_and_referers->second.emplace(record.getControlNumber());
                        else
                            missing_ppns_to_referers_map.emplace(referenced_ppn, std::set<std::string>{ record.getControlNumber() });
                    }
                    break;
                }
            }
        }
    }

    std::unordered_set<std::string> new_missing_ppns;
    std::string missing_references_text;
    for (const auto &[missing_ppn, referers] : missing_ppns_to_referers_map) {
        db_connection.queryOrDie("SELECT ppn FROM missing_references WHERE ppn='" + missing_ppn + "'");
        const DbResultSet result_set(db_connection.getLastResultSet());
        if (result_set.empty()) {
            new_missing_ppns.emplace(missing_ppn);
            missing_references_text += missing_ppn + " <- " + StringUtil::Join(referers, ", ") + "\n";
        }
    }

    LOG_INFO("Found " + std::to_string(new_missing_ppns.size()) + " new missing reference(s).");

    if (not new_missing_ppns.empty()) {
        const std::string ZIP_FILENAME("/tmp/missing_ppns.zip");
        ::unlink(ZIP_FILENAME.c_str());
        Archive::Writer archive_writer(ZIP_FILENAME, Archive::Writer::FileType::ZIP);
        archive_writer.addEntry("missing_ppns", missing_references_text.size());
        archive_writer.write(missing_references_text);
        archive_writer.close();

        const auto status_code(EmailSender::SendEmailWithFileAttachments(
            "nobody@nowhere.com", { email_address }, "Missing PPN's",
            "Attached is the new list of " + std::to_string(new_missing_ppns.size()) + " missing PPN('s).",
            { ZIP_FILENAME }));
        if (status_code > 299)
            LOG_ERROR("Failed to send an email to \"" + email_address + "\"!  The server returned "
                      + std::to_string(status_code) + ".");

        const unsigned BATCH_SIZE(20);
        std::string values;
        unsigned count(0);
        for (const auto &new_missing_ppn : new_missing_ppns) {
            values += "('" + new_missing_ppn + "'),";
            ++count;
            if (count == BATCH_SIZE) {
                values.resize(values.size() - 1); // Strip off the last comma.
                db_connection.queryOrDie("INSERT INTO missing_references (ppn) VALUES " + values);
                count = 0, values.clear();
            }
        }
        if (not values.empty())
            db_connection.queryOrDie("INSERT INTO missing_references (ppn) VALUES " + values);
    }

    return EXIT_SUCCESS;
}
