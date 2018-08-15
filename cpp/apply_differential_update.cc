/** \file    apply_differential_update.cc
 *  \brief   A tool for applying a differential update to a complete MARC dump.
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


// Patches "input_member" w/ "difference_member".  The result is the patched "input_member".
void PatchMember(const bool use_subdirectories, const std::string &input_member, const std::string &difference_member,
                 const std::string &output_archive)
{
    LOG_DEBUG("Entering PatchMember: input_member=\"" + input_member + "\", difference_member=\"" + difference_member
              + ", and output_archive=\"" + output_archive + "\".");
    std::unordered_set<std::string> difference_ppns;
    CollectPPNs(difference_member, &difference_ppns);

    const auto input_reader(MARC::Reader::Factory(input_member, MARC::FileType::BINARY));
    const std::string output_filename(use_subdirectories ? StripTarGz(output_archive) + "/" + FileUtil::GetLastPathComponent(input_member)
                                                         : "patch-" + std::to_string(::getpid()));
    LOG_DEBUG("In PatchMember: output_filename=\"" + output_filename + "\".");
    const auto output_writer(MARC::Writer::Factory(output_filename, MARC::FileType::BINARY));

    // 1. Filter out the PPN's that are in "difference_member".
    while (const auto record = input_reader->read()) {
        if (difference_ppns.find(record.getControlNumber()) == difference_ppns.end())
            output_writer->write(record);
    }

    // 2. Append the records that are in "difference_member".
    const auto difference_reader(MARC::Reader::Factory(difference_member, MARC::FileType::BINARY));
    while (const auto difference_record = difference_reader->read())
        output_writer->write(difference_record);

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

    std::sort(input_archive_members.begin(), input_archive_members.end(), ArchiveMemberComparator);
    std::sort(difference_archive_members.begin(), difference_archive_members.end(), ArchiveMemberComparator);

    auto input_member(input_archive_members.cbegin());
    auto difference_member(difference_archive_members.cbegin());
    while (input_member != input_archive_members.cend() and difference_member != difference_archive_members.cend()) {
        if (input_member == input_archive_members.cend()) {
            if (use_subdirectories)
                FileUtil::CopyOrDie(*difference_member, RemovePIDSuffix(*difference_member));
            else
                RemoveSuffixFromDifferentialArchiveMember(*difference_member);
            ++difference_member;
        } else if (difference_member == difference_archive_members.cend()) {
            if (use_subdirectories)
                FileUtil::CopyOrDie(*input_member, StripTarGz(output_archive) + "/" + FileUtil::GetLastPathComponent(*input_member));
            ++input_member;
        } else {
            const char input_type(BSZUtil::GetTypeCharOrDie(*input_member));
            const char difference_type(BSZUtil::GetTypeCharOrDie(*difference_member));
            if (input_type == difference_type) {
                PatchMember(use_subdirectories, *input_member, *difference_member, output_archive);
                ++input_member;
                ++difference_member;
            } else if (input_type < difference_type) {
                if (use_subdirectories)
                    FileUtil::CopyOrDie(*input_member, StripTarGz(output_archive) + "/" + FileUtil::GetLastPathComponent(*input_member));
                ++input_member;
            } else {
                if (use_subdirectories)
                    FileUtil::CopyOrDie(*difference_member, RemovePIDSuffix(*difference_member));
                else
                    RemoveSuffixFromDifferentialArchiveMember(*difference_member);
                ++difference_member;
            }
        }
    }
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
    ArchiveWriter archive_writer(output_archive, archive_write_options);
    for (const auto &output_archive_member : output_archive_members)
        archive_writer.add(output_archive_member);
}


void GetDirectoryContentsWithRelativepath(const std::string &archive_name, std::vector<std::string> * const archive_members) {
    const std::string directory_name(StripTarGz(archive_name));
    FileUtil::GetFileNameList(".raw$", archive_members, directory_name);
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
        const auto directory_name(StripTarGz(output_archive));
        if (not FileUtil::MakeDirectory(directory_name))
            LOG_ERROR("failed to create directory \"" + directory_name + "\"!");
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

    if (not use_subdirectories)
        FileUtil::ChangeDirectoryOrDie("..");

    return EXIT_SUCCESS;
}
