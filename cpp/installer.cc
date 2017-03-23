/** \brief A tool for installing IxTheo and KrimDok from scratch on Ubuntu and Centos systems.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <unistd.h>
#include "Compiler.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " vufind_system_type\n";
    std::cerr << "       where \"vufind_system_type\" must be either \"krimdok\" or \"ixtheo\".\n\n";
    std::exit(EXIT_FAILURE);
}


// Print a log message to the terminal with a bright green background.
void Echo(const std::string &log_message) {
    std::cout << "\x1B" << "[42m--- " << log_message << "\x1B" << "[0m\n";
}


enum VuFindSystemType { KRIMDOK, IXTHEO };
enum OSSystemType { UBUNTU, CENTOS };


OSSystemType DetermineOSSystemType() {
    std::string file_contents;
    if (FileUtil::ReadString("/etc/issue", &file_contents)
        and StringUtil::FindCaseInsensitive(file_contents, "ubuntu") != std::string::npos)
        return UBUNTU;
    if (FileUtil::ReadString("/etc/redhat-release", &file_contents)
        and StringUtil::FindCaseInsensitive(file_contents, "centos") != std::string::npos)
        return CENTOS;
    Error("you're probably not on an Ubuntu nor on a CentOS system!");
}


void ExecOrDie(const std::string &command, const std::vector<std::string> &arguments,
               const std::string &new_stdin = "", const std::string &new_stdout = "")
{
    int exit_code;
    if ((exit_code = ExecUtil::Exec(command, arguments, "", new_stdin, new_stdout)) != 0)
        Error("Failed to execute \"" + command + "\"! (exit code was " + std::to_string(exit_code) + ")");
}


const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");


void ChangeDirectoryOrDie(const std::string &new_working_directory) {
    if (::chdir(new_working_directory.c_str()) != 0)
        Error("failed to set the new working directory to \"" + new_working_directory + "\"! ("
              + std::string(::strerror(errno)) + ")");
}


std::string GetPassword(const std::string &prompt) {
    errno = 0;
    const std::string password(::getpass((prompt + " >").c_str()));
    if (errno != 0)
        Error("failed to read the password from the terminal!");

    return password;
}


bool FileContainsLineStartingWith(const std::string &path, const std::string &prefix) {
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(path));
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        if (StringUtil::StartsWith(line, prefix))
            return true;
    }

    return false;
}


void MountDeptDriveOrDie(const VuFindSystemType vufind_system_type) {
    const std::string role_account(vufind_system_type == KRIMDOK ? "qubob15" : "qubob16");
    const std::string password(GetPassword("Enter password for " + role_account));
    const std::string credentials_file("/root/.smbcredentials");
    if (unlikely(not FileUtil::WriteString(credentials_file, "username=" + role_account + "\npassword=" + password
                                           + "\n")))
        Error("failed to write " + credentials_file + "!");
    if (not FileContainsLineStartingWith("/etc/fstab", "//sn00.zdv.uni-tuebingen.de/ZE020150"))
        FileUtil::AppendStringToFile("/etc/fstab",
                                     "//sn00.zdv.uni-tuebingen.de/ZE020150 /mnt/ZE020150 cifs "
                                     "credentials=/root/.smbcredentials,workgroup=uni-tuebingen.de,uid=root,"
                                     "gid=root,auto 0 0");
    ExecOrDie("/bin/mount", { "/mnt/ZE020150/" });
    Echo("Successfully mounted the department drive.");
}


void InstallUBToolsSubmodules() {
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    ExecOrDie(ExecUtil::Which("git"), { "submodule", "update", "--init", "--recursive" });
    Echo("Installed ub_tools submodules.");
}


void InstallUBTools() {
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    ExecOrDie(ExecUtil::Which("make"), { "install" });
    Echo("Installed ub_tools.");
}


static char krimdok_cronjobs[] =
    "PATH=/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin\n"
    "SHELL=/bin/bash\n"
    "BSZ_DATEN=/usr/local/ub_tools/bsz_daten\n"
    "BIN=/usr/local/bin\n"
    "EMAIL=krimdok-team@ub.uni-tuebingen.de\n"
    "LOG_DIR=/var/log/krimdok\n"
    "0 */4 * * * cd \"$BSZ_DATEN\" && \"$BIN/black_box_monitor.py\" \"$EMAIL\" "
        "> $LOG_DIR/black_box_monitor.log 2>&1\n"
    "0 0 * * * $BIN/log_rotate --max-rotation-count 4 $LOG_DIR/*.log\n"
    "0 1 * * * $BIN/full_text_cache_cleaner 100 > $LOG_DIR/full_text_cache_cleaner.log 2>&1\n"
    "0 2 * * * cd \"$BSZ_DATEN\" && \"$BIN/purge_old_data.py\" \"$EMAIL\" > $LOG_DIR/purge_old_data.log 2>&1\n"
    "0 3 * * * cd \"$BSZ_DATEN\" && \"$BIN/fetch_marc_updates.py\" \"$EMAIL\" "
        "> $LOG_DIR/fetch_marc_updates.log 2>&1\n"
    "0 4 * * * cd \"$BSZ_DATEN\" && \"$BIN/merge_differential_and_full_marc_updates\" \"$EMAIL\" "
        "> $LOG_DIR/merge_differential_and_full_marc_updates.log 2>&1\n"
    "0 5 * * * cd \"$BSZ_DATEN\" && \"$BIN/initiate_marc_pipeline.py\" \"$EMAIL\" \"$BIN/krimdok_marc_pipeline.sh\" "
        "> $LOG_DIR/initiate_marc_pipeline.log 2>&1\n"
    "0 21 * * 7 $VUFIND_HOME/solr.sh restart\n"
;


static char ixtheo_cronjobs[] =
    "PATH=/usr/local/bin:/bin:/usr/bin:/sbin:/usr/sbin\n"
    "SHELL=/bin/bash\n"
    "BSZ_DATEN=/usr/local/ub_tools/bsz_daten\n"
    "BIN=/usr/local/bin\n"
    "EMAIL=ixtheo-team@ub.uni-tuebingen.de\n"
    "LOG_DIR=/var/log/krimdok\n"
    "0 */4 * * * cd \"$BSZ_DATEN\" && \"$BIN/black_box_monitor.py\" \"$EMAIL\" "
        "> $LOG_DIR/black_box_monitor.log 2>&1\n"
    "0 0 * * * $BIN/log_rotate --max-rotation-count 4 $LOG_DIR/*.log\n"
    "0 1 * * * $BIN/full_text_cache_cleaner 100 > $LOG_DIR/full_text_cache_cleaner.log 2>&1\n"
    "0 2 * * * cd \"$BSZ_DATEN\" && \"$BIN/purge_old_data.py\" \"$EMAIL\" > $LOG_DIR/purge_old_data.log 2>&1\n"
    "0 3 * * * cd \"$BSZ_DATEN\" && \"$BIN/fetch_marc_updates.py\" \"$EMAIL\" "
        "> $LOG_DIR/fetch_marc_updates.log 2>&1\n"
    "30 3 * * * cd \"$BSZ_DATEN\" && \"$BIN/merge_differential_and_full_marc_updates\" \"$EMAIL\" "
        "> $LOG_DIR/merge_differential_and_full_marc_updates.log 2>&1\n"
    "0 4 * * * cd \"$BSZ_DATEN\" && \"$BIN/create_refterm_file.py\" \"$EMAIL\" 2>&1\n"
    "0 5 * * * cd \"$BSZ_DATEN\" && \"$BIN/initiate_marc_pipeline.py\" \"$EMAIL\" "
        "\"$BIN/ixtheo_marc_pipeline_fifo.sh\" > $LOG_DIR/initiate_marc_pipeline.log 2>&1\n"
    "0 22 * * * $BIN/new_journal_alert ixtheo {ixtheo_host} \"IxTheo Team<ixtheo-noreply@uni-tuebingen.de\" "
        "\"IxTheo Subscriptions\"\n"
    "0 23 * * * $BIN/new_journal_alert relbib {relbib_host} ""\"Relbib Team<relbib-noreply@uni-tuebingen.de>\" "
        "\"RelBib Subscriptions\"\n"
    "0 24 * * * $BIN/update_tad_email_acl.sh \"$EMAIL\"\n"
    "0 21 * * 7 $VUFIND_HOME/solr.sh restart\n"
;


std::string GetStringFromTerminal(const std::string &prompt) {
    std::cout << prompt << " >";
    std::string input;
    std::getline(std::cin, input);
    return StringUtil::TrimWhite(&input);
}


void InstallCronjobs(const VuFindSystemType vufind_system_type) {
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    if (vufind_system_type == IXTHEO) {
        names_to_values_map.insert(
            std::make_pair<std::string, std::vector<std::string>>(
                "ixtheo_host", { GetStringFromTerminal("IxTheo Hostname") }));
        names_to_values_map.insert(
            std::make_pair<std::string, std::vector<std::string>>(
                "relbib_host", { GetStringFromTerminal("RelBib Hostname") }));
    }
    
    FileUtil::AutoTempFile crontab_temp_file1;
    ExecOrDie(ExecUtil::Which("crontab"), { "-l" }, "", crontab_temp_file1.getFilePath());
    FileUtil::AutoTempFile crontab_temp_file2;
    ExecOrDie(ExecUtil::Which("sed"),
              { "-e", "/# START VUFIND AUTOGENERATED/,/# END VUFIND AUTOGENERATE/d",
                crontab_temp_file1.getFilePath(), crontab_temp_file2.getFilePath() });

    std::string cronjobs("# START VUFIND AUTOGENERATED\n");
    if (vufind_system_type == KRIMDOK)
        cronjobs += krimdok_cronjobs;
    else
        cronjobs += MiscUtil::ExpandTemplate(ixtheo_cronjobs, names_to_values_map);
    cronjobs += "# END VUFIND AUTOGENERATE\n";
    FileUtil::AppendStringToFile(crontab_temp_file2.getFilePath(), cronjobs);
    ExecOrDie(ExecUtil::Which("crontab"), { crontab_temp_file2.getFilePath() });
    Echo("Installed cronjobs.");
}


void InstallVuFind(const VuFindSystemType vufind_system_type) {
    const std::string git_url(vufind_system_type == KRIMDOK ? "https://github.com/ubtue/krimdok.git"
                                                            : "https://github.com/ubtue/ixtheo.git");
    ExecOrDie(ExecUtil::Which("git"), { "clone", git_url, UB_TOOLS_DIRECTORY });
    Echo("Downloaded VuFind git repository.");
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    VuFindSystemType vufind_system_type;
    if (::strcasecmp(argv[1], "krimdok") == 0)
        vufind_system_type = KRIMDOK;
    else if (::strcasecmp(argv[1], "ixtheo") == 0)
        vufind_system_type = IXTHEO;
    else
        Error("system type must be either \"krimdok\" or \"ixtheo\"!");

    //const OSSystemType os_system_type(DetermineOSSystemType());

    try {
        MountDeptDriveOrDie(vufind_system_type);
        InstallUBToolsSubmodules();
        InstallUBTools();
        InstallCronjobs(vufind_system_type);
        InstallVuFind(vufind_system_type);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
