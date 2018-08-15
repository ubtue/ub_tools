/** \brief Utility for deleting BSZ PPN's from BSZ-stype MARC archives.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "Archive.h"
#include "BSZUtil.h"
#include "Compiler.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--keep-intermediate-files] [--use-subdirectories] old_archive deletion_list new_archive\n";
    std::exit(EXIT_FAILURE);
}


const std::string DELETE_IDS_COMMAND("/usr/local/bin/delete_ids");


// Applies a deletion list to a single file.
void UpdateOneFile(const std::string &marc_filename, const std::string &deletion_list_file) {
    const std::string temp_output_filename(marc_filename + "-" + std::to_string(::getpid()));
    LOG_INFO("applying \"" + deletion_list_file + "\" to \"" + marc_filename + "\" to generate \"" + temp_output_filename + "\"!");

    if (unlikely(ExecUtil::Exec(DELETE_IDS_COMMAND,
                                { "--input-format=marc-21", "--output-format=marc-21", deletion_list_file, marc_filename,
                                  temp_output_filename }) != 0))
        LOG_ERROR("\"" + DELETE_IDS_COMMAND + "\" failed!");

    if (not FileUtil::RenameFile(temp_output_filename, marc_filename, /* remove_target = */true))
        LOG_ERROR("failed to rename \"" + temp_output_filename + "\" to \"" + marc_filename + "\"!");
}


std::string StripTarGz(const std::string &archive_filename) {
    if (unlikely(not StringUtil::EndsWith(archive_filename, ".tar.gz")))
        LOG_ERROR("\"" + archive_filename + "\" does not end w/ .tar.gz!");
    return archive_filename.substr(0, archive_filename.length() - 7);
}

    
void UpdateSubdirectory(const std::string &old_archive, const std::string &deletion_list, const std::string &new_archive) {
    const std::string old_directory(StripTarGz(old_archive));
    std::vector<std::string> archive_members;
    FileUtil::GetFileNameList(".raw$", &archive_members, old_directory);

    const std::string new_directory(StripTarGz(new_archive));
    if (not FileUtil::MakeDirectory(new_directory))
        LOG_ERROR("failed to create subdirectory: \"" + new_directory + "\"!");

    for (const auto &archive_member : archive_members) {
        if (unlikely(ExecUtil::Exec(DELETE_IDS_COMMAND,
                                    { "--input-format=marc-21", "--output-format=marc-21", deletion_list,
                                      old_directory + "/" + archive_member, new_directory + "/" + archive_member }) != 0))
            LOG_ERROR("\"" + DELETE_IDS_COMMAND + "\" failed!");
    }
}

    
void UpdateArchive(const std::string &old_archive, const std::string &deletion_list,
                   const std::string &new_archive)
{
    std::vector<std::string> archive_members;
    BSZUtil::ExtractArchiveMembers(old_archive, &archive_members);

    for (const auto &archive_member : archive_members)
        UpdateOneFile(archive_member, deletion_list);

    ArchiveWriter archive_writer(new_archive);
    for (const auto &archive_member : archive_members)
        archive_writer.add(archive_member);
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

    bool use_subdirectories(false);
    if (std::strcmp(argv[1], "--use-subdirectories") == 0) {
        use_subdirectories = true;
        --argc, ++argv;
    }

    if (argc != 4)
        Usage();

    const std::string old_archive(FileUtil::MakeAbsolutePath(argv[1]));
    const std::string deletion_list(FileUtil::MakeAbsolutePath(argv[2]));
    const std::string new_archive(FileUtil::MakeAbsolutePath(argv[3]));

    if (old_archive == deletion_list or old_archive == new_archive or new_archive == deletion_list)
        LOG_ERROR("all filename parameters must be distinct!");

    FileUtil::AutoTempDirectory working_directory(FileUtil::GetLastPathComponent(::progname) + "-working-dir-" + std::to_string(::getpid()),
                                                  /* cleanup_if_exception_is_active = */ false,
                                                  /* remove_when_out_of_scope = */ not keep_intermediate_files);
    FileUtil::ChangeDirectoryOrDie(working_directory.getDirectoryPath());

    if (use_subdirectories)
        UpdateSubdirectory(old_archive, deletion_list, new_archive);
    else
        UpdateArchive(old_archive, deletion_list, new_archive);

    FileUtil::ChangeDirectoryOrDie("..");

    return EXIT_SUCCESS;
}
