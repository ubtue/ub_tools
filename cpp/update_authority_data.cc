/** \brief Utility for replacing generating up-to-date authority MARC collections.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <dirent.h>
#include "Archive.h"
#include "Compiler.h"
#include "BSZUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " deletion_list reference_records_archive source_records target_records\n"
              << "       Replaces all records in \"source_records\" that have an identical control number\n"
              << "       as a record in \"reference_records\" with the corresponding record in\n"
              << "       \"reference_records\".  The file with the replacements as well as any records\n"
              << "       that could not be replaced is the output file \"target_records\".\n"
              << "       \"deletion_list\", \"reference_records_archive\", and \"source_records\" must all be\n"
              << "       regular expressions containing \\d\\d\\d\\d\\d\\d stading in for YYMMDD.  Additionally\n"
              << "       \"target_records\" must also contain the YYMMDD patternNo  (No other metacharacters\n"
              << "       than \\d should probably be used.)\n\n";
    std::exit(EXIT_FAILURE);
}


/** \param path_regex  A PCRE regex that must contain a \d\d\d\d\d\d subexpression standing in for YYYYMMDD.
 *  \return Either the most recent file or the empty string if no files matched the regex.
 */
std::string GetMostRecentFile(const std::string &path_regex) {
    if (unlikely(path_regex.find("\\d\\d\\d\\d\\d\\d") == std::string::npos))
        Error("in GetMostRecentFile: regex \"" + path_regex + "\" does not contain \\d\\d\\d\\d\\d\\d!");

    std::string filename_regex, dirname;
    FileUtil::DirnameAndBasename(path_regex, &filename_regex, &dirname);

    std::string most_recent_file;
    FileUtil::Directory directory(dirname, filename_regex);
    for (const auto entry : directory) {
        const int entry_type(entry.getType());
        if (entry_type == DT_REG or entry_type == DT_UNKNOWN) {
            std::string filename(entry.getName());
            if (filename > most_recent_file)
                most_recent_file.swap(filename);
        }
    }

    return most_recent_file;
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


const std::string MARC_REFERENCE_FILENAME("sekkor-aut.mrc");


void ExtractAuthorityDataFromArchiveOrDie(const std::string &archive_filename) {
    ::unlink(MARC_REFERENCE_FILENAME.c_str());
    ArchiveReader archive_reader(archive_filename);
    if (not archive_reader.extractEntry(MARC_REFERENCE_FILENAME))
        Error("failed to extract \"" + MARC_REFERENCE_FILENAME + "\" from \"" + archive_filename + "\"!");
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
        Usage();

    try {
        const std::string MARC_TEMP_FILENAME("/tmp/update_authority_data.temp.mrc");
        ::unlink(MARC_TEMP_FILENAME.c_str());
        
        const std::string MARC_TARGET_FILENAME(argv[4]);
        const std::string MARC_TARGET_DATE(BSZUtil::ExtractDateFromFilenameOrDie(MARC_TARGET_FILENAME));

        const std::string MARC_SOURCE_FILENAME(GetMostRecentFile(argv[3]));
        const std::string MARC_SOURCE_DATE(BSZUtil::ExtractDateFromFilenameOrDie(MARC_SOURCE_FILENAME));

        if (MARC_TARGET_DATE >= MARC_SOURCE_DATE) {
            std::cout << "Nothing to be done!\n";
            return EXIT_SUCCESS;
        }
        
        const std::string DELETION_LIST_FILENAME(GetMostRecentFile(argv[1]));
        if (not DELETION_LIST_FILENAME.empty()) {
            const std::string DELETION_LIST_DATE(BSZUtil::ExtractDateFromFilenameOrDie(DELETION_LIST_FILENAME));

            if (DELETION_LIST_DATE >= MARC_SOURCE_DATE) {
                std::unique_ptr<File> deletion_list_file(FileUtil::OpenInputFileOrDie(DELETION_LIST_FILENAME));
                std::unordered_set <std::string> delete_full_record_ids, local_deletion_ids;
                BSZUtil::ExtractDeletionIds(deletion_list_file.get(), &delete_full_record_ids, &local_deletion_ids);

                std::unique_ptr<MarcReader> marc_source_reader(MarcReader::Factory(MARC_SOURCE_FILENAME));
                std::unique_ptr<MarcWriter> marc_temp_writer(MarcWriter::Factory(MARC_TEMP_FILENAME));
                EraseRecords(marc_source_reader.get(), marc_temp_writer.get(), delete_full_record_ids);
            }
        }

        // If we did not apply a deletion list we'll still need our temporary file for the next phase:
        if (not FileUtil::Exists(MARC_TEMP_FILENAME))
            FileUtil::CopyOrDie(MARC_SOURCE_FILENAME, MARC_TEMP_FILENAME);
        
        const std::string MARC_REFERENCE_ARCHIVE_FILENAME(GetMostRecentFile(argv[2]));
        if (MARC_REFERENCE_ARCHIVE_FILENAME.empty())
            FileUtil::CopyOrDie(MARC_TEMP_FILENAME, MARC_TARGET_FILENAME);
        else {
            const std::string MARC_REFERENCE_ARCHIVE_DATE(
                BSZUtil::ExtractDateFromFilenameOrDie(MARC_REFERENCE_ARCHIVE_FILENAME));
            if (MARC_REFERENCE_ARCHIVE_DATE >= MARC_SOURCE_DATE) {
                ExtractAuthorityDataFromArchiveOrDie(MARC_REFERENCE_ARCHIVE_FILENAME);
                const std::string REPLACE_MARC_RECORDS_PATH("/usr/local/bin/replace_marc_records");
                if (ExecUtil::Exec(REPLACE_MARC_RECORDS_PATH,
                                   { MARC_REFERENCE_FILENAME, MARC_TEMP_FILENAME, MARC_TARGET_FILENAME }) != 0)
                    Error("failed to execute \"" + REPLACE_MARC_RECORDS_PATH + "\"!");
            } else
                FileUtil::CopyOrDie(MARC_TEMP_FILENAME, MARC_TARGET_FILENAME);
        }
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
