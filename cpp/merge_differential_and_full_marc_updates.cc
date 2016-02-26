/** \file    merge_differential_and_full_marc_updates.cc
 *  \brief   A tool for creating combined full updates from an older full update and one or more differential updates.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
loesch_liste    = LOEPPN-\d{6}
komplett_abzug  = SA-MARC-ixtheo-\d{6}.tar.gz
differenz_abzug = TA-MARC-ixtheo-\d{6}.tar.gz

[SMTPServer]
server_address  = smtpserv.uni-tuebingen.de
server_user     = qubob16@uni-tuebingen.de
server_password = vv:*i%Nk
*/

#include <algorithm>
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
#include <sys/types.h>
#include <unistd.h>
#include "Archive.h"
#include "Compiler.h"
#include "EmailSender.h"
#include "File.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " default_email_recipient\n";
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
	      "Please have a look at the log for details.\n", EmailSender::VERY_HIGH);
    std::exit(EXIT_FAILURE);
}


// Populates "filenames" with a list of regular files and returns the number of matching filenames that were found
// in the current working directory.  The list will be sorted in alphanumerical order.
unsigned GetSortedListOfRegularFiles(const std::string &filename_regex, std::vector<std::string> * const filenames) {
    filenames->clear();

    std::string err_msg;
    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(filename_regex, &err_msg));
    if (unlikely(not err_msg.empty()))
	LogSendEmailAndDie("in GetListOfRegularFiles: failed to compile file name regex: \"" + filename_regex
			   + "\". (" + err_msg + ")");

    DIR * const directory_stream(::opendir("."));
    if (unlikely(directory_stream == nullptr))
	LogSendEmailAndDie("in GetListOfRegularFiles: opendir(3) failed(" + std::string(::strerror(errno)) + ")");

    struct dirent *entry;
    while ((entry = ::readdir(directory_stream)) != nullptr) {
	if (entry->d_type == DT_REG and matcher->matched(entry->d_name))
	    filenames->emplace_back(entry->d_name);
    }
    ::closedir(directory_stream);

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


std::string ExtractDateFromFilename(const std::string &filename) {
    static const std::string DATE_EXTRACTION_REGEX(".*(\\d{6}).*");
    static RegexMatcher *matcher;
    if (matcher == nullptr) {
	std::string err_msg;
	matcher = RegexMatcher::RegexMatcherFactory(DATE_EXTRACTION_REGEX, &err_msg);
	if (unlikely(not err_msg.empty()))
	    LogSendEmailAndDie("in ExtractDate: failed to compile regex: \"" + DATE_EXTRACTION_REGEX +"\".");
    }

    if (unlikely(not matcher->matched(filename)))
	LogSendEmailAndDie("in ExtractDate: \"" + filename + "\" failed to match the regex \"" + DATE_EXTRACTION_REGEX + "\"!");

    return (*matcher)[1];
}


void GetFilesMoreRecentThan(const std::string &cutoff_date, const std::string &filename_pattern,
			    std::vector<std::string> * const filenames)
{
    GetSortedListOfRegularFiles(filename_pattern, filenames);

    const auto first_deletion_position(filenames->begin());
    auto last_deletion_position(filenames->begin());
    while (last_deletion_position < filenames->end() and ExtractDateFromFilename(*last_deletion_position) <= cutoff_date)
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
	LogSendEmailAndDie("failed to change directory to \"" + directory + "\"! " + std::string(::strerror(errno)) + ")");
}


void CreateAndChangeIntoTheWorkingDirectory() {
    const std::string working_directory(GetWorkingDirectoryName());
    if (not FileUtil::MakeDirectory(working_directory))
	LogSendEmailAndDie("in CreateAndChangeIntoTheWorkingDirectory failed to create \"" + working_directory + "\"!");

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
    regex_to_first_file_map->emplace(std::shared_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactory(regex_pattern, &err_msg)),
				     archive_entry_name);
    if (unlikely(not err_msg.empty()))
	LogSendEmailAndDie("in GetOutputNameAndMode: failed to compile regex \"" + regex_pattern + "\"! ("
			   + err_msg + ")");

    *disc_filename = archive_entry_name;
    *open_mode = "w"; // create new
}


// Extracts files from a MARC archive, typically a gzipped tar file, and combines files matching the same pattern.
// For example, if the archive contains "SA-MARC-ixtheoa001.raw" and "SA-MARC-ixtheoa002.raw", "SA-MARC-ixtheoa002.raw"
// will be concatenated onto "SA-MARC-ixtheoa001.raw" do that only a single disc file will result.
void ExtractMarcFilesFromArchive(const std::string &archive_name, const std::string &name_prefix = "") {
    std::map<std::shared_ptr<RegexMatcher>, std::string> regex_to_first_file_map;

    ArchiveReader reader(archive_name);
    ArchiveReader::EntryInfo file_info;
    while (reader.getNext(&file_info)) {
	if (unlikely(not file_info.isRegularFile()))
	    LogSendEmailAndDie("in ExtractMarcFilesFromArchive: unexpectedly, the entry \"" + file_info.getFilename()
			       + "\" in \"" + archive_name + "\" is not a regular file!");

	std::string output_filename, open_mode;
	GetOutputNameAndMode(file_info.getFilename(), &regex_to_first_file_map, &output_filename, &open_mode);
	output_filename = name_prefix + output_filename;
	File disc_file(output_filename, open_mode);

	char buf[8192];
	size_t read_count;
	while ((read_count = reader.read(buf, sizeof buf)) > 0) {
	    if (unlikely(disc_file.write(buf, read_count) != read_count))
		LogSendEmailAndDie("in ExtractMarcFilesFromArchive: failed to write data to \"" + output_filename
				   + "\"! (No room?)");
	}
    }
}


// Returns the current date in the DDMMYY format.
std::string GetCurrentDate() {
    const time_t now(std::time(nullptr));
    const struct tm * const local_time(std::localtime(&now));
    char buffer[6 + 1];
    if (unlikely(std::strftime(buffer, sizeof buffer, "%d%m%y", local_time) != 6))
	LogSendEmailAndDie("in GetCurrentDate: strftime(3) failed! (This should never happen!)");

    return buffer;
}


std::string ReplaceString(const std::string &original, const std::string &replacement, const std::string &s) {
    const size_t original_start(s.find(original));
    if (unlikely(original_start == std::string::npos))
	LogSendEmailAndDie("in ReplaceString: can't replace \"" + original + "\" with \"" + replacement + " in \"" + s + "\"!");
    return s.substr(0, original_start) + replacement + s.substr(original_start + original.length());
}


void ApplyUpdate(const std::string &old_full_archive, const std::string &new_full_archive,
		 const std::string &deletion_list_filename, const std::string &differential_archive)
{
    if (deletion_list_filename.empty())
	Log("Applying \"" + differential_archive + "\" to \"" + old_full_archive + "\" in order to create \""
	    + new_full_archive + "\".");
    else if (differential_archive.empty())
	Log("Applying \"" + deletion_list_filename + "\" to \"" + old_full_archive + "\" in order to create \""
	    + new_full_archive + "\".");
    else
	Log("Applying \"" + deletion_list_filename + "\" and \"" + differential_archive
	    + "\" to \"" + old_full_archive + "\" in order to create \"" + new_full_archive + "\".");
}


void RenameOrDie(const std::string &old_filename, const std::string &new_filename) {
    if (unlikely(::rename(old_filename.c_str(), new_filename.c_str()) != 0))
	LogSendEmailAndDie("in RenameOrDie: rename from \"" + old_filename + "\" to \"" + new_filename
			   + "\" failed! (" + std::string(::strerror(errno)) + ")");
}


// Creates a new full MARC archive from an old full archive as well as deltion lists and differential updates.
void ExtractAndCombineMarcFilesFromArchives(const std::string &complete_dump_filename,
					    const std::vector<std::string> &deletion_list_filenames,
					    const std::vector<std::string> &incremental_dump_filenames)
{
    ExtractMarcFilesFromArchive("../" + complete_dump_filename);

    // Construct the name of the new complete archive:
    const std::string current_date(GetCurrentDate());
    const std::string old_archive_date(ExtractDateFromFilename(complete_dump_filename));
    const std::string new_complete_dump_filename(ReplaceString(old_archive_date, current_date, complete_dump_filename));
    if (unlikely(new_complete_dump_filename == complete_dump_filename))
	LogSendEmailAndDie("in ExtractAndCombineMarcFilesFromArchives: new complete MARC dump name \""
			   + new_complete_dump_filename + "\" is the same as the name of the existing complete MARC dump!");
    Log("About to create new complete MARC dump file: " + new_complete_dump_filename);

    // Iterate over the deletion list and incremental dump filename lists and apply one or both as appropriate:
    auto deletion_list_filename(deletion_list_filenames.cbegin());
    auto incremental_dump_filename(incremental_dump_filenames.cbegin());
    std::string old_archive_name(complete_dump_filename);
    unsigned apply_count(0);
    while (deletion_list_filename != deletion_list_filenames.cend()
	   or incremental_dump_filename != incremental_dump_filenames.cend())
    {
	++apply_count;

	std::string temp_archive_name(new_complete_dump_filename + "." + std::to_string(apply_count));
	if (deletion_list_filename == deletion_list_filenames.cend()) {
	    ApplyUpdate(old_archive_name, temp_archive_name, "", *incremental_dump_filename);
	    ++incremental_dump_filename;
	} else if (incremental_dump_filename == incremental_dump_filenames.cend()) {
	    ApplyUpdate(old_archive_name, temp_archive_name, *deletion_list_filename, "");
	    ++deletion_list_filename;
	} else {
	    ApplyUpdate(old_archive_name, temp_archive_name, *deletion_list_filename, *incremental_dump_filename);
	    ++deletion_list_filename;
	    ++incremental_dump_filename;
	}

	old_archive_name = temp_archive_name;
    }
    RenameOrDie(old_archive_name, new_complete_dump_filename);
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/cronjobs/merge_differential_and_full_marc_updates.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
	Usage();

    ::default_email_recipient = argv[1];

    try {
	const IniFile ini_file(CONF_FILE_PATH);
	::email_server_address  = ini_file.getString("SMTPServer", "server_address");
	::email_server_user     = ini_file.getString("SMTPServer", "server_user");
	::email_server_password = ini_file.getString("SMTPServer", "server_password");

	const std::string deletion_list_pattern(ini_file.getString("Files", "deletion_list"));
	const std::string complete_dump_pattern(ini_file.getString("Files", "complete_dump"));
	const std::string incremental_dump_pattern(ini_file.getString("Files", "incremental_dump"));

	const std::string complete_dump_filename(PickCompleteDumpFilename(complete_dump_pattern));
	const std::string complete_dump_filename_date(ExtractDateFromFilename(complete_dump_filename));

	std::vector<std::string> deletion_list_filenames;
	GetFilesMoreRecentThan(complete_dump_filename_date, deletion_list_pattern, &deletion_list_filenames);
	if (not deletion_list_filenames.empty())
	    Log("identified " + std::to_string(deletion_list_filenames.size()) + " deletion list filenames for application.");

	std::vector<std::string> incremental_dump_filenames;
	GetFilesMoreRecentThan(complete_dump_filename_date, incremental_dump_pattern, &incremental_dump_filenames);
	if (not incremental_dump_filenames.empty())
	    Log("identified " + std::to_string(incremental_dump_filenames.size())
		+ " incremental dump filenames for application.");

	if (deletion_list_filenames.empty() and incremental_dump_filenames.empty())
	    SendEmail(std::string(::progname),
		      "No recent deletion lists nor incremental dump filenames.\nTherefore we have nothing to do!\n",
		      EmailSender::VERY_LOW);

	CreateAndChangeIntoTheWorkingDirectory();
	ExtractAndCombineMarcFilesFromArchives(complete_dump_filename, deletion_list_filenames, incremental_dump_filenames);

	SendEmail(std::string(::progname), "Succeeded.\n", EmailSender::VERY_LOW);
    } catch (const std::exception &x) {
	LogSendEmailAndDie("caught exception: " + std::string(x.what()));
    }
}
