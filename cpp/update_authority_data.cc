/** \brief Utility for replacing generating up-to-date authority MARC collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "BSZUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " deletion_list reference_records source_records target_records\n"
              << "       Replaces all records in \"source_records\" that have an identical control number\n"
              << "       as a record in \"reference_records\" with the corresponding record in\n"
              << "       \"reference_records\".  The file with the replacements as well as any records\n"
              << "       that could not be replaced is the output file \"target_records\".\n\n";
    std::exit(EXIT_FAILURE);
}


// Copies records from "marc_reader" to "marc_writer", skipping those whose ID's are found in
// "delete_full_record_ids".
void EraseRecords(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                  const std::unordered_set <std::string> &delete_full_record_ids)
{
    std::cout << "Eliminating records listed in a deletion list...\n";
    
    unsigned total_record_count(0), deletion_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++total_record_count;

        if (delete_full_record_ids.find(record.getControlNumber()) == delete_full_record_ids.cend())
            ++deletion_count;
        else
            marc_writer->write(record);
    }

    std::cout << "Read " << total_record_count << " records and dropped " << deletion_count << " records.\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
        Usage();

    try {
        std::unique_ptr<File> deletion_list_file(FileUtil::OpenInputFileOrDie(argv[1]));
        std::unordered_set <std::string> delete_full_record_ids, local_deletion_ids;
        BSZUtil::ExtractDeletionIds(deletion_list_file.get(), &delete_full_record_ids, &local_deletion_ids);


        std::unique_ptr<MarcReader> marc_source_reader(MarcReader::Factory(argv[3]));
        const std::string MARC_TEMPFILE("/tmp/update_authority_data.temp.mrc");
        std::unique_ptr<MarcWriter> marc_temp_writer(MarcWriter::Factory(MARC_TEMPFILE));
        EraseRecords(marc_source_reader.get(), marc_temp_writer.get(), delete_full_record_ids);

        const std::string MARC_REFERENCE_FILE(argv[2]);
        const std::string MARC_TARGET_FILE(argv[4]);
        const std::string REPLACE_MARC_RECORDS_PATH("/usr/local/bin/replace_marc_records");
        if (ExecUtil::Exec(REPLACE_MARC_RECORDS_PATH, { MARC_REFERENCE_FILE, MARC_TEMPFILE, MARC_TARGET_FILE }) != 0)
            Error("failed to execute \"" + REPLACE_MARC_RECORDS_PATH + "\"!");
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
