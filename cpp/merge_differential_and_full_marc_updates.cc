/** \file    merge_differential_and_full_marc_updates.cc
 *  \brief   A tool for creating combined full updates from an older full update and one or more differential updates.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2018 Library of the University of Tübingen

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

/*
  Config files for this program look like this:

[Files]
deletion_list              = LOEPPN(?:_m)?-\d{6}
incremental_authority_dump = (?:WA-MARCcomb-sekkor)-(\d{6}).tar.gz
*/

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>
#include "Archive.h"
#include "BSZUtil.h"
#include "Compiler.h"
#include "EmailSender.h"
#include "ExecUtil.h"
#include "File.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--keep-intermediate-files] default_email_recipient\n";
    std::exit(EXIT_FAILURE);
}


std::string default_email_recipient;
std::string email_server_address;
std::string email_server_user;
std::string email_server_password;


const std::string EMAIL_CONF_FILE_PATH("/usr/local/var/lib/tuelib/cronjobs/smtp_server.conf");
const std::string CONF_FILE_PATH(
    "/usr/local/var/lib/tuelib/cronjobs/merge_differential_and_full_marc_updates.conf");


void ProcessIniFiles(std::string * const deletion_list_pattern, std::string * const incremental_authority_dump_pattern) {
    const IniFile email_ini_file(EMAIL_CONF_FILE_PATH);
    ::email_server_address  = email_ini_file.getString("SMTPServer", "server_address");
    ::email_server_user     = email_ini_file.getString("SMTPServer", "server_user");
    ::email_server_password = email_ini_file.getString("SMTPServer", "server_password");

    const IniFile ini_file(CONF_FILE_PATH);
    *deletion_list_pattern = ini_file.getString("Files", "deletion_list");
    *incremental_authority_dump_pattern = ini_file.getString("Files", "incremental_authority_dump");
}


std::string GetProgramBasename() {
    static std::string basename;
    if (basename.empty()) {
        std::string dirname;
        FileUtil::DirnameAndBasename(::progname, &dirname, &basename);
    }

    return basename;
}


std::string GetHostname() {
    char buf[1024];
    if (unlikely(::gethostname(buf, sizeof buf) != 0))
        LOG_ERROR("gethostname(2) failed! (" + std::string(::strerror(errno)) + ")");
    buf[sizeof(buf) - 1] = '\0';

    return buf;
}


void SendEmail(const std::string &subject, const std::string &message_body, const EmailSender::Priority priority) {
    if (not EmailSender::SendEmail(::email_server_user, ::default_email_recipient, subject, message_body, priority))
        LOG_ERROR("failed to send an email!");
}


void LogSendEmailAndDie(const std::string &one_line_message) {
    LOG_DEBUG(one_line_message);
    SendEmail(GetProgramBasename() + " failed! (from " + GetHostname() + ")",
              one_line_message + "\n", EmailSender::VERY_HIGH);
    std::exit(EXIT_FAILURE);
}


// Populates "filenames" with a list of regular files and returns the number of matching filenames that were found
// in the current working directory.  The list will be sorted in alphanumerical order.
unsigned GetSortedListOfRegularFiles(const std::string &filename_regex, std::vector<std::string> * const filenames)
{
    filenames->clear();

    FileUtil::Directory directory(".", filename_regex + "$" /* Skip additional extensions. */);
    for (const auto entry : directory) {
        const int entry_type(entry.getType());
        if (entry_type == DT_REG or entry_type == DT_UNKNOWN)
            filenames->emplace_back(entry.getName());
    }

    std::sort(filenames->begin(), filenames->end());

    return filenames->size();
}


void GetFilesMoreRecentThanOrEqual(const std::string &cutoff_date, const std::string &filename_pattern,
                                   std::vector<std::string> * const filenames)
{
    GetSortedListOfRegularFiles(filename_pattern, filenames);

    const auto first_deletion_position(filenames->begin());
    auto last_deletion_position(filenames->begin());
    while (last_deletion_position < filenames->end()
           and BSZUtil::ExtractDateFromFilenameOrDie(*last_deletion_position) < cutoff_date)
        ++last_deletion_position;

    const auto erase_count(std::distance(first_deletion_position, last_deletion_position));
    if (unlikely(erase_count > 0)) {
        LOG_WARNING("ignoring " + std::to_string(erase_count) + " files matching \"" + filename_pattern
                    + "\" because they are too old for the cut-off date " + cutoff_date + "!");
        filenames->erase(first_deletion_position, last_deletion_position);
    }
}


std::string GetWorkingDirectoryName() {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(::progname, &dirname, &basename);
    return basename + ".working_directory";
}


void CreateAndChangeIntoTheWorkingDirectory() {
    const std::string working_directory(GetWorkingDirectoryName());
    if (FileUtil::Exists(working_directory)) {
        if (not FileUtil::RemoveDirectory(working_directory))
            LogSendEmailAndDie("in CreateAndChangeIntoTheWorkingDirectory failed to remove stale \""
                               + working_directory + "\"!");
    }
    if (not FileUtil::MakeDirectory(working_directory))
        LogSendEmailAndDie("in CreateAndChangeIntoTheWorkingDirectory failed to create \"" + working_directory
                           + "\"!");

    FileUtil::ChangeDirectoryOrDie(working_directory);
}


/** \brief Based on the name of the archive entry "archive_entry_name", this function generates a disc file name.
 *
 * The strategy used is to return identify an earlier entry name that only differed in positions that are digits.
 * If such a name can be identified then "disc_filename" will be set to that name, o/w "disc_filename" will be
 * set to "archive_entry_name".  The "open_mode" will be set to "a" for append if we found a similar earlier entry and
 * to "w" for write if this is the first occurrence of a name pattern.
 */
void GetOutputNameAndMode(const std::string &archive_entry_name,
                          std::map<std::shared_ptr<RegexMatcher>, std::string> * const regex_to_first_file_map,
                          std::string * const disc_filename, std::string * const open_mode)
{
    for (const auto &reg_ex_and_first_name : *regex_to_first_file_map) {
        if (reg_ex_and_first_name.first->matched(archive_entry_name)) {
            *disc_filename = reg_ex_and_first_name.second;
            *open_mode = "a"; // append
            return;
        }
    }

    std::string regex_pattern;
    for (char ch : archive_entry_name)
        regex_pattern += isdigit(ch) ? "\\d" : std::string(1, ch);

    std::string err_msg;
    regex_to_first_file_map->emplace(std::shared_ptr<RegexMatcher>(
        RegexMatcher::RegexMatcherFactory(regex_pattern, &err_msg)), archive_entry_name);
    if (unlikely(not err_msg.empty()))
        LogSendEmailAndDie("in GetOutputNameAndMode: failed to compile regex \"" + regex_pattern + "\"! ("
                           + err_msg + ")");

    *disc_filename = archive_entry_name;
    *open_mode = "w"; // create new
}


// Extracts files from a MARC archive, typically a gzipped tar file, and combines files matching the same pattern.
// For example, if the archive contains "SA-MARC-ixtheoa001.raw" and "SA-MARC-ixtheoa002.raw",
// "SA-MARC-ixtheoa002.raw" will be concatenated onto "SA-MARC-ixtheoa001.raw" so that only a single disc file will
// result.
// Note: The "returned" file list will be alphanumerically sorted.
void ExtractMarcFilesFromArchive(const std::string &archive_name, std::vector<std::string> * const extracted_names,
                                 const std::string &name_prefix = "", const std::string &name_suffix = "")
{
    LOG_DEBUG("extracting files from archive \"" + archive_name + "\".");
    extracted_names->clear();

    std::map<std::shared_ptr<RegexMatcher>, std::string> regex_to_first_file_map;

    ArchiveReader reader(archive_name);
    ArchiveReader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
        if (unlikely(not file_info.isRegularFile()))
            LogSendEmailAndDie("in ExtractMarcFilesFromArchive: unexpectedly, the entry \"" + file_info.getFilename()
                               + "\" in \"" + archive_name + "\" is not a regular file!");

        std::string output_filename, open_mode;
        GetOutputNameAndMode(file_info.getFilename(), &regex_to_first_file_map, &output_filename, &open_mode);
        output_filename = name_prefix + output_filename + name_suffix;
        File disc_file(output_filename, open_mode);

        if (open_mode != "a")
            extracted_names->emplace_back(output_filename);

        char buf[8192];
        size_t read_count;
        while ((read_count = reader.read(buf, sizeof buf)) > 0) {
            if (unlikely(disc_file.write(buf, read_count) != read_count))
                LogSendEmailAndDie("in ExtractMarcFilesFromArchive: failed to write data to \"" + output_filename
                                   + "\"! (No room?)");
        }
    }

    std::sort(extracted_names->begin(), extracted_names->end());
}


// Returns the current date in the YYMMDD format.
std::string GetCurrentDate() {
    const time_t now(std::time(nullptr));
    const struct tm * const local_time(std::localtime(&now));
    char buffer[6 + 1];
    if (unlikely(std::strftime(buffer, sizeof buffer, "%y%m%d", local_time) != 6))
        LogSendEmailAndDie("in GetCurrentDate: strftime(3) failed! (This should never happen!)");

    return buffer;
}


std::string ReplaceStringOrDie(const std::string &original, const std::string &replacement, const std::string &s) {
    const size_t original_start(s.find(original));
    if (unlikely(original_start == std::string::npos))
        LogSendEmailAndDie("in ReplaceStringOrDie: can't replace \"" + original + "\" with \"" + replacement
                           + " in \"" + s + "\"!");
    return s.substr(0, original_start) + replacement + s.substr(original_start + original.length());
}


void CopyFileOrDie(const std::string &from, const std::string &to) {
    struct stat stat_buf;
    if (unlikely(::stat(from.c_str(), &stat_buf) != 0))
        LogSendEmailAndDie("in CopyFileOrDie: stat(2) on \"" + from + "\" failed! (" + std::string(::strerror(errno))
                           + ")");

    const int from_fd(::open(from.c_str(), O_RDONLY));
    if (unlikely(from_fd == -1))
        LogSendEmailAndDie("in CopyFileOrDie: open(2) on \"" + from + "\" failed! (" + std::string(::strerror(errno))
                           + ")");

    const int to_fd(::open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, stat_buf.st_mode));
    if (unlikely(to_fd == -1))
        LogSendEmailAndDie("in CopyFileOrDie: open(2) on \"" + to + "\" failed! (" + std::string(::strerror(errno))
                           + ")");

    if (unlikely(::sendfile(to_fd, from_fd, /* offset = */nullptr, stat_buf.st_size) == -1))
        LogSendEmailAndDie("in CopyFileOrDie: sendfile(2) on \"" + from + "\" and \"" + to + "\" failed! ("
                           + std::string(::strerror(errno)) + ")");

    ::close(from_fd);
    ::close(to_fd);
}


/** \return True if all names end in "[abc]001.raw", else false. */
bool ArchiveEntryFilenamesMeetNamingExpectations(const std::vector<std::string> &archive_entry_names) {
    for (const auto &entry_name : archive_entry_names) {
        if (not StringUtil::EndsWith(entry_name, "a001.raw") and not StringUtil::EndsWith(entry_name, "b001.raw")
            and not StringUtil::EndsWith(entry_name, "c001.raw"))
            return false;
    }

    return true;
}


// Hopefully returns strings like "a001.raw" etc.
inline std::string GetArchiveEntrySuffix(const std::string &archive_entry_name) {
    return archive_entry_name.substr(archive_entry_name.length() - 8 - 1);
}


void MergeAndDedupArchiveFiles(const std::vector<std::string> &local_data_filenames,
                               const std::vector<std::string> &no_local_data_filenames,
                               const std::string &target_archive_name)
{
    LOG_DEBUG("merging and deduping archive files to create \"" + target_archive_name + "\".");
    FileUtil::AutoTempDirectory working_dir(".", /* cleanup_if_exception_is_active = */false);
    FileUtil::ChangeDirectoryOrDie(working_dir.getDirectoryPath());

    auto local_data_filename(local_data_filenames.cbegin());
    auto no_local_data_filename(no_local_data_filenames.cbegin());

    while (local_data_filename != local_data_filenames.cend() or no_local_data_filename != no_local_data_filenames.cend()) {
        if (local_data_filename == local_data_filenames.cend()
            or GetArchiveEntrySuffix(*no_local_data_filename) < GetArchiveEntrySuffix(*local_data_filename))
        {
            LOG_INFO("copying no-local-data file \"" + *no_local_data_filename + "\"");
            CopyFileOrDie("../" + *no_local_data_filename, GetArchiveEntrySuffix(*no_local_data_filename));
            ++no_local_data_filename;
        } else if (no_local_data_filename == no_local_data_filenames.cend()
                   or GetArchiveEntrySuffix(*local_data_filename) < GetArchiveEntrySuffix(*no_local_data_filename))
        {
            LOG_INFO("copying local-data file \"" + *local_data_filename + "\"");
            CopyFileOrDie("../" + *local_data_filename, GetArchiveEntrySuffix(*local_data_filename));
            ++local_data_filename;
        } else { // If we end up here, local_data_filename and no_local_data_filename have the same suffix.
            LOG_INFO("merging both, the local-data file \"" + *local_data_filename
                 + "\" and the no-local-data file \"" + *no_local_data_filename + "\"");
            const std::string common_prefix(local_data_filename->substr(0, local_data_filename->length() - 3 /* extension */));

            // We can't use the usual ".raw" file name here because RemoveDuplicateControlNumberRecords requires a ".xml" or
            // a ".mrc" extension to identify the file type.
            const std::string temp_filename(common_prefix + "mrc");

            FileUtil::ConcatFiles(temp_filename, { *local_data_filename, *no_local_data_filename });
            const unsigned dropped_count(MARC::RemoveDuplicateControlNumberRecords(temp_filename));
            const std::string archive_member_filename(GetArchiveEntrySuffix(*local_data_filename));
            FileUtil::RenameFileOrDie(temp_filename, archive_member_filename, true /* remove_target */);
            LOG_INFO("dropped " + std::to_string(dropped_count) + " records with duplicate PPN's and generated \""
                 + archive_member_filename + "\".");

            ++local_data_filename;
            ++no_local_data_filename;
        }
    }

    // Create the archive with the combined entries:
    std::vector<std::string> combined_entries;
    FileUtil::GetFileNameList("[abc]00.\\.raw", &combined_entries);
    ArchiveWriter archive_writer("../" + target_archive_name);
    for (const auto &combined_entry : combined_entries)
        archive_writer.add(combined_entry);

    FileUtil::ChangeDirectoryOrDie("..");
}


std::string GetNoLocalDataFileName(const std::string &local_data_filename) {
    const std::string date(BSZUtil::ExtractDateFromFilenameOrDie(local_data_filename));
    const auto date_start(local_data_filename.find(date));
    return local_data_filename.substr(0, date_start - 1 /* skip hyphen */) + "_o-" + local_data_filename.substr(date_start);
}


// Here we combine an archive which contains local data with one that contains no local data but possibly duplicate control
// numbers.  We return the name of the combined archive.
std::string CombineMarcBiblioArchives(const std::string &filename_prefix, const std::string &combined_filename_prefix) {
    const std::string local_data_archive_name(filename_prefix + ".tar.gz");
    const std::string no_local_data_archive_name(GetNoLocalDataFileName(local_data_archive_name));
    const std::string combined_archive_name(combined_filename_prefix + ".tar.gz");
    if (not FileUtil::Exists(local_data_archive_name)) {
        if (not FileUtil::Exists(no_local_data_archive_name))
            LOG_ERROR("in CombineMarcBiblioArchives: neither \"" + local_data_archive_name + "\" nor \""
                      + no_local_data_archive_name + "\" can be found!");
        CopyFileOrDie(no_local_data_archive_name, combined_archive_name);
        return combined_archive_name;
    } else if (not FileUtil::Exists(no_local_data_archive_name)) {
        CopyFileOrDie(local_data_archive_name, combined_archive_name);
        return combined_archive_name;
    }

    //
    // If we made it this far, both source archives exist.
    //

    FileUtil::AutoTempDirectory auto_temp_marc_local_data_dir(".", /* cleanup_if_exception_is_active = */false);
    std::vector<std::string> local_archive_member_filenames;
    ExtractMarcFilesFromArchive(local_data_archive_name, &local_archive_member_filenames,
                                auto_temp_marc_local_data_dir.getDirectoryPath() + "/");
    if (not ArchiveEntryFilenamesMeetNamingExpectations(local_archive_member_filenames))
        LOG_ERROR("in CombineMarcBiblioArchives: archive \"" + local_data_archive_name
                  + "\" contains at least one entry that does not meet our naming expectations"
                  + " in " + StringUtil::Join(local_archive_member_filenames, ", ") + "! (1)");

    FileUtil::AutoTempDirectory auto_temp_marc_no_local_data_dir(".", /* cleanup_if_exception_is_active = */false);
    std::vector<std::string> no_local_archive_member_filenames;
    ExtractMarcFilesFromArchive(no_local_data_archive_name, &no_local_archive_member_filenames,
                                auto_temp_marc_no_local_data_dir.getDirectoryPath() + "/");
    if (not ArchiveEntryFilenamesMeetNamingExpectations(no_local_archive_member_filenames))
        LOG_ERROR("in CombineMarcBiblioArchives: archive \"" + no_local_data_archive_name
                  + "\" contains at least one entry that does not meet our naming expectations"
                  + " in " + StringUtil::Join(no_local_archive_member_filenames, ", ") + "! (2)");

    MergeAndDedupArchiveFiles(local_archive_member_filenames, no_local_archive_member_filenames, combined_archive_name);
    return combined_archive_name;
}


std::string GetOrGenerateCompleteDumpFile(const std::string &tuefind_flavour) {
    const std::string complete_dump_filename_pattern("Complete-MARC-" + tuefind_flavour + "-\\d{6}\\.tar\\.gz");
    std::vector<std::string> complete_dump_filenames;
    GetSortedListOfRegularFiles(complete_dump_filename_pattern, &complete_dump_filenames);

    const std::string SA_filename_pattern("SA-MARC-" + tuefind_flavour + "-\\d{6}\\.tar\\.gz");
    std::vector<std::string> SA_filenames;
    GetSortedListOfRegularFiles(SA_filename_pattern, &SA_filenames);
    if (unlikely(complete_dump_filenames.empty() and SA_filenames.empty()))
        LogSendEmailAndDie("did not find a complete MARC dump matching either \"" + complete_dump_filename_pattern
                           + "\" or \"" + SA_filename_pattern + "\"!");

    std::string most_recent_SA_date;
    if (SA_filenames.empty()) {
        const std::string &chosen_filename(complete_dump_filenames.back());
        LOG_DEBUG("picking \"" + chosen_filename + "\" as the complete MARC dump.");
        return chosen_filename;
    } else
        most_recent_SA_date = BSZUtil::ExtractDateFromFilenameOrDie(SA_filenames.back());

    if (not complete_dump_filenames.empty()
        and BSZUtil::ExtractDateFromFilenameOrDie(complete_dump_filenames.back()) > most_recent_SA_date)
    {
        const std::string &chosen_filename(complete_dump_filenames.back());
        LOG_DEBUG("picking \"" + chosen_filename + "\" as the complete MARC dump.");
        return chosen_filename;
    }

    // If we end up here we have to generate a new complete MARC dump:
    const std::string new_complete_dump_filename(
        CombineMarcBiblioArchives("SA-MARC-" + tuefind_flavour + "-" + most_recent_SA_date,
                                  "Complete-MARC-" + tuefind_flavour + "-" + most_recent_SA_date));
    LOG_DEBUG("generated \"" + new_complete_dump_filename + "\".");

    return new_complete_dump_filename;
}


// Appends "append_source" onto "append_target".
void AppendFileOrDie(const std::string &append_target, const std::string &append_source) {
    LOG_DEBUG("about to append \"" + append_source + "\" onto \"" + append_target + "\".");
    File append_target_file(append_target, "a");
    if (unlikely(append_target_file.fail()))
        LogSendEmailAndDie("in AppendFileOrDie: failed to open \"" + append_target + "\" for writing! ("
                           + std::string(::strerror(errno)) + ")");
    File append_source_file(append_source, "r");
    if (unlikely(append_source_file.fail()))
        LogSendEmailAndDie("in AppendFileOrDie: failed to open \"" + append_source + "\" for reading! ("
                           + std::string(::strerror(errno)) + ")");
    if (unlikely(not append_target_file.append(append_source_file)))
        LogSendEmailAndDie("in AppendFileOrDie: failed to append \"" + append_source + "\" to \"" + append_target
                           + "\"! (" + std::string(::strerror(errno)) + ")");
}


void DeleteFileOrDie(const std::string &filename) {
    LOG_DEBUG("about to delete \"" + filename + "\".");
    if (unlikely(::unlink(filename.c_str()) != 0))
        LogSendEmailAndDie("in DeleteFileOrDie: unlink(2) on \"" + filename + "\" failed! ("
                           + std::string(::strerror(errno)) + ")");
}


const std::string DELETE_IDS_COMMAND("/usr/local/bin/delete_ids");
const std::string LOCAL_DELETION_LIST_FILENAME("deletions.list");


void UpdateOneFile(const std::string &old_marc_filename, const std::string &new_marc_filename,
                   const std::string &differential_marc_file)
{
    LOG_DEBUG("creating \"" + new_marc_filename + "\" from \"" + old_marc_filename
              + "\" and an optional deletion list and difference file \"" + differential_marc_file + "\".");

    if (unlikely(ExecUtil::Exec(DELETE_IDS_COMMAND,
                                { LOCAL_DELETION_LIST_FILENAME, old_marc_filename, new_marc_filename }) != 0))
        LogSendEmailAndDie("in UpdateOneFile: \"" + DELETE_IDS_COMMAND + "\" failed!");

    if (FileUtil::Exists(differential_marc_file))
        AppendFileOrDie(new_marc_filename, differential_marc_file);
}


/** \brief  Returns a pathname that matches "regex".
 *  \param  regex     The PCRE regex that is the name pattern.
 *  \param  pathname  Where to return the matched name, if we had exactly one match.
 *  \return True if there is precisely one match, else false.
 *  \note   If no match was found "pathname" will be cleared.
 */
bool GetMatchingFilename(const std::string &regex, std::string * const pathname) {
    pathname->clear();

    std::vector<std::string> matched_pathnames;
    size_t count;
    if ((count = FileUtil::GetFileNameList(regex, &matched_pathnames)) != 1) {
        if (count == 0)
            pathname->clear();
        return false;
    }

    pathname->swap(matched_pathnames[0]);

    return true;
}


void GetBasenamesOrDie(std::string * const title_marc_basename, std::string * const superior_marc_basename,
                       std::string * const authority_marc_basename, const std::string &suffix = "")
{
    if (unlikely(not GetMatchingFilename("a001\\.raw" + suffix + "$", title_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"a001\\.raw" + suffix + "$\"!");

    if (unlikely(not GetMatchingFilename("b001\\.raw" + suffix + "$", superior_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"b001\\.raw" + suffix + "$\"!");

    if (unlikely(not GetMatchingFilename("c001\\.raw" + suffix + "$", authority_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"c001\\.raw" + suffix + "$\"!");
}


void DeleteFilesOrDie(const std::string &filename_regex) {
    if (unlikely(FileUtil::RemoveMatchingFiles(filename_regex) == -1))
        LogSendEmailAndDie("failed to delete files matching \"" + filename_regex + "\"!");
}


// Name of the shell script that extracts control numbers from a MARC file and appends them
// to a deletion list file.
const std::string EXTRACT_AND_APPEND_SCRIPT("/usr/local/bin/extract_IDs_in_erase_format.sh");


void ExtractAndAppendIDs(const std::string &marc_filename, const std::string &deletion_list_filename) {
    if (unlikely(ExecUtil::Exec(EXTRACT_AND_APPEND_SCRIPT, { marc_filename, deletion_list_filename }) != 0))
        LogSendEmailAndDie("\"" + EXTRACT_AND_APPEND_SCRIPT + "\" with arguments \"" + marc_filename + "\" and \""
                           + deletion_list_filename + "\" failed!");
}


/** \brief Replaces "filename"'s ending "old_suffix" with "new_suffix".
 *  \note Aborts if "filename" does not end with "old_suffix".
 */
std::string ReplaceSuffix(const std::string &filename, const std::string &old_suffix, const std::string &new_suffix) {
    if (unlikely(not StringUtil::EndsWith(filename, old_suffix)))
        LogSendEmailAndDie("in ReplaceSuffix: \"" + filename + "\" does not end with \"" + old_suffix + "\"!");
    return filename.substr(0, filename.length() - old_suffix.length()) + new_suffix;
}


void LogLineCount(const std::string &filename) {
    if (not FileUtil::Exists(filename)) {
        LOG_WARNING("\"" + filename + "\" does not exist!");
        return;
    }

    File input(filename, "r");
    unsigned line_count(0);
    while (not input.eof()) {
        input.getline();
        ++line_count;
    }

    LOG_INFO("\"" + filename + "\" contains " + std::to_string(line_count) + " lines.");
}


/** \brief Creates an empty file if "pathname" does not exist. */
void IfNotExistsMakeEmptyOrDie(const std::string &pathname) {
    if (not FileUtil::Exists(pathname) and not FileUtil::MakeEmpty(pathname))
        LogSendEmailAndDie("failed to create empty file \"" + pathname + "\"!");
}


void ApplyUpdate(const bool keep_intermediate_files, const unsigned apply_count,
                 const std::string &deletion_list_filename, const std::string &differential_archive)
{
    if (not deletion_list_filename.empty())
        CopyFileOrDie("../" + deletion_list_filename, LOCAL_DELETION_LIST_FILENAME);
    else if (differential_archive.empty())
        LogSendEmailAndDie("in ApplyUpdate: both, \"deletion_list_filename\" and \"differential_archive\" are "
                           "empty strings.  This should never happen!");

    // Unpack the differential archive and extract control numbers from its members appending them to the
    // deletion list file:
    if (not differential_archive.empty()) {
        LOG_DEBUG("updating the deletion list based on control numbers found in the files contained in the differential MARC archive.");
        std::vector<std::string> extracted_names;
        ExtractMarcFilesFromArchive("../" + differential_archive, &extracted_names, "diff_");
        for (const auto &extracted_name : extracted_names) {
            LOG_DEBUG("Processing \"" + extracted_name + "\" in order to extract control numbers to append to the deletion list.");
            ExtractAndAppendIDs(extracted_name, LOCAL_DELETION_LIST_FILENAME);
        }

        LogLineCount(LOCAL_DELETION_LIST_FILENAME);
    }

    // If we extracted empty MARC files we might not have a deletion list, thus...
    IfNotExistsMakeEmptyOrDie(LOCAL_DELETION_LIST_FILENAME);

    const std::string old_name_suffix("." + std::to_string(apply_count - 1));
    std::string title_marc_basename, superior_marc_basename, authority_marc_basename;
    GetBasenamesOrDie(&title_marc_basename, &superior_marc_basename, &authority_marc_basename, old_name_suffix);

    const std::string new_name_suffix("." + std::to_string(apply_count));
    std::string diff_filename;

    // Update the title data:
    std::string diff_filename_pattern("diff_(.*a001.raw|sekkor-tit.mrc)");
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LOG_WARNING("found no match for \"" + diff_filename_pattern
                    + "\" which might match a file extracted from \"" + differential_archive + "\"!");
    const std::string new_title_marc_filename(ReplaceSuffix(title_marc_basename, old_name_suffix, new_name_suffix));
    UpdateOneFile(title_marc_basename, new_title_marc_filename, diff_filename);

    // Update the superior data:
    diff_filename_pattern = "diff_.*b001.raw";
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LOG_WARNING("found no match for \"" + diff_filename_pattern
                    + "\" which might match a file extracted from \"" + differential_archive + "\"!");
    UpdateOneFile(superior_marc_basename, ReplaceSuffix(superior_marc_basename, old_name_suffix, new_name_suffix),
                  diff_filename);

    // Update the authority data:
    diff_filename_pattern = "diff_(.*c001.raw|sekkor-aut.mrc)";
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LOG_WARNING("found no match for \"" + diff_filename_pattern
                    + "\" which might match a file extracted from \"" + differential_archive + "\"!");
    const std::string new_authority_data_marc_filename(ReplaceSuffix(authority_marc_basename, old_name_suffix,
                                                               new_name_suffix));
    UpdateOneFile(authority_marc_basename, new_authority_data_marc_filename, diff_filename);

    if (not differential_archive.empty() and not keep_intermediate_files)
        DeleteFilesOrDie("diff_.*");

    if (not keep_intermediate_files) {
        DeleteFileOrDie(title_marc_basename);
        DeleteFileOrDie(superior_marc_basename);
        DeleteFileOrDie(authority_marc_basename);
        DeleteFileOrDie(LOCAL_DELETION_LIST_FILENAME);
    }
}


inline std::string RemoveFileNameSuffix(const std::string &filename, const std::string &suffix) {
    return ReplaceSuffix(filename, suffix, "");
}


// Creates a symlink called "link_filename" pointing to "target_filename".
void CreateSymlink(const std::string &target_filename, const std::string &link_filename) {
    if (unlikely(::unlink(link_filename.c_str()) == -1 and errno != ENOENT /* "No such file or directory." */))
        throw std::runtime_error("unlink(2) of \"" + link_filename + "\" failed: " + std::string(::strerror(errno)));
    if (unlikely(::symlink(target_filename.c_str(), link_filename.c_str()) != 0))
        LogSendEmailAndDie("failed to create symlink \"" + link_filename + "\" => \"" + target_filename + "\"! ("
                           + std::string(::strerror(errno)) + ")");
}


/** \brief Collects "raw" files w/ the "individual_file_suffix" and creates a new complete dump file from them.
 *  \return The name of the new complete dump file.
 */
std::string CreateNewCompleteMarcArchive(const std::string &old_date, const std::string &old_complete_dump_filename,
                                         const std::string &individual_file_suffix, const std::string &tuefind_flavour)
{
    LOG_DEBUG("Entering CreateNewCompleteMarcArchive w/ old_complete_dump_filename=\"" + old_complete_dump_filename + "\".");

    const std::string current_date(GetCurrentDate());
    const std::string new_complete_dump_filename(ReplaceStringOrDie(old_date, current_date, old_complete_dump_filename));
    LOG_DEBUG("old_date=\"" + old_date + "\", current_date=\"" + current_date + ", new_complete_dump_filename=\""
              + new_complete_dump_filename + "\"!");
    LOG_DEBUG("creating new MARC archive \"" + new_complete_dump_filename + "\".");
    std::vector<std::string> updated_marc_files;
    FileUtil::GetFileNameList("[abc]00.\\.raw\\" + individual_file_suffix, &updated_marc_files);
    ArchiveWriter archive_writer("../" + new_complete_dump_filename);
    for (const auto &updated_marc_file : updated_marc_files) {
        std::string archive_member_name(RemoveFileNameSuffix(updated_marc_file, individual_file_suffix));
        const std::string archive_member_prefix("SA-MARC-" + tuefind_flavour);
        if (not StringUtil::StartsWith(archive_member_name, archive_member_prefix)) {
            archive_member_name = archive_member_prefix
                + archive_member_name.substr(archive_member_name.length() - 8 /* [abc] + 3 digits + period + "raw" */);
        }
        LOG_DEBUG("Storing \"" + updated_marc_file + "\" as \"" + archive_member_name + "\" in \"" + new_complete_dump_filename + "\".");
        archive_writer.add(updated_marc_file, archive_member_name);
    }

    LOG_DEBUG("About to return \"" + new_complete_dump_filename + "\" from CreateNewCompleteMarcArchive.");
    return new_complete_dump_filename;
}


// Creates a new full MARC archive from an old full archive as well as deletion lists and differential updates.
std::string ExtractAndCombineMarcFilesFromArchives(const bool keep_intermediate_files, const std::string &tuefind_flavour,
                                                   const std::string &old_complete_dump_filename,
                                                   const std::vector<std::string> &deletion_list_filenames,
                                                   const std::vector<std::string> &incremental_dump_filenames)
{
    LOG_DEBUG("Entering ExtractAndCombineMarcFilesFromArchives w/ complete_dump_filename=\"" + old_complete_dump_filename + "\".");

    std::vector<std::string> extracted_names;
    ExtractMarcFilesFromArchive("../" + old_complete_dump_filename, &extracted_names,
                                /* name_prefix = */ "", /* name_suffix = */ ".0");

    // Iterate over the deletion list and incremental dump filename lists and apply one or both as appropriate:
    LOG_DEBUG("In ExtractAndCombineMarcFilesFromArchives: starting iteration over deletion and incremental lists.");
    auto deletion_list_filename(deletion_list_filenames.cbegin());
    auto incremental_dump_filename(incremental_dump_filenames.cbegin());
    unsigned apply_count(0);
    while (deletion_list_filename != deletion_list_filenames.cend()
           or incremental_dump_filename != incremental_dump_filenames.cend())
    {
        ++apply_count;

        if (deletion_list_filename == deletion_list_filenames.cend()) {
            ApplyUpdate(keep_intermediate_files, apply_count, "", *incremental_dump_filename);
            ++incremental_dump_filename;
        } else if (incremental_dump_filename == incremental_dump_filenames.cend()) {
            ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename, "");
            ++deletion_list_filename;
        } else {
            const std::string deletion_list_date(BSZUtil::ExtractDateFromFilenameOrDie(*deletion_list_filename));
            const std::string incremental_dump_date(BSZUtil::ExtractDateFromFilenameOrDie(*incremental_dump_filename));
            if (deletion_list_date < incremental_dump_date) {
                ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename, "");
                ++deletion_list_filename;
            } else if (incremental_dump_date > deletion_list_date) {
                ApplyUpdate(keep_intermediate_files, apply_count, "", *incremental_dump_filename);
                ++incremental_dump_filename;
            } else {
                ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename, *incremental_dump_filename);
                ++deletion_list_filename;
                ++incremental_dump_filename;
            }
        }
    }

    const std::string old_date(BSZUtil::ExtractDateFromFilenameOrDie("../" + old_complete_dump_filename));

    if (not keep_intermediate_files) {
        LOG_DEBUG("deleting \"" + old_complete_dump_filename + "\".");
        DeleteFileOrDie("../" + old_complete_dump_filename);
    }

    LOG_DEBUG("About to exit ExtractAndCombineMarcFilesFromArchives.");
    return CreateNewCompleteMarcArchive(old_date, old_complete_dump_filename, "." + std::to_string(apply_count), tuefind_flavour);
}


void RemoveDirectoryOrDie(const std::string &directory_name) {\
    LOG_DEBUG("about to remove subdirectory \"" + directory_name + "\" and any contained files.");
    if (unlikely(not FileUtil::RemoveDirectory(directory_name)))
        LogSendEmailAndDie("failed to recursively remove \"" + directory_name + "\"! ("  + std::string(::strerror(errno)) + ")");
}


// Merges the filenames of the "incremental_dump_filenames" list into the "incremental_authority_dump_filenames" list.
// If filenames in both lists have the same datestamp, we insert the authority dump filename *before* the
// incremental dump filename.
void MergeAuthorityAndIncrementalDumpLists(const std::vector<std::string> &incremental_authority_dump_filenames,
                                           std::vector<std::string> * const incremental_dump_filenames)
{
    std::vector<std::string> merged_list;

    auto incremental_authority_dump_filename(incremental_authority_dump_filenames.begin());
    auto incremental_dump_filename(incremental_dump_filenames->begin());
    while (incremental_authority_dump_filename != incremental_authority_dump_filenames.end()
           or incremental_dump_filename != incremental_dump_filenames->end())
    {
        if (incremental_authority_dump_filename == incremental_authority_dump_filenames.end()) {
            merged_list.emplace_back(*incremental_dump_filename);
            ++incremental_dump_filename;
        } else if (incremental_dump_filename == incremental_dump_filenames->end()) {
            merged_list.emplace_back(*incremental_authority_dump_filename);
            ++incremental_authority_dump_filename;
        } else {
            const std::string incremental_authority_dump_date(
                BSZUtil::ExtractDateFromFilenameOrDie(*incremental_authority_dump_filename));
            const std::string incremental_dump_date(
                BSZUtil::ExtractDateFromFilenameOrDie(*incremental_dump_filename));
            if (incremental_authority_dump_date < incremental_dump_date) {
                merged_list.emplace_back(*incremental_authority_dump_filename);
                ++incremental_authority_dump_filename;
            } else if (incremental_dump_date < incremental_authority_dump_date) {
                merged_list.emplace_back(*incremental_dump_filename);
                ++incremental_dump_filename;
            } else { // Identical dates.
                merged_list.emplace_back(*incremental_authority_dump_filename);
                ++incremental_authority_dump_filename;
                merged_list.emplace_back(*incremental_dump_filename);
                ++incremental_dump_filename;
            }
        }
    }

    incremental_dump_filenames->swap(merged_list);
}


bool MergeIncrementalDumpFiles_Comparator(const std::string &filename1, const std::string &filename2) {
    const std::string date1(BSZUtil::ExtractDateFromFilenameOrDie(filename1));
    const std::string date2(BSZUtil::ExtractDateFromFilenameOrDie(filename2));

    if (date1 != date2)
        return date1 < date2;

    const bool file1_has_o(filename1.find("_o") != std::string::npos);
    return not file1_has_o;
}


void MergeIncrementalDumpFiles_Sort(std::vector<std::string> * const incremental_dump_filenames) {
    std::sort(incremental_dump_filenames->begin(), incremental_dump_filenames->end(), MergeIncrementalDumpFiles_Comparator);
}


void MergeIncrementalDumpFiles(std::vector<std::string> incremental_dump_filenames,
                               std::vector<std::string> * const merged_incremental_dump_filenames)
{
    MergeIncrementalDumpFiles_Sort(&incremental_dump_filenames);

    auto incremental_dump_filename(incremental_dump_filenames.cbegin());
    while (incremental_dump_filename != incremental_dump_filenames.cend()) {
        const std::string date(BSZUtil::ExtractDateFromFilenameOrDie(*incremental_dump_filename));
        merged_incremental_dump_filenames->emplace_back(
            CombineMarcBiblioArchives(FileUtil::GetFilenameWithoutExtensionOrDie(*incremental_dump_filename), "Merged-" + date));
        ++incremental_dump_filename;

        // We may have had two files that have the same date and only differ in one file having an additional "_o" in its
        // filename. In this case they would have been sorted together and we have to skip over the additional file with
        // the "_o" and the same date:
        if (incremental_dump_filename != incremental_dump_filenames.cend()
            and BSZUtil::ExtractDateFromFilenameOrDie(*(incremental_dump_filename)) == date)
            ++incremental_dump_filename;
    }
}


// Shift a given YYMMDD to ten days before
std::string ShiftDateToTenDaysBefore(const std::string &cutoff_date) {
    struct tm cutoff_date_tm(TimeUtil::StringToStructTm(cutoff_date, "%y%m%d"));
    const time_t cutoff_date_time_t(TimeUtil::TimeGm(cutoff_date_tm));
    if (unlikely(cutoff_date_time_t == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (1)");
    const time_t new_cutoff_date(TimeUtil::AddDays(cutoff_date_time_t, -10));
    if (unlikely(new_cutoff_date == TimeUtil::BAD_TIME_T))
        LOG_ERROR("in ShiftDateToTenDaysBefore: bad time conversion! (2)");
    return TimeUtil::TimeTToString(new_cutoff_date, "%y%m%d");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    bool keep_intermediate_files(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--keep-intermediate-files") != 0)
            Usage();
        keep_intermediate_files = true;
        --argc, ++argv;
    } else if (argc != 2)
        Usage();

    ::default_email_recipient = argv[1];

    const std::string tuefind_flavour(MiscUtil::GetEnv("TUEFIND_FLAVOUR"));

    std::string deletion_list_pattern,  incremental_authority_dump_pattern;
    ProcessIniFiles(&deletion_list_pattern, &incremental_authority_dump_pattern);

    const std::string complete_dump_filename(GetOrGenerateCompleteDumpFile(tuefind_flavour));
    const std::string complete_dump_filename_date(BSZUtil::ExtractDateFromFilenameOrDie(complete_dump_filename));

    std::vector<std::string> deletion_list_filenames;
    GetFilesMoreRecentThanOrEqual(complete_dump_filename_date, deletion_list_pattern, &deletion_list_filenames);
    if (not deletion_list_filenames.empty())
        LOG_INFO("identified " + std::to_string(deletion_list_filenames.size()) + " deletion list filenames for application.");

    const std::string incremental_dump_pattern("(T|W)A-MARC-" + tuefind_flavour + "(_o)?-\\d{6}\\.tar\\.gz");
    std::vector<std::string> incremental_dump_filenames;
    GetFilesMoreRecentThanOrEqual(complete_dump_filename_date, incremental_dump_pattern, &incremental_dump_filenames);
    if (not incremental_dump_filenames.empty())
        LOG_INFO("identified " + std::to_string(incremental_dump_filenames.size()) + " incremental dump filenames for application.");
    std::vector<std::string> merged_incremental_dump_filenames;
    MergeIncrementalDumpFiles(incremental_dump_filenames, &merged_incremental_dump_filenames);

    std::vector<std::string> incremental_authority_dump_filenames;
    // incremental authority dumps are only delivered once a week and a longer span of time must
    // be taken into account
    GetFilesMoreRecentThanOrEqual(ShiftDateToTenDaysBefore(complete_dump_filename_date),
                                  incremental_authority_dump_pattern, &incremental_authority_dump_filenames);
    if (not incremental_authority_dump_filenames.empty())
        LOG_INFO("identified " + std::to_string(incremental_authority_dump_filenames.size())
                 + " authority dump filenames for application.");

    if (deletion_list_filenames.empty() and merged_incremental_dump_filenames.empty()
        and incremental_authority_dump_filenames.empty())
    {
        SendEmail(::progname,
                  "No recent deletion lists, incremental dump filenames and authority dump filenames.\n"
                  "Therefore we have nothing to do!\n", EmailSender::VERY_LOW);
        return EXIT_SUCCESS;
    }

    MergeAuthorityAndIncrementalDumpLists(incremental_authority_dump_filenames, &merged_incremental_dump_filenames);

    CreateAndChangeIntoTheWorkingDirectory();
    const std::string new_complete_dump_filename(
        ExtractAndCombineMarcFilesFromArchives(keep_intermediate_files, tuefind_flavour, complete_dump_filename,
                                               deletion_list_filenames, merged_incremental_dump_filenames));
    FileUtil::ChangeDirectoryOrDie(".."); // Leave the working directory again.

    if (not keep_intermediate_files) {
        RemoveDirectoryOrDie(GetWorkingDirectoryName());
        DeleteFilesOrDie(incremental_dump_pattern);
        DeleteFilesOrDie("^Merged-\\d{6}\\.tar\\.gz$");
        DeleteFilesOrDie(incremental_authority_dump_pattern);
        DeleteFilesOrDie(deletion_list_pattern);
    }

    CreateSymlink(new_complete_dump_filename, "Complete-MARC-" + tuefind_flavour + "-current.tar.gz");

    SendEmail(std::string(::progname) + " (" + GetHostname() + ")",
              "Succeeded in creating the new complete archive \"" + new_complete_dump_filename + "\".\n",
              EmailSender::VERY_LOW);

    return EXIT_SUCCESS;
}
