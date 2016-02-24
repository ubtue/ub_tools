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

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include "Compiler.h"
#include "EmailSender.h"
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
    std::cerr << one_line_message << '\n';
    SendEmail(std::string(::progname) + " failed! (from " + GetHostname() + ")",
	      "Please have a look at the log for details.\n", EmailSender::VERY_HIGH);
    std::exit(EXIT_FAILURE);
}


// Populates "filenames" with a list of regular files and returns the number of matching filenames that were found
// in the current working directory.
unsigned GetListOfRegularFiles(const std::string &filename_regex, std::vector<std::string> * const filenames) {
    filenames->clear();

    std::string err_msg;
    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(filename_regex, &err_msg));
    if (unlikely(not err_msg.empty()))
	LogSendEmailAndDie("in GetListOfRegularFiles: bad file regex: \"" + filename_regex +"\".");

    DIR * const directory_stream(::opendir("."));
    if (unlikely(directory_stream == nullptr))
	LogSendEmailAndDie("in GetListOfRegularFiles: opendir(3) failed(" + std::string(::strerror(errno)) + ")");

    struct dirent *entry;
    while ((entry = ::readdir(directory_stream)) != nullptr) {
	if (entry->d_type == DT_REG and matcher->matched(entry->d_name))
	    filenames->emplace_back(entry->d_name);
    }
    ::closedir(directory_stream);

    return filenames->size();
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/cronjobs/handle_partial_updates.conf");


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

	std::vector<std::string> filenames;
	GetListOfRegularFiles(".*\\.cc", &filenames);
	for (const auto &filename : filenames)
	    std::cout << filename << '\n';

	SendEmail(std::string(::progname), "Succeeded.\n", EmailSender::VERY_LOW);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
