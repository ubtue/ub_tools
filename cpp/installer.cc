/** \brief A tool for installing IxTheo and KrimDok from scratch on Ubuntu and Centos systems.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \note Compile with   g++ -std=gnu++14 -O3 -o installer installer.cc
 *  \note or             clang++ -std=gnu++11 -Wno-vla-extension -Wno-c++1y-extensions -O3 -o installer installer.cc
 *
 *  \copyright 2016-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "DbConnection.h"
#include "DnsUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "SELinuxUtil.h"
#include "StringUtil.h"
#include "SystemdUtil.h"
#include "Template.h"
#include "VuFind.h"
#include "UBTools.h"
#include "util.h"


/* Somewhere in the middle of the GCC 2.96 development cycle, a mechanism was implemented by which the user can tag likely branch directions and
   expect the blocks to be reordered appropriately.  Define __builtin_expect to nothing for earlier compilers.  */
#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#       define __builtin_expect(x, expected_value) (x)
#endif


#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


[[noreturn]] void Error(const std::string &msg) {
    if (::progname == nullptr)
        std::cerr << "You must set \"progname\" in main() with \"progname = argv[0];\" in oder to use Error().\n";
    else
        std::cerr << ::progname << ": " << msg << '\n';
    std::exit(EXIT_FAILURE);
}


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " --ub-tools-only|(vufind_system_type [--omit-cronjobs] [--omit-systemctl])\n";
    std::cerr << "       where \"vufind_system_type\" must be either \"krimdok\" or \"ixtheo\".\n\n";
    std::exit(EXIT_FAILURE);
}


// Print a log message to the terminal with a bright green background.
void Echo(const std::string &log_message) {
    std::cout << "\x1B" << "[42m--- " << log_message << "\x1B" << "[0m\n";
}


enum VuFindSystemType { KRIMDOK, IXTHEO };


std::string VuFindSystemTypeToString(VuFindSystemType vufind_system_type) {
    if (vufind_system_type == KRIMDOK)
        return "krimdok";
    else if (vufind_system_type == IXTHEO)
        return "ixtheo";
    else
        Error("invalid VuFind system type!");
}


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


// Detect if OS is running inside docker (e.g. if we might have problems to access systemctl)
bool IsDockerEnvironment() {
    return RegexMatcher::Matched("docker", FileUtil::ReadStringFromPseudoFileOrDie("/proc/1/cgroup"));
}


const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");
const std::string VUFIND_DIRECTORY("/usr/local/vufind");
const std::string INSTALLER_DATA_DIRECTORY(UB_TOOLS_DIRECTORY + "/cpp/data/installer");
const std::string INSTALLER_SCRIPTS_DIRECTORY(INSTALLER_DATA_DIRECTORY + "/scripts");


void ChangeDirectoryOrDie(const std::string &new_working_directory) {
    if (::chdir(new_working_directory.c_str()) != 0)
        Error("failed to set the new working directory to \"" + new_working_directory + "\"! ("
              + std::string(::strerror(errno)) + ")");
}


class TemporaryChDir {
    std::string old_working_dir_;
public:
    explicit TemporaryChDir(const std::string &new_working_dir);
    ~TemporaryChDir();
};


TemporaryChDir::TemporaryChDir(const std::string &new_working_dir)
    : old_working_dir_(FileUtil::GetCurrentWorkingDirectory())
{
    ChangeDirectoryOrDie(new_working_dir);
}


TemporaryChDir::~TemporaryChDir() {
    ChangeDirectoryOrDie(old_working_dir_);
}


void GitActivateCustomHooks(const std::string &repository) {
    const std::string original_git_directory(repository + "/.git");
    const std::string original_hooks_directory(original_git_directory + "/hooks");
    const std::string custom_hooks_directory(repository + "/git-config/hooks");

    if (FileUtil::IsDirectory(custom_hooks_directory) and FileUtil::IsDirectory(original_hooks_directory)) {
        Echo("Activating custom git hooks in " + repository);
        FileUtil::RemoveDirectory(original_hooks_directory);
        TemporaryChDir tmp1(original_git_directory);
        FileUtil::CreateSymlink(custom_hooks_directory, "hooks");
    }
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
    const std::string MOUNT_POINT("/mnt/ZE020150/");
    if (not FileUtil::MakeDirectory(MOUNT_POINT))
        Error("failed to create mount point \"" + MOUNT_POINT + "\"!");

    if (FileUtil::IsMountPoint(MOUNT_POINT) or FileUtil::IsDirectory(MOUNT_POINT + "/FID-Entwicklung"))
        Echo("Department drive already mounted");
    else {
        const std::string role_account(vufind_system_type == KRIMDOK ? "qubob15" : "qubob16");
        const std::string password(MiscUtil::GetPassword("Enter password for " + role_account));
        const std::string credentials_file("/root/.smbcredentials");
        if (unlikely(not FileUtil::WriteString(credentials_file, "username=" + role_account + "\npassword=" + password
                                              + "\n")))
            Error("failed to write " + credentials_file + "!");
        if (not FileContainsLineStartingWith("/etc/fstab", "//sn00.zdv.uni-tuebingen.de/ZE020150"))
            FileUtil::AppendStringToFile("/etc/fstab",
                                        "//sn00.zdv.uni-tuebingen.de/ZE020150 " + MOUNT_POINT + " cifs "
                                        "credentials=/root/.smbcredentials,workgroup=uni-tuebingen.de,uid=root,"
                                        "gid=root,vers=1.0,auto 0 0");
        ExecUtil::ExecOrDie("/bin/mount", { MOUNT_POINT });
        Echo("Successfully mounted the department drive.");
    }
}


void AssureMysqlServerIsRunning(const OSSystemType os_system_type) {
    std::unordered_set<unsigned> running_pids;
    switch(os_system_type) {
    case UBUNTU:
        running_pids = ExecUtil::FindActivePrograms("mysqld");
        if (running_pids.size() == 0)
            ExecUtil::Exec(ExecUtil::Which("mysqld"), { "--daemonize" });
        break;
    case CENTOS:
        running_pids = ExecUtil::FindActivePrograms("mysqld");
        if (running_pids.size() == 0) {
            ExecUtil::Exec("/usr/libexec/mariadb-prepare-db-dir", {});
            pid_t pid(ExecUtil::Spawn(ExecUtil::Which("mysqld_safe"), {}));
            ExecUtil::Exec("/usr/libexec/mariadb-wait-ready", { std::to_string(pid) });
        }
    }
}


void MySQLImportFileIfExists(const std::string &sql_file, const std::string &sql_database,
                             const std::string &root_username, const std::string &root_password)
{
    if (FileUtil::Exists(sql_file))
        DbConnection::MySQLImportFile(sql_file, sql_database, root_username, root_password);
}


void CreateUbToolsDatabase(const OSSystemType os_system_type) {
    AssureMysqlServerIsRunning(os_system_type);

    const std::string root_username("root");
    const std::string root_password("");

    IniFile ini_file(DbConnection::DEFAULT_CONFIG_FILE_PATH);
    const auto section(ini_file.getSection("Database"));
    const std::string sql_database(section->getString("sql_database"));
    const std::string sql_username(section->getString("sql_username"));
    const std::string sql_password(section->getString("sql_password"));

    if (not DbConnection::MySQLUserExists(sql_username, root_username, root_password)) {
        std::cout << "creating ub_tools MySQL user\n";
        DbConnection::MySQLCreateUser(sql_username, sql_password, root_username, root_password);
    }

    if (not DbConnection::MySQLDatabaseExists(sql_database, root_username, root_password)) {
        std::cout << "creating ub_tools MySQL database\n";
        DbConnection::MySQLCreateDatabase(sql_database, root_username, root_password);
        DbConnection::MySQLGrantAllPrivileges(sql_database, sql_username, root_username, root_password);
        DbConnection::MySQLImportFile(INSTALLER_DATA_DIRECTORY + "/ub_tools.sql", sql_database, root_username, root_password);
    }
}


void CreateVuFindDatabases(const VuFindSystemType vufind_system_type, const OSSystemType os_system_type) {
    AssureMysqlServerIsRunning(os_system_type);

    const std::string root_username("root");
    const std::string root_password("");

    const std::string sql_database("vufind");
    const std::string sql_username("vufind");
    const std::string sql_password("vufind");

    if (not DbConnection::MySQLDatabaseExists(sql_database, root_username, root_password)) {
        std::cout << "creating " << sql_database << " database\n";
        DbConnection::MySQLCreateDatabase(sql_database, root_username, root_password);
        DbConnection::MySQLCreateUser(sql_username, sql_password, root_username, root_password);
        DbConnection::MySQLGrantAllPrivileges(sql_database, sql_username, root_username, root_password);
        DbConnection::MySQLImportFile(VUFIND_DIRECTORY + "/module/VuFind/sql/mysql.sql", sql_database, root_username, root_password);
        MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/TueFind/sql/mysql.sql", sql_database, root_username, root_password);
        switch(vufind_system_type) {
        case IXTHEO:
            MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/IxTheo/sql/mysql.sql", sql_database, root_username, root_password);
            break;
        case KRIMDOK:
            MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/KrimDok/sql/mysql.sql", sql_database, root_username, root_password);
            break;
        }
    }

    if (vufind_system_type == IXTHEO) {
        const std::string ixtheo_database("ixtheo");
        const std::string ixtheo_username("ixtheo");
        const std::string ixtheo_password("ixtheo");
        if (not DbConnection::MySQLDatabaseExists(ixtheo_database, root_username, root_password)) {
            std::cout << "creating " << ixtheo_database << " database\n";
            DbConnection::MySQLCreateDatabase(ixtheo_database, root_username, root_password);
            DbConnection::MySQLCreateUser(ixtheo_username, ixtheo_password, root_username, root_password);
            DbConnection::MySQLGrantAllPrivileges(ixtheo_database, ixtheo_username, root_username, root_password);
            DbConnection::MySQLImportFile(INSTALLER_DATA_DIRECTORY + "/ixtheo.sql", ixtheo_database, root_username, root_password);
        }
    }
}


void SystemdEnableAndRunUnit(const std::string unit) {
    if (not SystemdUtil::IsUnitAvailable(unit))
        LOG_ERROR(unit + " unit not found in systemd, installation problem?");

    if (not SystemdUtil::IsUnitEnabled(unit))
        SystemdUtil::EnableUnit(unit);

    if (not SystemdUtil::IsUnitRunning(unit))
        SystemdUtil::StartUnit(unit);
}


void InstallSoftwareDependencies(const OSSystemType os_system_type, const std::string vufind_system_type_string,
                                 bool ub_tools_only, bool install_systemctl)
{
    // install / update dependencies
    std::string script;
    if (os_system_type == UBUNTU)
        script = INSTALLER_SCRIPTS_DIRECTORY + "/install_ubuntu_packages.sh";
    else
        script = INSTALLER_SCRIPTS_DIRECTORY + "/install_centos_packages.sh";

    if (ub_tools_only)
        ExecUtil::ExecOrDie(script);
    else
        ExecUtil::ExecOrDie(script, { vufind_system_type_string });

    // check systemd configuration
    if (install_systemctl) {
        std::string apache_unit_name, mysql_unit_name;
        switch(os_system_type) {
        case UBUNTU:
            apache_unit_name = "apache2";
            mysql_unit_name = "mysql";
            break;
        case CENTOS:
            apache_unit_name = "httpd";
            mysql_unit_name = "mariadb";

            if (not FileUtil::Exists("/etc/my.cnf"))
                ExecUtil::ExecOrDie(ExecUtil::Which("mysql_install_db"),
                                    { "--user=mysql", "--ldata=/var/lib/mysql/", "--basedir=/usr" });
            break;
        }

        // we need to make sure that at least mysql is running, to be able to create databases
        if (IsDockerEnvironment())
            AssureMysqlServerIsRunning(os_system_type);
        else if(SystemdUtil::IsAvailable()) {
            SystemdEnableAndRunUnit(apache_unit_name);
            SystemdEnableAndRunUnit(mysql_unit_name);
        }
    }
}


void CreateUsrLocalRun() {
    const std::string USR_LOCAL_RUN("/usr/local/run");
    if (FileUtil::IsDirectory(USR_LOCAL_RUN))
        return;
    if (not FileUtil::MakeDirectory(USR_LOCAL_RUN))
        Error("failed to create \"" + USR_LOCAL_RUN + "\"!");
}


void InstallUBTools(const bool make_install, const OSSystemType os_system_type) {
    // First install iViaCore-mkdep...
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY + "/cpp/lib/mkdep");
    ExecUtil::ExecOrDie(ExecUtil::Which("make"), { "--jobs=4", "install" });

    // ...then create /usr/local/var/lib/tuelib
    if (not FileUtil::Exists(UBTools::GetTuelibPath())) {
        Echo("creating " + UBTools::GetTuelibPath());
        ExecUtil::ExecOrDie(ExecUtil::Which("mkdir"), { "-p", UBTools::GetTuelibPath() });
    }

    const std::string ZOTERO_ENHANCEMENT_MAPS_DIRECTORY(UBTools::GetTuelibPath() + "/zotero-enhancement-maps");
    if (not FileUtil::Exists(ZOTERO_ENHANCEMENT_MAPS_DIRECTORY)) {
        const std::string git_url("https://github.com/ubtue/zotero-enhancement-maps.git");
        ExecUtil::ExecOrDie(ExecUtil::Which("git"), { "clone", git_url, ZOTERO_ENHANCEMENT_MAPS_DIRECTORY });
    }

    // build issn_to_misc_bits.map (else SELinuxUtil cannot verify file permissions)
    const std::string ISSN_TO_MISC_BITS_MAP_PATH(UBTools::GetTuelibPath() + "issn_to_misc_bits.map");
    ExecUtil::ExecOrDie(UB_TOOLS_DIRECTORY + "/cronjobs/combine_issn_to_misc_bits_map.sh");

    // Add SELinux permissions for files we need to access via the Web.
    // Needs to be done exactly for each file, because we might have files with passwords in there!
    if (SELinuxUtil::IsEnabled()) {
        SELinuxUtil::FileContext::AddRecordIfMissing(ISSN_TO_MISC_BITS_MAP_PATH, "httpd_sys_content_t",
                                                     ISSN_TO_MISC_BITS_MAP_PATH);
        SELinuxUtil::FileContext::AddRecordIfMissing(ZOTERO_ENHANCEMENT_MAPS_DIRECTORY, "httpd_sys_content_t",
                                                     ZOTERO_ENHANCEMENT_MAPS_DIRECTORY + "(/.*)?");
    }

    // ...and then install the rest of ub_tools:
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    if (make_install)
        ExecUtil::ExecOrDie(ExecUtil::Which("make"), { "--jobs=4", "install" });
    else
        ExecUtil::ExecOrDie(ExecUtil::Which("make"), { "--jobs=4" });

    CreateUbToolsDatabase(os_system_type);
    GitActivateCustomHooks(UB_TOOLS_DIRECTORY);
    CreateUsrLocalRun();

    Echo("Installed ub_tools.");
}


std::string GetStringFromTerminal(const std::string &prompt) {
    std::cout << prompt << " >";
    std::string input;
    std::getline(std::cin, input);
    return StringUtil::TrimWhite(&input);
}


void InstallCronjobs(const VuFindSystemType vufind_system_type) {
    Template::Map names_to_values_map;
    if (vufind_system_type == IXTHEO) {
        names_to_values_map.insertScalar("ixtheo_host", GetStringFromTerminal("IxTheo Hostname"));
        names_to_values_map.insertScalar("relbib_host", GetStringFromTerminal("RelBib Hostname"));
    }

    FileUtil::AutoTempFile crontab_temp_file_old;
    // crontab -l returns error code if crontab is empty, so dont use ExecUtil::ExecOrDie!!!
    ExecUtil::Exec(ExecUtil::Which("crontab"), { "-l" }, "", crontab_temp_file_old.getFilePath());
    FileUtil::AutoTempFile crontab_temp_file_custom;
    const std::string crontab_block_start = "# START VUFIND AUTOGENERATED";
    const std::string crontab_block_end = "# END VUFIND AUTOGENERATED";
    ExecUtil::ExecOrDie(ExecUtil::Which("sed"),
              { "-e", "/" + crontab_block_start + "/,/" + crontab_block_end + "/d",
                crontab_temp_file_old.getFilePath() }, "", crontab_temp_file_custom.getFilePath());
    const std::string cronjobs_custom(FileUtil::ReadStringOrDie(crontab_temp_file_custom.getFilePath()));

    std::string cronjobs_generated(crontab_block_start + "\n");
    if (vufind_system_type == KRIMDOK)
        cronjobs_generated += FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY + "/krimdok.cronjobs");
    else
        cronjobs_generated += Template::ExpandTemplate(FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY + "/ixtheo.cronjobs"),
                                                       names_to_values_map);
    cronjobs_generated += crontab_block_end + "\n";

    FileUtil::AutoTempFile crontab_temp_file_new;
    FileUtil::AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_generated);
    FileUtil::AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_custom);

    ExecUtil::ExecOrDie(ExecUtil::Which("crontab"), { crontab_temp_file_new.getFilePath() });
    Echo("Installed cronjobs.");
}


// Note: this will also create a group with the same name
void CreateUserIfNotExists(const std::string &username) {
    const int user_exists(ExecUtil::Exec(ExecUtil::Which("id"), { "-u", username }));
    if (user_exists == 1) {
        Echo("Creating user " + username + "...");
        ExecUtil::ExecOrDie(ExecUtil::Which("useradd"), { "--system", "--user-group", "--no-create-home", username });
    } else if (user_exists > 1)
        Error("Failed to check if user exists: " + username);
}


void GenerateXml(const std::string &filename_source, const std::string &filename_target) {
    std::string dirname_source, basename_source;
    FileUtil::DirnameAndBasename(filename_source, &dirname_source, &basename_source);

    Echo("Generating " + filename_target + " from " + basename_source);
    ExecUtil::Exec(ExecUtil::Which("xmllint"), { "--xinclude", "--format", filename_source }, "", filename_target);
}


void GitAssumeUnchanged(const std::string &filename) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecUtil::ExecOrDie(ExecUtil::Which("git"), { "update-index", "--assume-unchanged", filename });
}

void GitCheckout(const std::string &filename) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecUtil::ExecOrDie(ExecUtil::Which("git"), { "checkout", filename });
}

void UseCustomFileIfExists(std::string filename_custom, std::string filename_default) {
    if (FileUtil::Exists(filename_custom)) {
        FileUtil::CreateSymlink(filename_custom, filename_default);
        GitAssumeUnchanged(filename_default);
    } else {
        GitCheckout(filename_default);
    }
}


void DownloadVuFind() {
    if (FileUtil::IsDirectory(VUFIND_DIRECTORY)) {
        Echo("VuFind directory already exists, skipping download");
    } else {
        Echo("Downloading TueFind git repository");
        const std::string git_url("https://github.com/ubtue/tuefind.git");
        ExecUtil::ExecOrDie(ExecUtil::Which("git"), { "clone", git_url, VUFIND_DIRECTORY });
        GitActivateCustomHooks(VUFIND_DIRECTORY);

        TemporaryChDir tmp2(VUFIND_DIRECTORY);
        ExecUtil::ExecOrDie(ExecUtil::Which("composer"), { "install" });
    }
}


/**
 * Configure Apache User
 * - Create user "vufind" as system user if not exists
 * - Grant permissions on relevant directories
 */
void ConfigureApacheUser(const OSSystemType os_system_type) {
    const std::string username("vufind");
    CreateUserIfNotExists(username);

    // systemd will start apache as root
    // but apache will start children as configured in /etc
    std::string config_filename;
    switch (os_system_type) {
    case UBUNTU:
        config_filename = "/etc/apache2/envvars";
        ExecUtil::ExecOrDie(ExecUtil::Which("sed"),
            { "-i", "s/export APACHE_RUN_USER=www-data/export APACHE_RUN_USER=" + username + "/",
              config_filename });

        ExecUtil::ExecOrDie(ExecUtil::Which("sed"),
            { "-i", "s/export APACHE_RUN_GROUP=www-data/export APACHE_RUN_GROUP=" + username + "/",
              config_filename });
        break;
    case CENTOS:
        config_filename = "/etc/httpd/conf/httpd.conf";
        ExecUtil::ExecOrDie(ExecUtil::Which("sed"),
            { "-i", "s/User apache/User " + username + "/", config_filename });

        ExecUtil::ExecOrDie(ExecUtil::Which("sed"),
            { "-i", "s/Group apache/Group " + username + "/", config_filename });
        break;
    }

    ExecUtil::ExecOrDie(ExecUtil::Which("find"),
                        { VUFIND_DIRECTORY + "/local", "-name", "cache", "-exec", "chown", "-R", username + ":" + username, "{}",
                          "+" });
    ExecUtil::ExecOrDie(ExecUtil::Which("chown"), { "-R", username + ":" + username, "/usr/local/var/log/tuefind" });
    if (SELinuxUtil::IsEnabled()) {
        SELinuxUtil::FileContext::AddRecordIfMissing(VUFIND_DIRECTORY + "/local/tuefind/instances/ixtheo/cache",
                                                     "httpd_sys_rw_content_t",
                                                     VUFIND_DIRECTORY + "/local/tuefind/instances/ixtheo/cache(/.*)?");

        SELinuxUtil::FileContext::AddRecordIfMissing(VUFIND_DIRECTORY + "/local/tuefind/instances/relbib/cache",
                                                     "httpd_sys_rw_content_t",
                                                     VUFIND_DIRECTORY + "/local/tuefind/instances/relbib/cache(/.*)?");

        SELinuxUtil::FileContext::AddRecordIfMissing(VUFIND_DIRECTORY + "/local/tuefind/instances/bibstudies/cache",
                                                     "httpd_sys_rw_content_t",
                                                     VUFIND_DIRECTORY + "/local/tuefind/instances/bibstudies/cache(/.*)?");

        SELinuxUtil::FileContext::AddRecordIfMissing(VUFIND_DIRECTORY + "/local/tuefind/instances/krimdok/cache",
                                                     "httpd_sys_rw_content_t",
                                                     VUFIND_DIRECTORY + "/local/tuefind/instances/krimdok/cache(/.*)?");
    }
}


static void GenerateAndInstallVuFindServiceTemplate(const VuFindSystemType system_type, const std::string &service_name) {
    FileUtil::AutoTempDirectory temp_dir;

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("solr_heap", system_type == KRIMDOK ? "4G" : "8G");
    const std::string vufind_service(Template::ExpandTemplate(FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY
                                                                                        + "/" + service_name + ".service.template"),
                                                             names_to_values_map));
    const std::string service_file_path(temp_dir.getDirectoryPath() + "/" + service_name + ".service");
    FileUtil::WriteStringOrDie(service_file_path, vufind_service);
    SystemdUtil::InstallUnit(service_file_path);
}


static void GenerateAndInstallSystemMonitorServiceTemplate(const std::string &service_name) {
    FileUtil::AutoTempDirectory temp_dir;

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("hostname", DnsUtil::GetHostname());
    const std::string system_monitor_service(Template::ExpandTemplate(FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY
                                                                                                + "/" + service_name + ".service.template"),
                                                                      names_to_values_map));
    const std::string service_file_path(temp_dir.getDirectoryPath() + "/" + service_name + ".service");
    FileUtil::WriteStringOrDie(service_file_path, system_monitor_service);
    SystemdUtil::InstallUnit(service_file_path);
}


/**
 * Configure Solr User and SystemD services.
 * - Create user "solr" as system user if not exists
 * - Grant permissions on relevant directories
 * - register solr and system_monitor services in systemd
 */
void ConfigureSolrUserAndService(const VuFindSystemType system_type, const bool install_systemctl) {
    // note: if you wanna change username, don't do it only here, also check vufind.service!
    const std::string USER_AND_GROUP_NAME("solr");
    const std::string VUFIND_SERVICE("vufind");
    const std::string SYSTEM_MONITOR_SERVICE("system_monitor");

    CreateUserIfNotExists(USER_AND_GROUP_NAME);

    Echo("Setting directory permissions for Solr user...");
    ExecUtil::ExecOrDie(ExecUtil::Which("chown"),
                        { "-R", USER_AND_GROUP_NAME + ":" + USER_AND_GROUP_NAME, VUFIND_DIRECTORY + "/solr" });
    ExecUtil::ExecOrDie(ExecUtil::Which("chown"),
                        { "-R", USER_AND_GROUP_NAME + ":" + USER_AND_GROUP_NAME, VUFIND_DIRECTORY + "/import" });

    const std::string solr_security_settings("solr hard nofile 65535\n"
                                             "solr soft nofile 65535\n"
                                             "solr hard nproc 65535\n"
                                             "solr soft nproc 65535\n");
    FileUtil::WriteString("/etc/security/limits.d/20-solr.conf", solr_security_settings);

    // systemctl: we do enable as well as daemon-reload and restart
    // to achieve an idempotent installation
    if (install_systemctl) {
        Echo("Activating " + VUFIND_SERVICE + " service...");
        GenerateAndInstallVuFindServiceTemplate(system_type, VUFIND_SERVICE);
        SystemdEnableAndRunUnit(VUFIND_SERVICE);

        Echo("Activating " + SYSTEM_MONITOR_SERVICE + " service...");
        GenerateAndInstallSystemMonitorServiceTemplate(SYSTEM_MONITOR_SERVICE);
        SystemdEnableAndRunUnit(SYSTEM_MONITOR_SERVICE);
    }
}


void SetEnvironmentVariables(const std::string &vufind_system_type_string) {
    std::vector<std::pair<std::string, std::string>> keys_and_values {
        { "VUFIND_HOME", VUFIND_DIRECTORY },
        { "VUFIND_LOCAL_DIR", VUFIND_DIRECTORY + "/local/tuefind/instances/" + vufind_system_type_string },
        { "TUEFIND_FLAVOUR", vufind_system_type_string },
    };

    std::string variables;
    for (const auto &key_and_value : keys_and_values)
        variables += "export " + key_and_value.first + "=" + key_and_value.second + "\n";

    const std::string vufind_script_path("/etc/profile.d/vufind.sh");
    FileUtil::WriteString(vufind_script_path, variables);
    MiscUtil::LoadExports(vufind_script_path, /* overwrite = */ true);
}


/**
 * Configure VuFind system
 * - Solr Configuration
 * - Schema Fields & Types
 * - solrmarc settings (including VUFIND_LOCAL_DIR)
 * - alphabetical browse
 * - cronjobs
 * - create directories /usr/local/var/log/tuefind
 *
 * Writes a file into vufind directory to save configured system type
 */
void ConfigureVuFind(const VuFindSystemType vufind_system_type, const OSSystemType os_system_type, const bool install_cronjobs,
                     const bool install_systemctl)
{
    const std::string vufind_system_type_string(VuFindSystemTypeToString(vufind_system_type));
    Echo("Starting configuration for " + vufind_system_type_string);
    const std::string dirname_solr_conf = VUFIND_DIRECTORY + "/solr/vufind/biblio/conf";

    Echo("SOLR Configuration (solrconfig.xml)");
    ExecUtil::ExecOrDie(dirname_solr_conf + "/make_symlinks.sh", { vufind_system_type_string });

    Echo("SOLR Schema (schema_local_*.xml)");
    ExecUtil::ExecOrDie(dirname_solr_conf + "/generate_xml.sh", { vufind_system_type_string });

    Echo("solrmarc (marc_local.properties)");
    ExecUtil::ExecOrDie(VUFIND_DIRECTORY + "/import/make_marc_local_properties.sh", { vufind_system_type_string });

    SetEnvironmentVariables(vufind_system_type_string);

    Echo("alphabetical browse");
    UseCustomFileIfExists(VUFIND_DIRECTORY + "/index-alphabetic-browse_" + vufind_system_type_string + ".sh",
                          VUFIND_DIRECTORY + "/index-alphabetic-browse.sh");

    if (install_cronjobs) {
        Echo("cronjobs");
        InstallCronjobs(vufind_system_type);
    }

    Echo("creating log directory");
    ExecUtil::ExecOrDie(ExecUtil::Which("mkdir"), { "-p", "/usr/local/var/log/tuefind" });
    if (SELinuxUtil::IsEnabled()) {
        SELinuxUtil::FileContext::AddRecordIfMissing("/usr/local/var/log/tuefind",
                                                     "httpd_sys_rw_content_t",
                                                     "/usr/local/var/log/tuefind(/.*)?");
    }

    ConfigureSolrUserAndService(vufind_system_type, install_systemctl);
    ConfigureApacheUser(os_system_type);

    Echo(vufind_system_type_string + " configuration completed!");
}


int Main(int argc, char **argv) {
    bool ub_tools_only(false);
    std::string vufind_system_type_string;
    VuFindSystemType vufind_system_type;
    bool omit_cronjobs(false);
    bool omit_systemctl(false);

    if (argc < 2 || argc > 4)
        Usage();

    if (std::strcmp("--ub-tools-only", argv[1]) == 0) {
        ub_tools_only = true;
        if (argc > 2)
            Usage();
    } else {
        vufind_system_type_string = argv[1];
        if (::strcasecmp(vufind_system_type_string.c_str(), "auto") == 0) {
            vufind_system_type_string = VuFind::GetTueFindFlavour();
            if (not vufind_system_type_string.empty())
                std::cout << "using auto-detected tuefind installation type \""
                             + vufind_system_type_string + "\"\n";
            else
                Error("could not auto-detect tuefind installation type");
        }

        if (::strcasecmp(vufind_system_type_string.c_str(), "krimdok") == 0)
            vufind_system_type = KRIMDOK;
        else if (::strcasecmp(vufind_system_type_string.c_str(), "ixtheo") == 0)
            vufind_system_type = IXTHEO;
        else
            Usage();

        if (argc >= 3) {
            for (int i = 2; i <= 3; ++i) {
                if (i < argc) {
                    if (std::strcmp("--omit-cronjobs", argv[i]) == 0)
                        omit_cronjobs = true;
                    else if (std::strcmp("--omit-systemctl", argv[i]) == 0)
                        omit_systemctl = true;
                    else
                        Usage();
                }
            }
        }
    }

    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    const OSSystemType os_system_type(DetermineOSSystemType());

    // Install dependencies before vufind
    // correct PHP version for composer dependancies
    InstallSoftwareDependencies(os_system_type, vufind_system_type_string,
                                ub_tools_only, not omit_systemctl);

    if (not ub_tools_only) {
        MountDeptDriveOrDie(vufind_system_type);
        DownloadVuFind();
        ConfigureVuFind(vufind_system_type, os_system_type, not omit_cronjobs, not omit_systemctl);
    }
    InstallUBTools(/* make_install = */ true, os_system_type);
    if (not ub_tools_only)
        CreateVuFindDatabases(vufind_system_type, os_system_type);

    return EXIT_SUCCESS;
}
