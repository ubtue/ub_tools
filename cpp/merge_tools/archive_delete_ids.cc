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
#include "Compiler.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--keep-intermediate-files] old_directory deletion_list new_directory entire_record_deletion_log\n"
        "Record ID's of records that were deleted and not merely modified will be written to \"entire_record_deletion_log\".");
}


const std::string DELETE_IDS_COMMAND("/usr/local/bin/delete_ids");


void UpdateSubdirectory(const std::string &old_directory, const std::string &deletion_list, const std::string &new_directory,
                        const std::string &entire_record_deletion_log) {
    std::vector<std::string> archive_members;
    FileUtil::GetFileNameList("\\.(mrc|raw)$", &archive_members, old_directory);

    if (not FileUtil::MakeDirectory(new_directory))
        LOG_ERROR("failed to create subdirectory: \"" + new_directory + "\"!");

    for (const auto &archive_member : archive_members) {
        if (unlikely(ExecUtil::Exec(DELETE_IDS_COMMAND, { "--input-format=marc-21", "--output-format=marc-21", deletion_list,
                                                          old_directory + "/" + archive_member, new_directory + "/" + archive_member,
                                                          entire_record_deletion_log })
                     != 0))
            LOG_ERROR("\"" + DELETE_IDS_COMMAND + "\" failed!");
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 5)
        Usage();

    bool keep_intermediate_files(false);
    if (std::strcmp(argv[1], "--keep-intermediate-files") == 0) {
        keep_intermediate_files = true;
        --argc, ++argv;
    }

    if (argc != 5)
        Usage();

    const std::string old_directory(FileUtil::MakeAbsolutePath(argv[1]));
    const std::string deletion_list(FileUtil::MakeAbsolutePath(argv[2]));
    const std::string new_directory(FileUtil::MakeAbsolutePath(argv[3]));

    if (old_directory == deletion_list or old_directory == new_directory or new_directory == deletion_list)
        LOG_ERROR("all filename parameters must be distinct!");

    FileUtil::AutoTempDirectory working_directory(FileUtil::GetLastPathComponent(::progname) + "-working-dir-" + std::to_string(::getpid()),
                                                  /* cleanup_if_exception_is_active = */ false,
                                                  /* remove_when_out_of_scope = */ not keep_intermediate_files);
    FileUtil::ChangeDirectoryOrDie(working_directory.getDirectoryPath());

    UpdateSubdirectory(old_directory, deletion_list, new_directory, argv[4]);

    FileUtil::ChangeDirectoryOrDie("..");

    return EXIT_SUCCESS;
}
