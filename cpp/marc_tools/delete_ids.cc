/** \brief Utility for deleting partial or entire MARC records based on an input list.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \documentation https://wiki.bsz-bw.de/doku.php?id=v-team:daten:datendienste:sekkor under "Löschungen".
 *
 *  \copyright 2015-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <vector>
#include <cstdlib>
#include "BSZUtil.h"
#include "DbConnection.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--input-format=(marc-21|marc-xml)] [--output-format=(marc-21|marc-xml)]"
            " deletion_list input_marc21 output_marc21 entire_record_deletion_log\n"
            "Record ID's of records that were deleted and not merely modified will be written to \"entire_record_deletion_log\".");
}


/** \brief Deletes LOK sections from the local data database if their pseudo tags are found in "local_deletion_ids"
 */
void DeleteLocalSections(DbConnection * const db_connection, const std::unordered_set <std::string> &local_deletion_ids,
                         const std::string &ppn)
{
    db_connection->queryOrDie("SELECT local_fields FROM local_data WHERE ppn = "
                              + db_connection->escapeAndQuoteString(ppn));
    auto result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return; // Nothing to be done!

    const auto row(result_set.getNextRow());
    const auto local_fields_blob(row["local_fields"]);

    size_t processed_size(0);
    bool keep_section;
    std::string new_local_fields_blob;
    do {
        // Convert the 4 character hex string to the size of the following field contents:
        const size_t field_contents_size(StringUtil::ToUnsignedLong(local_fields_blob.substr(processed_size, 4), 16));
        processed_size += 4;

        if (unlikely(processed_size + field_contents_size > local_fields_blob.size()))
            LOG_ERROR("Inconsitent blob length for record with PPN " + ppn + " (1)");

        const MARC::Record::Field local_field(local_fields_blob.substr(processed_size, field_contents_size));
        const auto pseudo_tag_and_data(local_field.getFirstSubfieldWithCode('0'));
        if (StringUtil::StartsWith(pseudo_tag_and_data, "001 "))
            keep_section = local_deletion_ids.find(pseudo_tag_and_data.substr(4)) != local_deletion_ids.cend();

        if (keep_section)
            new_local_fields_blob += local_fields_blob.substr(processed_size - 4, 4 + field_contents_size);

        processed_size += field_contents_size;
    } while (processed_size < local_fields_blob.size());

    if (processed_size > local_fields_blob.size())
        LOG_ERROR("Inconsitent blob length for record with PPN " + ppn + " (2)");

    if (new_local_fields_blob.size() < local_fields_blob.size()) {
        if (new_local_fields_blob.empty())
            db_connection->queryOrDie("DELETE FROM local_data WHERE ppn = "
                                      + db_connection->escapeAndQuoteString(ppn));
        else
            db_connection->queryOrDie("REPLACE INTO local_data (ppn, local_fields) VALUES("
                                      + db_connection->escapeAndQuoteString(ppn) + ","
                                      + db_connection->sqliteEscapeBlobData(new_local_fields_blob) + ")");
    }
}


void ProcessRecords(DbConnection * const db_connection, const std::unordered_set <std::string> &title_deletion_ids,
                    const std::unordered_set <std::string> &local_deletion_ids, MARC::Reader * const marc_reader,
                    MARC::Writer * const marc_writer, File * const entire_record_deletion_log)
{
    unsigned total_record_count(0), deleted_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        if (title_deletion_ids.find(record.getControlNumber()) != title_deletion_ids.end()) {
            ++deleted_record_count;
            (*entire_record_deletion_log) << record.getControlNumber() << '\n';
        } else
            marc_writer->write(record);

        if (not local_deletion_ids.empty())
            DeleteLocalSections(db_connection, local_deletion_ids, record.getControlNumber());
    }

    LOG_INFO("Read " + std::to_string(total_record_count) + " records.");
    LOG_INFO("Deleted " + std::to_string(deleted_record_count) + " records.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 5)
        Usage();

    const auto reader_type(MARC::GetOptionalReaderType(&argc, &argv, 1));
    const auto writer_type(MARC::GetOptionalWriterType(&argc, &argv, 1));

    if (argc != 5)
        Usage();

    const auto deletion_list(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unordered_set<std::string> title_deletion_ids, local_deletion_ids;
    BSZUtil::ExtractDeletionIds(deletion_list.get(), &title_deletion_ids, &local_deletion_ids);

    DbConnection db_connection(UBTools::GetTuelibPath() + "local_data.sq3", DbConnection::READWRITE);

    const auto marc_reader(MARC::Reader::Factory(argv[2], reader_type));
    const auto marc_writer(MARC::Writer::Factory(argv[3], writer_type));
    const auto entire_record_deletion_log(FileUtil::OpenForAppendingOrDie(argv[4]));

    ProcessRecords(&db_connection,title_deletion_ids, local_deletion_ids, marc_reader.get(), marc_writer.get(), entire_record_deletion_log.get());

    return EXIT_SUCCESS;
}
