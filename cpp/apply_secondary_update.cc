/** \file    apply_secondary_update.cc
 *  \brief   A tool for applying a secondary differential update to a complete MARC dump.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2018 Library of the University of Tübingen

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
#include "unistd.h"
#include "Archive.h"
#include "BSZUtil.h"
#include "Compiler.h"
#include "File.h"
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=log_level] [--keep-intermediate-files] [--use-subdirectories] input_archive "
              << "difference_archive output_archive\n"
              << "       Log levels are DEBUG, INFO, WARNING and ERROR with INFO being the default.\n\n";
    std::exit(EXIT_FAILURE);
}


// Compare according to type ('a', 'b', or 'c').
bool ArchiveMemberComparator(const std::string &member_name1, const std::string &member_name2) {
    return BSZUtil::GetTypeCharOrDie(member_name1) < BSZUtil::GetTypeCharOrDie(member_name2);
}


std::string RemoveSuffix(const std::string &s, const std::string &suffix) {
    if (unlikely(not StringUtil::EndsWith(s, suffix)))
        LOG_ERROR("\"" + s + "\" does not end w/ \"" + suffix + "\"!");
    return s.substr(0, s.length() - suffix.length());
}


inline std::string StripTarGz(const std::string &archive_filename) {
    return RemoveSuffix(archive_filename, ".tar.gz");
}


inline std::string RemovePIDSuffix(const std::string &s) {
    const std::string SUFFIX("-" + std::to_string(::getpid()));
    return RemoveSuffix(s, SUFFIX);
}

// Assumes that member_name ends in "-PID" and renames it to a new name w/o the "-PID".
inline void RemoveSuffixFromDifferentialArchiveMember(const std::string &member_name) {
    FileUtil::RenameFileOrDie(member_name, RemovePIDSuffix(member_name));
}


void CollectPPNs(const std::string &marc_filename, std::unordered_set<std::string> * const ppns) {
    const auto reader(MARC::Reader::Factory(marc_filename, MARC::FileType::BINARY));
    while (const auto record = reader->read())
        ppns->emplace(record.getControlNumber());
}


// Appends the local data from "source" to "target".
void CopyLocalData(const MARC::Record &source, MARC::Record * const target) {
    for (auto local_field(source.getFirstField("LOK")); local_field != source.cend(); ++local_field)
        target->appendField(*local_field);
}

    
// Patches "input_member" w/ "difference_member".  The result is the patched "input_member".
void PatchMember(const bool use_subdirectories, const std::string &input_member, const std::string &difference_member,
                 const std::string &output_archive, const std::unordered_map<std::string, off_t> &local_control_number_to_offset_map)
{
    

    if (not use_subdirectories) {
        ::unlink(input_member.c_str());
        FileUtil::RenameFileOrDie(output_filename, input_member);
    }
}


void PatchArchiveMembersAndCreateOutputArchive(const bool use_subdirectories, std::vector<std::string> input_archive_members,
                                               std::vector<std::string> difference_archive_members, const std::string &output_archive)
{
    if (input_archive_members.empty())
        LOG_ERROR("no input archive members!");
    if (difference_archive_members.empty())
        LOG_WARNING("no difference archive members!");
    std::set<std::string> unprocessed_input_members(input_archive_members.cbegin(), input_archive_members.cend());

    std::unordered_map<std::string, off_t> local_control_number_to_offset_map;
    for (const auto &secondary_archive : difference_archive_members) {
        if (BSZUtil::GetTypeCharOrDie(secondary_archive) != 'l')
            continue;

        const auto reader(MARC::Reader::Factory(secondary_archive));
        MARC::CollectRecordOffsets(reader.get(), &local_control_number_to_offset_map);
    }

    for (const auto &secondary_archive : difference_archive_members) {
        if (BSZUtil::GetTypeCharOrDie(secondary_archive) == 'l')
            continue;

        PatchMember(
    }

    for (const auto unprocessed_input_member : unprocessed_input_members)
        FileUtil::CopyOrDie(unprocessed_input_member, output_archive + "/" + FileUtil::GetLastPathComponent(unprocessed_input_member));
    
    if (use_subdirectories)
        return; // No need to create an archive file.

    //
    // Recreate archive
    //

    std::vector<std::string> output_archive_members;
    if (FileUtil::GetFileNameList(".*[abc]001.raw$", &output_archive_members) == 0)
        LOG_ERROR("missing output archive members!");

    std::string archive_write_options;
    if (StringUtil::EndsWith(output_archive, ".gz"))
        archive_write_options = "compression-level=1"; // lowest compression level => fastet
    else
        LOG_WARNING("output archive name \"" + output_archive + "\" does not end w/ \".gz\"!");
    Archive::Writer archive_writer(output_archive, archive_write_options);
    for (const auto &output_archive_member : output_archive_members)
        archive_writer.add(output_archive_member);
}


void GetDirectoryContentsWithRelativepath(const std::string &archive_name, std::vector<std::string> * const archive_members) {
    const std::string directory_name(StripTarGz(archive_name));
    FileUtil::GetFileNameList(".(raw|mrc)$", archive_members, directory_name);
    for (auto &archive_member : *archive_members)
        archive_member = directory_name + "/" + archive_member;
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

    const std::string input_archive(FileUtil::MakeAbsolutePath(argv[1]));
    const std::string difference_archive(FileUtil::MakeAbsolutePath(argv[2]));
    const std::string output_archive(FileUtil::MakeAbsolutePath(argv[3]));

    if (input_archive == difference_archive or input_archive == output_archive or difference_archive == output_archive)
        LOG_ERROR("all archive names must be distinct!");

    std::unique_ptr<FileUtil::AutoTempDirectory> working_directory;
    if (use_subdirectories) {
        Archive::UnpackArchive(difference_archive, StripTarGz(difference_archive));
        const auto directory_name(StripTarGz(output_archive));
        if (not FileUtil::MakeDirectory(directory_name))
            LOG_ERROR("failed to create directory: \"" + directory_name + "\"!");
    } else {
        working_directory.reset(new FileUtil::AutoTempDirectory(FileUtil::GetLastPathComponent(::progname) + "-working-dir",
                                                                /* cleanup_if_exception_is_active = */false,
                                                                /* remove_when_out_of_scope = */ not keep_intermediate_files));
        FileUtil::ChangeDirectoryOrDie(working_directory->getDirectoryPath());
    }

    std::vector<std::string> input_archive_members, difference_archive_members;
    if (use_subdirectories) {
        GetDirectoryContentsWithRelativepath(input_archive, &input_archive_members);
        GetDirectoryContentsWithRelativepath(difference_archive, &difference_archive_members);
    } else {
        BSZUtil::ExtractArchiveMembers(input_archive, &input_archive_members);
        BSZUtil::ExtractArchiveMembers(difference_archive, &difference_archive_members, "-" + std::to_string(::getpid()));
    }

    PatchArchiveMembersAndCreateOutputArchive(use_subdirectories, input_archive_members, difference_archive_members, output_archive);

    if (use_subdirectories) {
        if (not keep_intermediate_files and not FileUtil::RemoveDirectory(StripTarGz(difference_archive)))
            LOG_ERROR("failed to remove directory: \"" + StripTarGz(difference_archive) + "\"!");
    } else
        FileUtil::ChangeDirectoryOrDie("..");

    return EXIT_SUCCESS;
}
