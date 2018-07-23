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
#include "Compiler.h"
#include "File.h"
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=log_level] [--keep-intermediate-files] input_archive "
              << "difference_archive output_archive\n"
              << "       Log levels are DEBUG, INFO, WARNING and ERROR with INFO being the default.\n\n";
    std::exit(EXIT_FAILURE);
}


// Hopefully returns 'a', 'b' or 'c'.
char GetTypeChar(const std::string &member_name) {
    static auto matcher(RegexMatcher::RegexMatcherFactoryOrDie("([abc])00\\d.raw"));
    if (not matcher->matched(member_name))
        LOG_ERROR("bad member type for archive member \"" + member_name + "\"!");

    return (*matcher)[1][0];
}


// Maps ".*[abc]00?.raw" to ".*[abc]001.raw"
inline std::string GenerateOutputMemberName(std::string member_name) {
    if (unlikely(member_name.length() < 8))
        LOG_ERROR("short archive member name \"" + member_name + "\"!");
    member_name[member_name.length() - 5 - 1] = '1';
    return member_name;
}


// Extracts members from "archive_name" combining those of the same type, e.g. members ending in "a001.raw" and "a002.raw" would
// be extracted as a single concatenated file whose name ends in "a001.raw".  If "optional_suffix" is not empty it will be appended
// to each filename.
// An enforced precondition is that all members must end in "[abc]00\\d.raw$".
void ExtractArchiveMembers(const std::string &archive_name, std::vector<std::string> * const archive_members,
                           const std::string &optional_suffix = "")
{
    static auto member_matcher(RegexMatcher::RegexMatcherFactoryOrDie("[abc]00\\d.raw$"));

    std::map<char, std::shared_ptr<File>> member_type_to_file_map;
    ArchiveReader reader(archive_name);
    ArchiveReader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
        const std::string member_name(file_info.getFilename());
        if (unlikely(not file_info.isRegularFile()))
            LOG_ERROR("unexpectedly, the entry \"" + member_name + "\" in \"" + archive_name + "\" is not a regular file!");
        if (unlikely(not member_matcher->matched(member_name)))
            LOG_ERROR("unexpected entry name \"" + member_name + "\"!");

        const char member_type(GetTypeChar(member_name));
        auto type_and_file(member_type_to_file_map.find(member_type));
        if (type_and_file == member_type_to_file_map.end()) {
            const auto output_filename(GenerateOutputMemberName(member_name) + optional_suffix);
            std::shared_ptr<File> file(FileUtil::OpenOutputFileOrDie(output_filename));
            member_type_to_file_map.emplace(member_type, file);
            type_and_file = member_type_to_file_map.find(member_type);
            archive_members->emplace_back(output_filename);
        }

        char buf[8192];
        size_t read_count;
        while ((read_count = reader.read(buf, sizeof buf)) > 0) {
            if (unlikely(type_and_file->second->write(buf, read_count) != read_count))
                LOG_ERROR("failed to write data to \"" + type_and_file->second->getPath() + "\"! (No room?)");
        }
    }
}


// Compare according to type ('a', 'b', or 'c').
bool ArchiveMemberComparator(const std::string &member_name1, const std::string &member_name2) {
    return GetTypeChar(member_name1) < GetTypeChar(member_name2);
}


// Assumes that member_name ends in "-PID" and renames it to a new name w/o the "-PID".
void RemoveSuffixFromDifferentialArchiveMember(const std::string &member_name) {
    const std::string SUFFIX("-" + std::to_string(::getpid()));
    if (unlikely(not StringUtil::EndsWith(member_name, SUFFIX)))
        LOG_ERROR("differential member name \"" + member_name + "\" is missing suffix \"" + SUFFIX + "\"!");
    FileUtil::RenameFileOrDie(member_name, member_name.substr(member_name.length() - SUFFIX.length()));
}


void CollectPPNs(const std::string &marc_filename, std::unordered_set<std::string> * const ppns) {
    const auto reader(MARC::Reader::Factory(marc_filename, MARC::FileType::BINARY));
    while (const auto record = reader->read())
        ppns->emplace(record.getControlNumber());
}


// Patches "input_member" w/ "difference_member".  The result is the patched "input_member".
void PatchMember(const std::string &input_member, const std::string &difference_member) {
    std::unordered_set<std::string> difference_ppns;
    CollectPPNs(difference_member, &difference_ppns);

    const auto input_reader(MARC::Reader::Factory(input_member, MARC::FileType::BINARY));
    const std::string temp_filename("patch-" + std::to_string(::getpid()));
    const auto temp_writer(MARC::Writer::Factory(temp_filename, MARC::FileType::BINARY));

    // 1. Filter out the PPN's that are in "difference_member".
    while (const auto record = input_reader->read()) {
        if (difference_ppns.find(record.getControlNumber()) == difference_ppns.end())
            temp_writer->write(record);
    }

    // 2. Append the records that are in "difference_member".
    const auto difference_reader(MARC::Reader::Factory(difference_member, MARC::FileType::BINARY));
    while (const auto difference_record = difference_reader->read())
        temp_writer->write(difference_record);

    FileUtil::RenameFileOrDie(temp_filename, input_member);
}


void ChangeDirectoryOrDie(const std::string &directory) {
    if (unlikely(::chdir(directory.c_str()) != 0))
        LOG_ERROR("failed to change directory to \"" + directory + "\"!");
}


void PatchArchiveMembersAndCreateOutputArchive(std::vector<std::string> input_archive_members,
                                               std::vector<std::string> difference_archive_members, const std::string &output_archive)
{
    if (input_archive_members.empty())
        LOG_ERROR("no input archive members!");
    if (difference_archive_members.empty())
        LOG_ERROR("no difference archive members!");

    std::sort(input_archive_members.begin(), input_archive_members.end(), ArchiveMemberComparator);
    std::sort(difference_archive_members.begin(), difference_archive_members.end(), ArchiveMemberComparator);

    auto input_member(input_archive_members.cbegin());
    auto difference_member(difference_archive_members.cbegin());
    while (input_member != input_archive_members.cend() and difference_member != difference_archive_members.cend()) {
        if (input_member == input_archive_members.cend()) {
            RemoveSuffixFromDifferentialArchiveMember(*difference_member);
            ++difference_member;
        } else if (difference_member == difference_archive_members.cend())
            ++input_member;
        else {
            const char input_type(GetTypeChar(*input_member));
            const char difference_type(GetTypeChar(*difference_member));
            if (input_type == difference_type) {
                PatchMember(*input_member, *difference_member);
                ++input_member;
                ++difference_member;
            } else if (input_type < difference_type)
                ++input_member;
            else {
                RemoveSuffixFromDifferentialArchiveMember(*difference_member);
                ++difference_member;
            }
        }
    }

    //
    // Recreate archive
    //

    std::vector<std::string> output_archive_members;
    if (FileUtil::GetFileNameList(".*[abc]00\\d.raw", &output_archive_members) == 0)
        LOG_ERROR("missing output archive members!");

    ArchiveWriter archive_writer("../" + output_archive);
    for (const auto &output_archive_member : output_archive_members)
        archive_writer.add(output_archive_member);
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

    const std::string input_archive(argv[1]);
    const std::string difference_archive(argv[2]);
    const std::string output_archive(argv[3]);

    if (input_archive == difference_archive or input_archive == output_archive or difference_archive == output_archive)
        LOG_ERROR("all archive names must be distinct!");

    FileUtil::AutoTempDirectory working_directory(std::string(::progname) + "-working-dir");
    ChangeDirectoryOrDie(working_directory.getDirectoryPath());

    std::vector<std::string> input_archive_members;
    ExtractArchiveMembers("../" + input_archive, &input_archive_members);

    std::vector<std::string> difference_archive_members;
    ExtractArchiveMembers("../" + difference_archive, &difference_archive_members, "-" + std::to_string(::getpid()));

    PatchArchiveMembersAndCreateOutputArchive(input_archive_members, difference_archive_members, "../" + output_archive);

    ChangeDirectoryOrDie("..");

    if (not keep_intermediate_files) {
        if (not FileUtil::RemoveDirectory(working_directory.getDirectoryPath()))
            LOG_ERROR("failed to delete working directory: \"" + working_directory.getDirectoryPath() + "\"!");
    }

    return EXIT_SUCCESS;
}
