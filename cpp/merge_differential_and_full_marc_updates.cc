/** \file    merge_differential_and_full_marc_updates.cc
 *  \brief   A tool for creating combined full updates from an older full update and one or more differential updates.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017 Library of the University of Tübingen

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
deletion_list          = LOEPPN(?:_m)?-\d{6}
complete_dump          = SA-MARC-ixtheo-\d{6}.tar.gz
incremental_dump       = (:?TA-MARC-ixtheo|SA-MARC-ixtheo_o|TA-MARC-ixtheo_o)-\d{6}.tar.gz
complete_dump_linkname = SA-MARC-ixtheo-current.tar.gz
errors_list            = Errors_ixtheo_\d{6}
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
#include <ctime>
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
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--keep-intermediate-files] default_email_recipient\n";
    std::exit(EXIT_FAILURE);
}


namespace {


std::string default_email_recipient;
std::string email_server_address;
std::string email_server_user;
std::string email_server_password;


} // unnamed namespace


std::string GetProgramBasename() {
    static std::string basename;
    if (basename.empty()) {
        std::string dirname;
        FileUtil::DirnameAndBasename(::progname, &dirname, &basename);
    }

    return basename;
}


void Log(const std::string &log_message) {
    std::cerr << GetProgramBasename() << ": " << log_message << '\n';
}


void LogWarning(const std::string &log_message) {
    Log("WARNING: " + log_message);
}


std::string GetHostname() {
    char buf[1024];
    if (unlikely(::gethostname(buf, sizeof buf) != 0))
        Error("gethostname(2) failed! (" + std::string(::strerror(errno)) + ")");
    buf[sizeof(buf) - 1] = '\0';

    return buf;
}


void SendEmail(const std::string &subject, const std::string &message_body, const EmailSender::Priority priority) {
    if (not EmailSender::SendEmail(::email_server_user, ::default_email_recipient, subject, message_body, priority))
        Error("failed to send an email!");
}


void LogSendEmailAndDie(const std::string &one_line_message) {
    Log(one_line_message);
    SendEmail(GetProgramBasename() + " failed! (from " + GetHostname() + ")",
              one_line_message + "\n", EmailSender::VERY_HIGH);
    std::exit(EXIT_FAILURE);
}


// Populates "filenames" with a list of regular files and returns the number of matching filenames that were found
// in the current working directory.  The list will be sorted in alphanumerical order.
unsigned GetSortedListOfRegularFiles(const std::string &filename_regex, std::vector<std::string> * const filenames) {
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


std::string PickCompleteDumpFilename(const std::string &complete_dump_pattern) {
    std::vector<std::string> complete_dump_filenames;
    GetSortedListOfRegularFiles(complete_dump_pattern, &complete_dump_filenames);
    if (unlikely(complete_dump_filenames.empty()))
        LogSendEmailAndDie("did not find a complete MARC dump matching \"" + complete_dump_pattern + "\"!");

    const std::string &chosen_filename(complete_dump_filenames.back());
    Log("picking \"" + chosen_filename + "\" as the complete MARC dump.");

    return chosen_filename;
}


enum CompOutcome { CO_GREATER, CO_SMALLER, CO_EQUAL };


CompOutcome DateCompare(const std::string &filename1, const std::string &filename2) {
    const std::string date1(BSZUtil::ExtractDateFromFilenameOrDie(filename1));
    const std::string date2(BSZUtil::ExtractDateFromFilenameOrDie(filename2));
    if (date1 < date2)
        return CO_SMALLER;
    else if (date1 > date2)
        return CO_GREATER;
    return CO_EQUAL;
}


/** \brief Advances "filename_iter" to where its date is greater or equal to the date of "reference_filename".
 *  \return The date matching if "filename_iter"'s date is the same as the reference date else the empty string.
 */
std::string AdvanceToDate(const std::string &reference_filename, const std::vector<std::string>::const_iterator &end,
                   std::vector<std::string>::const_iterator * const filename_iter)
{
    while (*filename_iter != end) {
        const CompOutcome comp_outcome(DateCompare(reference_filename, **filename_iter));
        if (comp_outcome == CO_EQUAL)
            return **filename_iter;
        if (comp_outcome == CO_SMALLER)
            return "";
        ++*filename_iter;
    }

    return "";
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
        Log("Warning: ignoring " + std::to_string(erase_count) + " files matching \"" + filename_pattern
            + "\" because they are too old for the cut-off date " + cutoff_date + "!");
        filenames->erase(first_deletion_position, last_deletion_position);
    }
}


std::string GetWorkingDirectoryName() {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(::progname, &dirname, &basename);
    return basename + ".working_directory";
}


void ChangeDirectoryOrDie(const std::string &directory) {
    if (unlikely(::chdir(directory.c_str()) != 0))
        LogSendEmailAndDie("failed to change directory to \"" + directory + "\"! " + std::string(::strerror(errno))
                           + ")");
}


void CreateAndChangeIntoTheWorkingDirectory() {
    const std::string working_directory(GetWorkingDirectoryName());
    if (not FileUtil::MakeDirectory(working_directory))
        LogSendEmailAndDie("in CreateAndChangeIntoTheWorkingDirectory failed to create \"" + working_directory
                           + "\"!");

    ChangeDirectoryOrDie(working_directory);
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
// "SA-MARC-ixtheoa002.raw" will be concatenated onto "SA-MARC-ixtheoa001.raw" do that only a single disc file will
// result.
void ExtractMarcFilesFromArchive(const std::string &archive_name, std::vector<std::string> * const extracted_names,
                                 const std::string &name_prefix = "", const std::string &name_suffix = "")
{
    Log("extracting files from archive \"" + archive_name + "\".");
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


// Creates an previously not existing empty file with read and write permission for the owner only.
void CreateEmptyFileOrDie(const std::string &filename) {
    const int fd(::open(filename.c_str(), O_CREAT | O_EXCL, 0600));
    if (unlikely(fd == -1))
        LogSendEmailAndDie("failed to create the empty file \"" + filename + "\"! (" + std::string(::strerror(errno))
                           + ")");
    ::close(fd);
}


void RenameOrDie(const std::string &old_filename, const std::string &new_filename) {
    if (unlikely(::rename(old_filename.c_str(), new_filename.c_str()) != 0))
        LogSendEmailAndDie("in RenameOrDie: rename from \"" + old_filename + "\" to \"" + new_filename
                           + "\" failed! (" + std::string(::strerror(errno)) + ")");
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


// Appends "append_source" onto "append_target".
void AppendFileOrDie(const std::string &append_target, const std::string &append_source) {
    Log("about to append \"" + append_source + "\" onto \"" + append_target + "\".");
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
    Log("about to delete \"" + filename + "\".");
    if (unlikely(::unlink(filename.c_str()) != 0))
        LogSendEmailAndDie("in DeleteFileOrDie: unlink(2) on \"" + filename + "\" failed! ("
                           + std::string(::strerror(errno)) + ")");
}


const std::string DELETE_IDS_COMMAND("/usr/local/bin/delete_ids");
const std::string LOCAL_DELETION_LIST_FILENAME("deletions.list");


void UpdateOneFile(const std::string &old_marc_filename, const std::string &new_marc_filename,
                   const std::string &differential_marc_file)
{
    Log("creating \"" + new_marc_filename + "\" from \"" + old_marc_filename
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
    if (unlikely(not GetMatchingFilename("a001.raw" + suffix + "$", title_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"a001.raw" + suffix + "$\"!");

    if (unlikely(not GetMatchingFilename("b001.raw" + suffix + "$", superior_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"b001.raw" + suffix + "$\"!");

    if (unlikely(not GetMatchingFilename("c001.raw" + suffix + "$", authority_marc_basename)))
        LogSendEmailAndDie("did not find precisely one file matching \"c001.raw" + suffix + "$\"!");
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


/** \brief If "old_name" is non-empty, rename it to "new_name", o/w create an empty file named "new_name". */
void MoveOrCreateFileOrDie(const std::string &old_name, const std::string &new_name) {
    if (old_name.empty()) {
        if (unlikely(not FileUtil::MakeEmpty(new_name)))
            LogSendEmailAndDie("failed to create an empty file named \"" + new_name + "\"! ("
                               + std::string(::strerror(errno)) + ")");
    } else if (unlikely(not FileUtil::RenameFile(old_name, new_name, /* remove_target = */ true))) {
            LogSendEmailAndDie("failed to rename \"" + old_name + "\" to \"" + new_name + "\"! ("
                               + std::string(::strerror(errno)) + ")");
    }
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
        LogWarning("\"" + filename + "\" does not exist!");
        return;
    }

    File input(filename, "r");
    unsigned line_count(0);
    while (not input.eof()) {
        input.getline();
        ++line_count;
    }

    Log("\"" + filename + "\" contains " + std::to_string(line_count) + " lines.");
}


/** \brief Creates an empty file if "pathname" does not exist. */
void IfNotExistsMakeEmptyOrDie(const std::string &pathname) {
    if (not FileUtil::Exists(pathname) and not FileUtil::MakeEmpty(pathname))
        LogSendEmailAndDie("failed to create empty file \"" + pathname + "\"!");
}


const std::string APPEND_MARC_XML_CMD("/usr/local/bin/append_marc_xml");


// Appends "source_filename" to "target_filename".
void AppendMarcXMLOrDie(const std::string &source_filename, const std::string &target_filename) {
    if (unlikely(ExecUtil::Exec(APPEND_MARC_XML_CMD, { source_filename, target_filename }) != 0))
        LogSendEmailAndDie("\"" + APPEND_MARC_XML_CMD + "\" with arguments \"" + source_filename + "\" and \""
                           + target_filename + "\" failed!");
}


const std::string DOWNLOAD_ERROR_RECORDS_SCRIPT("/usr/local/bin/download_error_records.sh");
const std::string BIBLIO_ERROR_RECORDS("biblio_error_records.xml");
const std::string AUTH_ERROR_RECORDS("auth_error_records.xml");


void DownloadErrorRecordsOrDie(const std::string &errors_list_filename) {
    if (errors_list_filename.empty())
        return;

    if (unlikely(ExecUtil::Exec(DOWNLOAD_ERROR_RECORDS_SCRIPT,
                                { errors_list_filename, BIBLIO_ERROR_RECORDS, AUTH_ERROR_RECORDS }) != 0))
        LogSendEmailAndDie("\"" + DOWNLOAD_ERROR_RECORDS_SCRIPT + "\" with arguments \"" + errors_list_filename
                           + "\", \"" + BIBLIO_ERROR_RECORDS +"\" and \"" + AUTH_ERROR_RECORDS + "\" failed!");
}


void ApplyUpdate(const bool keep_intermediate_files, const unsigned apply_count,
                 const std::string &deletion_list_filename, const std::string &errors_list_filename,
                 const std::string &differential_archive)
{
    if (not deletion_list_filename.empty())
        CopyFileOrDie("../" + deletion_list_filename, LOCAL_DELETION_LIST_FILENAME);
    else if (differential_archive.empty())
        LogSendEmailAndDie("in ApplyUpdate: both, \"deletion_list_filename\" and \"differential_archive\" are "
                           "empty strings.  This should never happen!");

    DownloadErrorRecordsOrDie(errors_list_filename);

    // Unpack the differential archive and extract control numbers from its members appending them to the
    // deletion list file:
    if (not differential_archive.empty()) {
        Log("updating the deletion list based on control numbers found in the files contained in the differential "
            "MARC archive.");
        std::vector<std::string> extracted_names;
        ExtractMarcFilesFromArchive("../" + differential_archive, &extracted_names, "diff_");
        for (const auto &extracted_name : extracted_names) {
            Log("Processing \"" + extracted_name
                + "\" in order to extract control numbers to append to the deletion list.");
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
    std::string diff_filename_pattern("diff_.*a001.raw");
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LogWarning("found no match for \"" + diff_filename_pattern + "\" which might match a file extracted from \""
                   + differential_archive + "\"!");
    const std::string new_title_marc_filename(ReplaceSuffix(title_marc_basename, old_name_suffix, new_name_suffix));
    UpdateOneFile(title_marc_basename, new_title_marc_filename, diff_filename);
    if (FileUtil::Exists(BIBLIO_ERROR_RECORDS)) {
        AppendMarcXMLOrDie(BIBLIO_ERROR_RECORDS, new_title_marc_filename);
        Log("Appended \"" + BIBLIO_ERROR_RECORDS + "\" to \"" + new_title_marc_filename + "\".");
    }

    // Update the superior data:
    diff_filename_pattern = "diff_.*b001.raw";
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LogWarning("found no match for \"" + diff_filename_pattern + "\" which might match a file extracted from \""
                   + differential_archive + "\"!");
    UpdateOneFile(superior_marc_basename, ReplaceSuffix(superior_marc_basename, old_name_suffix, new_name_suffix),
                  diff_filename);

    // Update the authority data:
    diff_filename_pattern = "diff_.*c001.raw";
    if (not differential_archive.empty() and not GetMatchingFilename(diff_filename_pattern, &diff_filename))
        LogWarning("found no match for \"" + diff_filename_pattern + "\" which might match a file extracted from \""
                   + differential_archive + "\"!");
    const std::string new_authority_data_marc_filename(ReplaceSuffix(authority_marc_basename, old_name_suffix,
                                                               new_name_suffix));
    UpdateOneFile(authority_marc_basename, new_authority_data_marc_filename, diff_filename);
    if (FileUtil::Exists(AUTH_ERROR_RECORDS)) {
        AppendMarcXMLOrDie(AUTH_ERROR_RECORDS, new_authority_data_marc_filename);
        Log("Appended \"" + AUTH_ERROR_RECORDS + "\" to \"" + new_authority_data_marc_filename + "\".");
    }

    if (not differential_archive.empty() and not keep_intermediate_files)
        DeleteFilesOrDie("diff_.*");

    if (not keep_intermediate_files) {
        DeleteFileOrDie(title_marc_basename);
        DeleteFileOrDie(superior_marc_basename);
        DeleteFileOrDie(authority_marc_basename);
        DeleteFileOrDie(LOCAL_DELETION_LIST_FILENAME);
        if (FileUtil::Exists(BIBLIO_ERROR_RECORDS))
            DeleteFileOrDie(BIBLIO_ERROR_RECORDS);
        if (FileUtil::Exists(AUTH_ERROR_RECORDS))
            DeleteFileOrDie(AUTH_ERROR_RECORDS);
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


// Creates a new full MARC archive from an old full archive as well as deletion lists and differential updates.
std::string ExtractAndCombineMarcFilesFromArchives(const bool keep_intermediate_files,
                                                   const std::string &complete_dump_filename,
                                                   const std::vector<std::string> &deletion_list_filenames,
                                                   const std::vector<std::string> &errors_list_filenames,
                                                   const std::vector<std::string> &incremental_dump_filenames)
{
    std::vector<std::string> extracted_names;
    ExtractMarcFilesFromArchive("../" + complete_dump_filename, &extracted_names,
                                /* name_prefix = */ "", /* name_suffix = */ ".0");

    // Iterate over the deletion list and incremental dump filename lists and apply one or both as appropriate:
    auto deletion_list_filename(deletion_list_filenames.cbegin());
    auto errors_list_filename(errors_list_filenames.cbegin());
    auto incremental_dump_filename(incremental_dump_filenames.cbegin());
    unsigned apply_count(0);
    while (deletion_list_filename != deletion_list_filenames.cend()
           or incremental_dump_filename != incremental_dump_filenames.cend())
    {
        ++apply_count;

        if (deletion_list_filename == deletion_list_filenames.cend()) {
            ApplyUpdate(keep_intermediate_files, apply_count, "",
                        AdvanceToDate(*incremental_dump_filename, errors_list_filenames.cend(),
                                      &errors_list_filename),
                        *incremental_dump_filename);
            ++incremental_dump_filename;
        } else if (incremental_dump_filename == incremental_dump_filenames.cend()) {
            ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename,
                        AdvanceToDate(*deletion_list_filename, errors_list_filenames.cend(), &errors_list_filename),
                        "");
            ++deletion_list_filename;
        } else {
            const std::string deletion_list_date(BSZUtil::ExtractDateFromFilenameOrDie(*deletion_list_filename));
            const std::string incremental_dump_date(BSZUtil::ExtractDateFromFilenameOrDie(*incremental_dump_filename));
            if (deletion_list_date < incremental_dump_date) {
                ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename,
                            AdvanceToDate(*deletion_list_filename, errors_list_filenames.cend(),
                                          &errors_list_filename),
                            "");
                ++deletion_list_filename;
            } else if (incremental_dump_date > deletion_list_date) {
                ApplyUpdate(keep_intermediate_files, apply_count, "",
                            AdvanceToDate(*incremental_dump_filename, errors_list_filenames.cend(),
                                          &errors_list_filename),
                            *incremental_dump_filename);
                ++incremental_dump_filename;
            } else {
                ApplyUpdate(keep_intermediate_files, apply_count, *deletion_list_filename,
                            AdvanceToDate(*incremental_dump_filename, errors_list_filenames.cend(),
                                          &errors_list_filename),
                            *incremental_dump_filename);
                ++deletion_list_filename;
                ++incremental_dump_filename;
            }
        }
    }

    const std::string old_date(BSZUtil::ExtractDateFromFilenameOrDie("../" + complete_dump_filename));

    if (not keep_intermediate_files) {
        Log("deleting \"" + complete_dump_filename + "\".");
        DeleteFileOrDie("../" + complete_dump_filename);
    }

    // Create new complete MARC archive:
    const std::string current_date(GetCurrentDate());
    const std::string new_complete_dump_filename(ReplaceStringOrDie(old_date, current_date, complete_dump_filename));
    Log("creating new MARC archive \"" + new_complete_dump_filename + "\".");
    const std::string filename_suffix("." + std::to_string(apply_count));
    std::vector<std::string> updated_marc_files;
    FileUtil::GetFileNameList("[abc]00.\\.raw\\" + filename_suffix, &updated_marc_files);
    ArchiveWriter archive_writer("../" + new_complete_dump_filename);
    for (const auto &updated_marc_file : updated_marc_files) {
        const std::string archive_member_name(RemoveFileNameSuffix(updated_marc_file, filename_suffix));
        Log("Storing \"" + updated_marc_file + "\" as \"" + archive_member_name + "\" in \""
            + new_complete_dump_filename + "\".");
        archive_writer.add(updated_marc_file, archive_member_name);
    }

    return new_complete_dump_filename;
}


void RemoveDirectoryOrDie(const std::string &directory_name) {\
    Log("about to remove subdirectory \"" + directory_name + "\" and any contained files.");
    if (unlikely(not FileUtil::RemoveDirectory(directory_name)))
        LogSendEmailAndDie("failed to recursively remove \"" + directory_name + "\"! ("
                           + std::string(::strerror(errno)) + ")");
}


const std::string EMAIL_CONF_FILE_PATH("/usr/local/var/lib/tuelib/cronjobs/smtp_server.conf");
const std::string CONF_FILE_PATH("/usr/local/var/lib/tuelib/cronjobs/merge_differential_and_full_marc_updates.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    bool keep_intermediate_files(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--keep-intermediate-files") != 0)
            Usage();
        keep_intermediate_files = true;
        --argc, ++argv;
    } else if (argc != 2)
        Usage();

    ::default_email_recipient = argv[1];

    try {
        const IniFile email_ini_file(EMAIL_CONF_FILE_PATH);
        ::email_server_address  = email_ini_file.getString("SMTPServer", "server_address");
        ::email_server_user     = email_ini_file.getString("SMTPServer", "server_user");
        ::email_server_password = email_ini_file.getString("SMTPServer", "server_password");

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string deletion_list_pattern(ini_file.getString("Files", "deletion_list"));
        const std::string complete_dump_pattern(ini_file.getString("Files", "complete_dump"));
        const std::string incremental_dump_pattern(ini_file.getString("Files", "incremental_dump"));
        const std::string complete_dump_linkname(ini_file.getString("Files", "complete_dump_linkname"));
        // An error list is not unconditionally present
        std::string errors_list_pattern;
        ini_file.lookup("Files", "error_list", &errors_list_pattern);

        const std::string complete_dump_filename(PickCompleteDumpFilename(complete_dump_pattern));
        const std::string complete_dump_filename_date(BSZUtil::ExtractDateFromFilenameOrDie(complete_dump_filename));

        std::vector<std::string> deletion_list_filenames;
        GetFilesMoreRecentThanOrEqual(complete_dump_filename_date, deletion_list_pattern, &deletion_list_filenames);
        if (not deletion_list_filenames.empty())
            Log("identified " + std::to_string(deletion_list_filenames.size())
                + " deletion list filenames for application.");

        std::vector<std::string> errors_list_filenames;
        if (not errors_list_pattern.empty()) {
            GetFilesMoreRecentThanOrEqual(complete_dump_filename_date, errors_list_pattern, &errors_list_filenames);
            if (not errors_list_filenames.empty())
                Log("identified " + std::to_string(errors_list_filenames.size())
                    + " errors list filenames for application.");
        }

        std::vector<std::string> incremental_dump_filenames;
        GetFilesMoreRecentThanOrEqual(complete_dump_filename_date, incremental_dump_pattern,
                                      &incremental_dump_filenames);
        if (not incremental_dump_filenames.empty())
            Log("identified " + std::to_string(incremental_dump_filenames.size())
                + " incremental dump filenames for application.");

        if (deletion_list_filenames.empty() and incremental_dump_filenames.empty()) {
            SendEmail(std::string(::progname),
                      "No recent deletion lists nor incremental dump filenames.\nTherefore we have nothing to do!\n",
                      EmailSender::VERY_LOW);
            return 0;
        }

        CreateAndChangeIntoTheWorkingDirectory();
        const std::string new_complete_dump_filename(
            ExtractAndCombineMarcFilesFromArchives(keep_intermediate_files, complete_dump_filename,
                                                   deletion_list_filenames, errors_list_filenames,
                                                   incremental_dump_filenames));
        ChangeDirectoryOrDie(".."); // Leave the working directory again.

        if (not keep_intermediate_files) {
            RemoveDirectoryOrDie(GetWorkingDirectoryName());
            DeleteFilesOrDie(incremental_dump_pattern);
            DeleteFilesOrDie(deletion_list_pattern);
            if (not errors_list_pattern.empty())  
                DeleteFilesOrDie(errors_list_pattern);
        }

        CreateSymlink(new_complete_dump_filename, complete_dump_linkname);

        SendEmail(std::string(::progname) + " (" + GetHostname() + ")",
                  "Succeeded in creating the new complete archive \"" + new_complete_dump_filename + "\".\n",
                  EmailSender::VERY_LOW);
    } catch (const std::exception &x) {
        LogSendEmailAndDie("caught exception: " + std::string(x.what()));
    }
}
