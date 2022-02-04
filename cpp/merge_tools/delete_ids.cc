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
#include "FileUtil.h"
#include "LocalDataDB.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--input-format=(marc-21|marc-xml)] [--output-format=(marc-21|marc-xml)]"
        " deletion_list input_marc21 output_marc21 entire_record_deletion_log\n"
        "Record ID's of records that were deleted and not merely modified will be written to \"entire_record_deletion_log\".");
}


void ProcessRecords(LocalDataDB * const local_data_db, const std::unordered_set<std::string> &title_deletion_ids,
                    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer, File * const entire_record_deletion_log) {
    unsigned total_record_count(0), deleted_record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ++total_record_count;

        const auto ppn(record.getControlNumber());
        if (title_deletion_ids.find(ppn) != title_deletion_ids.end()) {
            local_data_db->removeTitleDataSet(ppn);
            ++deleted_record_count;
            (*entire_record_deletion_log) << ppn << '\n';
        } else
            marc_writer->write(record);
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

    LocalDataDB local_data_db(LocalDataDB::READ_WRITE);
    for (const auto &local_deletion_id : local_deletion_ids)
        local_data_db.removeLocalDataSet(local_deletion_id);

    const auto marc_reader(MARC::Reader::Factory(argv[2], reader_type));
    const auto marc_writer(MARC::Writer::Factory(argv[3], writer_type));
    const auto entire_record_deletion_log(FileUtil::OpenForAppendingOrDie(argv[4]));

    ProcessRecords(&local_data_db, title_deletion_ids, marc_reader.get(), marc_writer.get(), entire_record_deletion_log.get());

    return EXIT_SUCCESS;
}
