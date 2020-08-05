/** \file    apply_differential_update.cc
 *  \brief   A tool for applying a differential update to a complete MARC dump.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018-2020 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <map>
#include <stdexcept>
#include <unordered_set>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include "Archive.h"
#include "BSZUtil.h"
#include "Compiler.h"
#include "FileUtil.h"
#include "LocalDataDB.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--min-log-level=log_level] [--keep-intermediate-files] input_directory "
            "difference_archive output_directory\n"
            "       Log levels are DEBUG, INFO, WARNING and ERROR with INFO being the default.\n");
}


void CopyAndCollectPPNs(LocalDataDB * const local_data_db, MARC::Reader * const reader, MARC::Writer * const writer,
                        std::unordered_set<std::string> * const previously_seen_ppns)
{
    while (auto record = reader->read()) {
        const auto PPN(record.getControlNumber());
        if (previously_seen_ppns->find(PPN) == previously_seen_ppns->end()) {
            const auto first_local_field(record.findTag("LOK"));
            if (unlikely(first_local_field != record.end())) {
                std::vector<std::string> local_fields;
                for (auto local_field(first_local_field); local_field != record.end(); ++local_field)
                    local_fields.emplace_back(local_field->getContents());
                local_data_db->insertOrReplace(PPN, local_fields);

                record.truncate(first_local_field);
            }

            previously_seen_ppns->emplace(PPN);
            if (record.getFirstField("ORI") == record.end())
                record.insertField("ORI", 'a', FileUtil::GetLastPathComponent(reader->getPath()));
            writer->write(record);
        }
    }
}


void CopySelectedTypes(LocalDataDB * const local_data_db, const std::vector<std::string> &archive_members, MARC::Writer * const writer,
                       const std::set<BSZUtil::ArchiveType> &selected_types, std::unordered_set<std::string> * const previously_seen_ppns)
{
    for (const auto &archive_member : archive_members) {
        if (selected_types.find(BSZUtil::GetArchiveType(archive_member)) != selected_types.cend()) {
            const auto reader(MARC::Reader::Factory(archive_member, MARC::FileType::BINARY));
            CopyAndCollectPPNs(local_data_db, reader.get(), writer, previously_seen_ppns);
        }
    }
}


void PatchArchiveMembersAndCreateOutputArchive(LocalDataDB * const local_data_db, const std::vector<std::string> &input_archive_members,
                                               const std::vector<std::string> &difference_archive_members, const std::string &output_directory)
{
    if (input_archive_members.empty())
        LOG_ERROR("no input archive members!");
    if (difference_archive_members.empty())
        LOG_WARNING("no difference archive members!");

    //
    // We process title data first and combine all inferior and superior records.
    //

    const auto title_writer(MARC::Writer::Factory(output_directory + "/tit.mrc", MARC::FileType::BINARY));
    std::unordered_set<std::string> previously_seen_title_ppns;
    CopySelectedTypes(local_data_db, difference_archive_members, title_writer.get(), { BSZUtil::TITLE_RECORDS, BSZUtil::SUPERIOR_TITLES },
                      &previously_seen_title_ppns);
    CopySelectedTypes(local_data_db, input_archive_members, title_writer.get(), { BSZUtil::TITLE_RECORDS, BSZUtil::SUPERIOR_TITLES },
                      &previously_seen_title_ppns);

    const auto authority_writer(MARC::Writer::Factory(output_directory + "/aut.mrc", MARC::FileType::BINARY));
    std::unordered_set<std::string> previously_seen_authority_ppns;
    CopySelectedTypes(local_data_db, difference_archive_members, authority_writer.get(), { BSZUtil::AUTHORITY_RECORDS },
                      &previously_seen_authority_ppns);
    CopySelectedTypes(local_data_db, input_archive_members, authority_writer.get(), { BSZUtil::AUTHORITY_RECORDS },
                      &previously_seen_authority_ppns);
}


void GetDirectoryContentsWithRelativepath(const std::string &archive_name, std::vector<std::string> * const archive_members) {
    const std::string directory_name(archive_name);
    FileUtil::GetFileNameList(".(raw|mrc)$", archive_members, directory_name);
    for (auto &archive_member : *archive_members)
        archive_member = directory_name + "/" + archive_member;
}


std::string RemoveSuffix(const std::string &s, const std::string &suffix) {
    if (unlikely(not StringUtil::EndsWith(s, suffix)))
        LOG_ERROR("\"" + s + "\" does not end w/ \"" + suffix + "\"!");
    return s.substr(0, s.length() - suffix.length());
}


inline std::string StripTarGz(const std::string &archive_filename) {
    return RemoveSuffix(archive_filename, ".tar.gz");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    bool keep_intermediate_files(false);
    if (std::strcmp(argv[1], "--keep-intermediate-files") == 0) {
        keep_intermediate_files = true;
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    const std::string input_directory(FileUtil::MakeAbsolutePath(argv[1]));
    const std::string difference_archive(FileUtil::MakeAbsolutePath(argv[2]));
    const std::string output_directory(FileUtil::MakeAbsolutePath(argv[3]));

    if (input_directory == difference_archive or input_directory == output_directory or difference_archive == output_directory)
        LOG_ERROR("all archive names must be distinct!");

    std::unique_ptr<FileUtil::AutoTempDirectory> working_directory;
    const std::string difference_directory(StripTarGz(difference_archive));
    Archive::UnpackArchive(difference_archive, difference_directory);
    if (not FileUtil::MakeDirectory(output_directory))
        LOG_ERROR("failed to create directory: \"" + output_directory + "\"!");

    std::vector<std::string> input_archive_members, difference_archive_members;
    GetDirectoryContentsWithRelativepath(input_directory, &input_archive_members);
    GetDirectoryContentsWithRelativepath(StripTarGz(difference_archive), &difference_archive_members);

    LocalDataDB local_data_db(LocalDataDB::READ_WRITE);
    PatchArchiveMembersAndCreateOutputArchive(&local_data_db, input_archive_members, difference_archive_members, output_directory);

    if (not keep_intermediate_files and not FileUtil::RemoveDirectory(difference_directory))
        LOG_ERROR("failed to remove directory: \"" + difference_directory + "\"!");

    return EXIT_SUCCESS;
}
