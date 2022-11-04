/** \brief A tool for installing IxTheo and KrimDok from scratch on Ubuntu systems.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <vector>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "AppArmorUtil.h"
#include "DbConnection.h"
#include "DnsUtil.h"
#include "Downloader.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "Solr.h"
#include "StringUtil.h"
#include "SystemdUtil.h"
#include "Template.h"
#include "UBTools.h"
#include "VuFind.h"
#include "util.h"


/* Somewhere in the middle of the GCC 2.96 development cycle, a mechanism was implemented by which the user can tag likely branch directions
   and expect the blocks to be reordered appropriately.  Define __builtin_expect to nothing for earlier compilers.  */
#if __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif


#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)


[[noreturn]] void Error(const std::string &msg) {
    std::cerr << ::progname << ": " << msg << '\n';
    std::exit(EXIT_FAILURE);
}


[[noreturn]] void Usage() {
    ::Usage(
        "<system_type> [<options>]\n"
        "    invocation modes:\n"
        "        ub-tools-only\n"
        "        fulltext-backend (--test|--production) [--omit-cronjobs] [--omit-systemctl]\n"
        "        vufind (ixtheo|krimdok) (--test|--production) [--omit-cronjobs] [--omit-systemctl]\n");
}


// Print a log message to the terminal with a bright green background.
void Echo(const std::string &log_message) {
    std::cout << "\x1B"
              << "[42m--- " << log_message << "\x1B"
              << "[0m\n";
}


enum InstallationType { UB_TOOLS_ONLY, FULLTEXT_BACKEND, VUFIND };
enum VuFindSystemType { KRIMDOK, IXTHEO };


std::string VuFindSystemTypeToString(VuFindSystemType vufind_system_type) {
    if (vufind_system_type == KRIMDOK)
        return "krimdok";
    else if (vufind_system_type == IXTHEO)
        return "ixtheo";
    else
        Error("invalid VuFind system type!");
}


// Detect if OS is running inside docker (e.g. if we might have problems to access systemctl)
bool IsDockerEnvironment() {
    return RegexMatcher::Matched("docker", FileUtil::ReadStringFromPseudoFileOrDie("/proc/1/cgroup"));
}


const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");
const std::string VUFIND_DIRECTORY("/usr/local/vufind");
const std::string VUFIND_LOCAL_OVERRIDES_DIRECTORY(VUFIND_DIRECTORY + "/local/tuefind/local_overrides");
const std::string INSTALLER_DATA_DIRECTORY(UB_TOOLS_DIRECTORY + "/cpp/data/installer");
const std::string INSTALLER_SCRIPTS_DIRECTORY(INSTALLER_DATA_DIRECTORY + "/scripts");


void ChangeDirectoryOrDie(const std::string &new_working_directory) {
    if (::chdir(new_working_directory.c_str()) != 0)
        Error("failed to set the new working directory to \"" + new_working_directory + "\"! (" + std::string(::strerror(errno)) + ")");
}


class TemporaryChDir {
    std::string old_working_dir_;

public:
    explicit TemporaryChDir(const std::string &new_working_dir);
    ~TemporaryChDir();
};


TemporaryChDir::TemporaryChDir(const std::string &new_working_dir): old_working_dir_(FileUtil::GetCurrentWorkingDirectory()) {
    ChangeDirectoryOrDie(new_working_dir);
}


TemporaryChDir::~TemporaryChDir() {
    ChangeDirectoryOrDie(old_working_dir_);
}


void GitActivateCustomHooks(const std::string &repository) {
    Echo("Installer -> git activate custom hooks");
    const std::string original_git_directory(repository + "/.git");
    const std::string original_hooks_directory(original_git_directory + "/hooks");
    const std::string custom_hooks_directory(repository + "/git-config/hooks");

    if (FileUtil::IsDirectory(custom_hooks_directory) and FileUtil::IsDirectory(original_hooks_directory)) {
        Echo("Installer -> Activating custom git hooks in " + repository);
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


bool FileEndsWith(const std::string &path, const std::string &suffix) {
    return StringUtil::EndsWith(FileUtil::ReadStringOrDie(path), suffix);
}


struct Mountpoint {
    std::string path_;
    std::string test_path_;
    std::string unc_path_;

public:
    explicit Mountpoint(const std::string &path, const std::string &test_path, const std::string &unc_path)
        : path_(path), test_path_(test_path), unc_path_(unc_path) { }
};


void MountDeptDriveAndInstallSSHKeysOrDie(const VuFindSystemType vufind_system_type) {
    std::vector<Mountpoint> mount_points;
    Echo("Installer -> mount dept drive and install ssh key");
    mount_points.emplace_back(Mountpoint("/mnt/ZE020150", "/mnt/ZE020150/FID-Entwicklung", "//sn00.zdv.uni-tuebingen.de/ZE020150"));
    mount_points.emplace_back(Mountpoint("/mnt/ZE020110", "/mnt/ZE020110/FID-Projekte", "//sn00.zdv.uni-tuebingen.de/ZE020110"));

    for (const auto &mount_point : mount_points) {
        FileUtil::MakeDirectoryOrDie(mount_point.path_, /*recursive = */ true);
        if (FileUtil::IsMountPoint(mount_point.path_) or FileUtil::IsDirectory(mount_point.test_path_))
            Echo("Mount point already mounted: " + mount_point.path_);
        else {
            const std::string credentials_file("/root/.smbcredentials");
            if (not FileUtil::Exists(credentials_file)) {
                const std::string role_account(vufind_system_type == KRIMDOK ? "qubob15" : "qubob16");
                const std::string password(MiscUtil::GetPassword("Enter password for " + role_account));
                if (unlikely(not FileUtil::WriteString(credentials_file, "username=" + role_account + "\npassword=" + password + "\n")))
                    Error("failed to write " + credentials_file + "!");
            }
            if (not FileContainsLineStartingWith("/etc/fstab", mount_point.unc_path_)) {
                std::string appendix;
                if (not FileEndsWith("/etc/fstab", "\n"))
                    appendix = "\n";

                appendix += mount_point.unc_path_ + " " + mount_point.path_ + " cifs "
                            "credentials=/root/.smbcredentials,workgroup=uni-tuebingen.de,uid=root,"
                            "gid=root,auto 0 0";
                FileUtil::AppendStringToFile("/etc/fstab", appendix);
            }
            ExecUtil::ExecOrDie("/bin/mount", { mount_point.path_ });
            Echo("Successfully mounted " + mount_point.path_);
        }
    }

    const std::string SSH_KEYS_DIR_REMOTE("/mnt/ZE020150/FID-Entwicklung/");
    const std::string SSH_KEYS_DIR_LOCAL("/root/.ssh/");
    const std::string GITHUB_ROBOT_PRIVATE_KEY_REMOTE(SSH_KEYS_DIR_REMOTE + "github-robot");
    const std::string GITHUB_ROBOT_PRIVATE_KEY_LOCAL(SSH_KEYS_DIR_LOCAL + "github-robot");
    const std::string GITHUB_ROBOT_PUBLIC_KEY_REMOTE(SSH_KEYS_DIR_REMOTE + "github-robot.pub");
    const std::string GITHUB_ROBOT_PUBLIC_KEY_LOCAL(SSH_KEYS_DIR_LOCAL + "github-robot.pub");
    if (not FileUtil::Exists(SSH_KEYS_DIR_LOCAL))
        FileUtil::MakeDirectoryOrDie(SSH_KEYS_DIR_LOCAL, false, 0700);
    if (not FileUtil::Exists(GITHUB_ROBOT_PRIVATE_KEY_LOCAL)) {
        FileUtil::CopyOrDieXFs(GITHUB_ROBOT_PRIVATE_KEY_REMOTE, GITHUB_ROBOT_PRIVATE_KEY_LOCAL);
        FileUtil::ChangeModeOrDie(GITHUB_ROBOT_PRIVATE_KEY_LOCAL, 0600);
    }
    if (not FileUtil::Exists(GITHUB_ROBOT_PUBLIC_KEY_LOCAL)) {
        FileUtil::CopyOrDieXFs(GITHUB_ROBOT_PUBLIC_KEY_REMOTE, GITHUB_ROBOT_PUBLIC_KEY_LOCAL);
        FileUtil::ChangeModeOrDie(GITHUB_ROBOT_PUBLIC_KEY_LOCAL, 0600);
    }
}


void AssureMysqlServerIsRunning() {
    std::unordered_set<unsigned> running_pids;
    std::string mysql_sock_path;

    mysql_sock_path = "/var/run/mysqld/mysqld.sock";
    if (SystemdUtil::IsAvailable())
        SystemdUtil::StartUnit("mysql");
    else {
        running_pids = ExecUtil::FindActivePrograms("mysqld");
        if (running_pids.size() == 0)
            ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("mysqld"), { "--daemonize" });
    }

    const unsigned TIMEOUT(30); // seconds
    if (not FileUtil::WaitForFile(mysql_sock_path, TIMEOUT, 5 /*seconds*/))
        Error("Installer -> can't find " + mysql_sock_path + " after " + std::to_string(TIMEOUT) + " seconds of looking!");
}


void MySQLImportFileIfExists(const std::string &sql_file, const std::string &sql_database, const std::string &root_username,
                             const std::string &root_password) {
    if (FileUtil::Exists(sql_file))
        DbConnection::MySQLImportFile(sql_file, sql_database, root_username, root_password);
}


void CreateUbToolsDatabase(DbConnection * const db_connection_root) {
    IniFile ini_file(DbConnection::DEFAULT_CONFIG_FILE_PATH);
    const auto section(ini_file.getSection("Database"));
    const std::string sql_database(section->getString("sql_database"));
    const std::string sql_username(section->getString("sql_username"));
    const std::string sql_password(section->getString("sql_password"));

    Echo("Installer -> Trying database connection setting");
    db_connection_root->mySQLCreateUserIfNotExists(sql_username, sql_password);
    if (not db_connection_root->mySQLDatabaseExists(sql_database)) {
        Echo("Intaller -> Creating ub_tools MySQL database");
        db_connection_root->mySQLCreateDatabase(sql_database);
        db_connection_root->mySQLGrantAllPrivileges(sql_database, sql_username);
        db_connection_root->mySQLGrantAllPrivileges(sql_database + "_tmp", sql_username);
        db_connection_root->mySQLGrantGrantOption(sql_database, sql_username);

        Echo("Installer -> Trying to import database from sql file: " + INSTALLER_DATA_DIRECTORY + "/ub_tools.sql");
        DbConnection::MySQLImportFile(INSTALLER_DATA_DIRECTORY + "/ub_tools.sql", sql_database, sql_username, sql_password);
    }
}


void CreateVuFindDatabases(const VuFindSystemType vufind_system_type, DbConnection * const db_connection_root) {
    const std::string sql_database("vufind");
    const std::string sql_username("vufind");
    const std::string sql_password("vufind");

    IniFile ub_tools_ini_file(DbConnection::DEFAULT_CONFIG_FILE_PATH);
    const auto ub_tools_ini_section(ub_tools_ini_file.getSection("Database"));
    const std::string ub_tools_username(ub_tools_ini_section->getString("sql_username"));
    Echo("Installer -> Create vufind databases");
    db_connection_root->mySQLCreateUserIfNotExists(sql_username, sql_password);
    if (not db_connection_root->mySQLDatabaseExists(sql_database)) {
        Echo("Installer -> Creating " + sql_database + " database");
        Echo("Installer -> db: " + sql_database + " , user: " + sql_username);
        db_connection_root->mySQLCreateDatabase(sql_database);
        Echo("Installer -> mysql grant privileges");
        db_connection_root->mySQLGrantAllPrivileges(sql_database, sql_username);
        db_connection_root->mySQLGrantAllPrivileges(sql_database, ub_tools_username);
        db_connection_root->mySQLGrantGrantOption(sql_database, ub_tools_username);

        Echo("Installer -> Importing vufind db");
        DbConnection::MySQLImportFile(VUFIND_DIRECTORY + "/module/VuFind/sql/mysql.sql", sql_database, sql_username, sql_password);
        MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/TueFind/sql/mysql.sql", sql_database, sql_username, sql_password);
        Echo("Installer -> Importing tuefind");
        switch (vufind_system_type) {
        case IXTHEO:
            Echo("Installer -> Importing ixtheo");
            MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/IxTheo/sql/mysql.sql", sql_database, sql_username, sql_password);
            break;
        case KRIMDOK:
            Echo("Installer -> Importing krimdok");
            MySQLImportFileIfExists(VUFIND_DIRECTORY + "/module/KrimDok/sql/mysql.sql", sql_database, sql_username, sql_password);
            break;
        }
    }

    if (vufind_system_type == IXTHEO) {
        IniFile translations_ini_file(UBTools::GetTuelibPath() + "translations.conf");
        const auto translations_ini_section(translations_ini_file.getSection("Database"));
        const std::string ixtheo_database(translations_ini_section->getString("sql_database"));
        const std::string ixtheo_username(translations_ini_section->getString("sql_username"));
        const std::string ixtheo_password(translations_ini_section->getString("sql_password"));
        Echo("Installer -> vufind system, checking database connection setting");
        db_connection_root->mySQLCreateUserIfNotExists(ixtheo_username, ixtheo_password);
        if (not db_connection_root->mySQLDatabaseExists(ixtheo_database)) {
            Echo("Installer -> Creating " + ixtheo_database + " database");
            db_connection_root->mySQLCreateDatabase(ixtheo_database);
            db_connection_root->mySQLGrantAllPrivileges(ixtheo_database, ixtheo_username);
            db_connection_root->mySQLGrantAllPrivileges(ixtheo_database, sql_username);
            db_connection_root->mySQLGrantAllPrivileges(ixtheo_database, ub_tools_username);
            db_connection_root->mySQLGrantGrantOption(ixtheo_database, ub_tools_username);
            DbConnection::MySQLImportFile(INSTALLER_DATA_DIRECTORY + "/ixtheo.sql", ixtheo_database, ixtheo_username, ixtheo_password);
        }
    }
}


void SystemdEnableAndRunUnit(const std::string unit) {
    if (not SystemdUtil::IsUnitAvailable(unit))
        LOG_ERROR("Installer -> " + unit + " unit not found in systemd, installation problem?");

    if (not SystemdUtil::IsUnitEnabled(unit)) {
        Echo("Installer -> Enabling system unit");
        SystemdUtil::EnableUnit(unit);
    }
    if (not SystemdUtil::IsUnitRunning(unit)) {
        Echo("Installer -> Starting the system unit");
        SystemdUtil::StartUnit(unit);
    }
}


void InstallSoftwareDependencies(const std::string vufind_system_type_string, const InstallationType installation_type,
                                 const bool install_systemctl) {
    // install / update dependencies
    Echo("Installer -> Install software dependencies from: " + INSTALLER_SCRIPTS_DIRECTORY + "/install_ubuntu_packages.sh");
    std::string script(INSTALLER_SCRIPTS_DIRECTORY + "/install_ubuntu_packages.sh");

    if (installation_type == UB_TOOLS_ONLY) {
        Echo("Installer -> running script for UBTools only");
        ExecUtil::ExecOrDie(script);
    } else if (installation_type == FULLTEXT_BACKEND) {
        Echo("Installer -> running script for fulltext backend");
        ExecUtil::ExecOrDie(script, { "fulltext_backend" });
    } else if (installation_type == VUFIND) {
        Echo("Installer -> running script with special param for vufind");
        ExecUtil::ExecOrDie(script, { vufind_system_type_string });
    }

    // check systemd configuration
    if (install_systemctl) {
        Echo("Installer -> starting systemctl for Apache2 and MySQL");
        std::string apache_unit_name("apache2");
        std::string mysql_unit_name("mysql");
        SystemdEnableAndRunUnit(apache_unit_name);
        SystemdEnableAndRunUnit(mysql_unit_name);
    }
}


void RegisterSystemUpdateVersion() {
    Echo("Installer -> registering system update version");
    const std::string SYSTEM_UPDATES_DIRECTORY(UB_TOOLS_DIRECTORY + "/cpp/data/system_updates");
    FileUtil::Directory directory(SYSTEM_UPDATES_DIRECTORY, "(^\\d+\\.sh$|\\d+\\.(?:.*)\\.sql)");
    unsigned max_version(99);
    for (const auto &update_script : directory) {
        const auto &script_name(update_script.getName());
        const auto script_version(StringUtil::ToUnsignedOrDie(script_name.substr(0, script_name.find('.'))));
        if (script_version > max_version)
            max_version = script_version;
    }

    const std::string VERSION_PATH(UBTools::GetTuelibPath() + "system_version");
    FileUtil::WriteStringOrDie(VERSION_PATH, std::to_string(max_version));
}


static void GenerateAndInstallVuFindServiceTemplate(const VuFindSystemType system_type, const std::string &service_name) {
    Echo("Installer -> generating and install vufind service template");
    FileUtil::AutoTempDirectory temp_dir;

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("solr_heap", system_type == KRIMDOK ? "4G" : "8G");
    // names_to_values_map.insertScalar("solr_heap", system_type == KRIMDOK ? "4G" : "4G");
    const std::string vufind_service(Template::ExpandTemplate(
        FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY + "/" + service_name + ".service.template"), names_to_values_map));
    const std::string service_file_path(temp_dir.getDirectoryPath() + "/" + service_name + ".service");
    Echo("Installer -> writing vufind service file.");
    FileUtil::WriteStringOrDie(service_file_path, vufind_service);
    Echo("Installer -> installing vufind servcie");
    SystemdUtil::InstallUnit(service_file_path);
    Echo("Installer -> enabling vufind service.");
    SystemdUtil::EnableUnit(service_name);
}


void SetupSysLog() {
    // Skip this if we are in docker environment
    if (IsDockerEnvironment())
        return;

    Echo("Installer -> setup syslog");
    // logfile for zts docker container
    const std::string ZTS_LOGFILE(UBTools::GetTueFindLogPath() + "/zts.log");
    FileUtil::TouchFileOrDie(ZTS_LOGFILE);

    // logfile for ub_tools programs using the SysLog class
    const std::string UB_TOOLS_LOGFILE(UBTools::GetTueFindLogPath() + "/syslog.log");
    FileUtil::TouchFileOrDie(UB_TOOLS_LOGFILE);

    FileUtil::ChangeOwnerOrDie(ZTS_LOGFILE, "syslog", "adm");
    FileUtil::ChangeOwnerOrDie(UB_TOOLS_LOGFILE, "syslog", "adm");

    FileUtil::CopyOrDie(INSTALLER_DATA_DIRECTORY + "/syslog.zts.conf", "/etc/rsyslog.d/30-zts.conf");
    FileUtil::CopyOrDie(INSTALLER_DATA_DIRECTORY + "/syslog.ub_tools.conf", "/etc/rsyslog.d/40-ub_tools.conf");
}

void SetupSudo() {
    Echo("Installer -> setup sudo");
    FileUtil::CopyOrDie(INSTALLER_DATA_DIRECTORY + "/sudo.zts-restart", "/etc/sudoers.d/99-zts_restart");
    FileUtil::CopyOrDie(INSTALLER_DATA_DIRECTORY + "/sudo.alphabrowse_index_ramdisk", "/etc/sudoers.d/99-alphabrowse_index_ramdisk");
}


void InstallUBTools(const bool make_install, DbConnection * const db_connection_root) {
    Echo("Installer -> install UBTools");
    // First install iViaCore-mkdep...
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY + "/cpp/lib/mkdep");
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("make"), { "--jobs=4", "install" });

    // ...then create /usr/local/var/lib/tuelib
    if (not FileUtil::Exists(UBTools::GetTuelibPath())) {
        Echo("Installer -> creating " + UBTools::GetTuelibPath());
        FileUtil::MakeDirectoryOrDie(UBTools::GetTuelibPath(), /* recursive = */ true);
    }

    // ..and /usr/local/var/log/tuefind
    if (not FileUtil::Exists(UBTools::GetTueFindLogPath())) {
        Echo("Installer -> creating " + UBTools::GetTueFindLogPath());
        FileUtil::MakeDirectoryOrDie(UBTools::GetTueFindLogPath(), /* recursive = */ true);
    }

    // ..and /usr/local/var/tmp
    if (not FileUtil::Exists(UBTools::GetTueLocalTmpPath())) {
        Echo("Installer -> creating " + UBTools::GetTueLocalTmpPath());
        FileUtil::MakeDirectoryOrDie(UBTools::GetTueLocalTmpPath(), /* recursive = */ true);
    }

    const std::string ZOTERO_ENHANCEMENT_MAPS_DIRECTORY(UBTools::GetTuelibPath() + "zotero-enhancement-maps");
    if (not FileUtil::Exists(ZOTERO_ENHANCEMENT_MAPS_DIRECTORY)) {
        Echo("Installer -> cloning Zetero");
        const std::string git_url("https://github.com/ubtue/zotero-enhancement-maps.git");
        ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("git"), { "clone", git_url, ZOTERO_ENHANCEMENT_MAPS_DIRECTORY });
    }

    // syslog
    SetupSysLog();
    SetupSudo();

    if (AppArmorUtil::IsEnabled()) {
        Echo("Installer -> setup AppArmor for apache2");
        const std::string profile_id("apache2");
        AppArmorUtil::InstallLocalProfile(INSTALLER_DATA_DIRECTORY + "/apparmor/" + profile_id);
        AppArmorUtil::SetLocalProfileMode(profile_id, AppArmorUtil::ENFORCE);
    }

    // ...and then install the rest of ub_tools:
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    if (make_install)
        ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("make"), { "--jobs=4", "install" });
    else
        ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("make"), { "--jobs=4" });

    CreateUbToolsDatabase(db_connection_root);
    GitActivateCustomHooks(UB_TOOLS_DIRECTORY);
    FileUtil::MakeDirectoryOrDie("/usr/local/run");
    RegisterSystemUpdateVersion();

    // Install boot notification service:
    if (SystemdUtil::IsAvailable()) {
        SystemdUtil::InstallUnit(UB_TOOLS_DIRECTORY + "/cpp/data/installer/boot_notification.service");
        SystemdUtil::EnableUnit("boot_notification");
    }

    Echo("Installer -> installed ub_tools.");
}


std::string GetStringFromTerminal(const std::string &prompt) {
    std::cout << prompt << " >";
    std::string input;
    std::getline(std::cin, input);
    return StringUtil::TrimWhite(&input);
}


void InstallCronjobs(const bool production, const std::string &cronjobs_template_file, const std::string &crontab_block_start,
                     const std::string &crontab_block_end, Template::Map &names_to_values_map) {
    Echo("Installer -> install cronjobs");
    FileUtil::AutoTempFile crontab_temp_file_old;
    // crontab -l returns error code if crontab is empty, so dont use ExecUtil::ExecOrDie!!!
    ExecUtil::Exec(ExecUtil::LocateOrDie("crontab"), { "-l" }, "", crontab_temp_file_old.getFilePath());
    FileUtil::AutoTempFile crontab_temp_file_custom;
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("sed"),
                        { "-e", "/" + crontab_block_start + "/,/" + crontab_block_end + "/d", crontab_temp_file_old.getFilePath() }, "",
                        crontab_temp_file_custom.getFilePath());
    const std::string cronjobs_custom(FileUtil::ReadStringOrDie(crontab_temp_file_custom.getFilePath()));

    if (production)
        names_to_values_map.insertScalar("production", "true");
    std::string cronjobs_generated(crontab_block_start + "\n");
    if (names_to_values_map.empty())
        cronjobs_generated += FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY + '/' + cronjobs_template_file);
    else
        cronjobs_generated += Template::ExpandTemplate(FileUtil::ReadStringOrDie(INSTALLER_DATA_DIRECTORY + '/' + cronjobs_template_file),
                                                       names_to_values_map);
    if (not StringUtil::EndsWith(cronjobs_generated, '\n'))
        cronjobs_generated += '\n';
    cronjobs_generated += crontab_block_end + "\n";

    FileUtil::AutoTempFile crontab_temp_file_new;
    FileUtil::AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_generated);
    FileUtil::AppendStringToFile(crontab_temp_file_new.getFilePath(), cronjobs_custom);

    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("crontab"), { crontab_temp_file_new.getFilePath() });
    Echo("Installed cronjobs.");
}


void InstallVuFindCronjobs(const bool production, const VuFindSystemType vufind_system_type) {
    Echo("Installer -> install vufind cronjob");
    static const std::string start_vufind_autogenerated("# START VUFIND AUTOGENERATED");
    static const std::string end_vufind_autogenerated("# END VUFIND AUTOGENERATED");

    Template::Map names_to_values_map;
    if (vufind_system_type == IXTHEO) {
        names_to_values_map.insertScalar("ixtheo_host", GetStringFromTerminal("IxTheo Hostname"));
        names_to_values_map.insertScalar("relbib_host", GetStringFromTerminal("RelBib Hostname"));
        names_to_values_map.insertScalar("bibstudies_host", GetStringFromTerminal("BibStudies Hostname"));
        names_to_values_map.insertScalar("churchlaw_host", GetStringFromTerminal("ChurchLaw Hostname"));
    }

    InstallCronjobs(production, (vufind_system_type == KRIMDOK ? "krimdok.cronjobs" : "ixtheo.cronjobs"), start_vufind_autogenerated,
                    end_vufind_autogenerated, names_to_values_map);
}


void AddUserToGroup(const std::string &username, const std::string &groupname) {
    Echo("Installer -> Adding user " + username + " to group " + groupname);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("usermod"), { "--append", "--groups", groupname, username });
}


// Note: this will also create a group with the same name
void CreateUserIfNotExists(const std::string &username) {
    const int user_exists(ExecUtil::Exec(ExecUtil::LocateOrDie("id"), { "-u", username }));
    if (user_exists == 1) {
        Echo("Installer -> Creating user " + username + "...");
        ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("useradd"),
                            { "--system", "--user-group", "--no-create-home", "--shell", "/bin/bash", username });
    } else if (user_exists > 1)
        Error("Installer -> Failed to check if user exists: " + username);
}


void GenerateXml(const std::string &filename_source, const std::string &filename_target) {
    std::string dirname_source, basename_source;
    FileUtil::DirnameAndBasename(filename_source, &dirname_source, &basename_source);

    Echo("Installer -> Generating " + filename_target + " from " + basename_source);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("xmllint"), { "--xinclude", "--format", filename_source }, "", filename_target);
}


void GitAssumeUnchanged(const std::string &filename) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("git"), { "update-index", "--assume-unchanged", filename });
}


void GitCheckout(const std::string &filename) {
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(filename, &dirname, &basename);
    TemporaryChDir tmp(dirname);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("git"), { "checkout", filename });
}


void UseCustomFileIfExists(std::string filename_custom, std::string filename_default) {
    if (FileUtil::Exists(filename_custom)) {
        FileUtil::CreateSymlink(filename_custom, filename_default);
        GitAssumeUnchanged(filename_default);
    } else
        GitCheckout(filename_default);
}


void DownloadVuFind() {
    if (FileUtil::IsDirectory(VUFIND_DIRECTORY))
        Echo("installer -> VuFind directory already exists, skipping download");
    else {
        Echo("installer -> downloading TueFind git repository");
        const std::string git_url("https://github.com/ubtue/tuefind.git");
        ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("git"), { "clone", git_url, VUFIND_DIRECTORY });
        GitActivateCustomHooks(VUFIND_DIRECTORY);
    }
}


/**
 * Configure Apache User
 * - Create user "vufind" as system user if not exists
 * - Grant permissions on relevant directories
 */
void ConfigureApacheUser() {
    Echo("Installer -> Configure apache user");
    const std::string username("vufind");
    CreateUserIfNotExists(username);

    // systemd will start apache as root
    // but apache will start children as configured in /etc
    std::string config_filename;

    AddUserToGroup(username, "www-data");
    config_filename = "/etc/apache2/envvars";
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("sed"),
                        { "-i", "s/export APACHE_RUN_USER=www-data/export APACHE_RUN_USER=" + username + "/", config_filename });

    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("sed"),
                        { "-i", "s/export APACHE_RUN_GROUP=www-data/export APACHE_RUN_GROUP=" + username + "/", config_filename });

    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("find"),
                        { VUFIND_DIRECTORY + "/local", "-name", "cache", "-exec", "chown", "-R", username + ":" + username, "{}", "+" });
    FileUtil::ChangeOwnerOrDie(UBTools::GetTueFindLogPath(), username, username, /*recursive=*/true);
}


/**
 * Configure Solr User and services
 * - Create user "solr" as system user if not exists
 * - Grant permissions on relevant directories
 * - register solr service in systemd
 */
void ConfigureSolrUserAndService(const VuFindSystemType system_type, const bool install_systemctl) {
    Echo("Installer -> Configure Solr User and Service");
    // note: if you wanna change username, don't do it only here, also check vufind.service!
    const std::string USER_AND_GROUP_NAME("solr");
    const std::string VUFIND_SERVICE("vufind");

    CreateUserIfNotExists(USER_AND_GROUP_NAME);

    Echo("Installer -> Setting directory permissions for Solr user...");
    FileUtil::ChangeOwnerOrDie(VUFIND_DIRECTORY + "/solr", USER_AND_GROUP_NAME, USER_AND_GROUP_NAME, /*recursive=*/true);
    FileUtil::ChangeOwnerOrDie(VUFIND_DIRECTORY + "/import", USER_AND_GROUP_NAME, USER_AND_GROUP_NAME, /*recursive=*/true);

    const std::string solr_security_settings(
        "solr hard nofile 65535\n"
        "solr soft nofile 65535\n"
        "solr hard nproc 65535\n"
        "solr soft nproc 65535\n");
    FileUtil::WriteString("/etc/security/limits.d/20-solr.conf", solr_security_settings);

    // systemctl: we do enable as well as daemon-reload and restart
    // to achieve an idempotent installation
    if (install_systemctl) {
        Echo("Installer -> Activating " + VUFIND_SERVICE + " service...");
        GenerateAndInstallVuFindServiceTemplate(system_type, VUFIND_SERVICE);
        SystemdEnableAndRunUnit(VUFIND_SERVICE);
    }
}


void PermanentlySetEnvironmentVariables(const std::vector<std::pair<std::string, std::string>> &keys_and_values,
                                        const std::string &script_path) {
    Echo("Installer -> Permanetly set environment variables");
    std::string variables;
    for (const auto &[key, value] : keys_and_values)
        variables += "export " + key + "=" + value + "\n";
    FileUtil::WriteString(script_path, variables);
    MiscUtil::LoadExports(script_path, /* overwrite = */ true);
}


void SetVuFindEnvironmentVariables(const std::string &vufind_system_type_string) {
    Echo("Installer -> Setup vufind environment");
    std::vector<std::pair<std::string, std::string>> keys_and_values{
        { "VUFIND_HOME", VUFIND_DIRECTORY },
        { "VUFIND_LOCAL_DIR", VUFIND_DIRECTORY + "/local/tuefind/instances/" + vufind_system_type_string },
        { "TUEFIND_FLAVOUR", vufind_system_type_string },
    };
    PermanentlySetEnvironmentVariables(keys_and_values, "/etc/profile.d/vufind.sh");
}

void SetFulltextEnvironmentVariables() {
    // Currently only the IxTheo approach is supported
    Echo("Installer -> Set full text environment variables");
    const std::vector<std::pair<std::string, std::string>> keys_and_values{ { "FULLTEXT_FLAVOUR", "fulltext_ixtheo" } };
    PermanentlySetEnvironmentVariables(keys_and_values, "/etc/profile.d/fulltext.sh");
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
void ConfigureVuFind(const bool production, const VuFindSystemType vufind_system_type, const bool install_cronjobs,
                     const bool install_systemctl) {
    Echo("Installer -> Configure vufind ...");
    // We need to increase default_socket_timeout for big downloads on slow mirrors, especially Solr (default 60 seconds) .
    TemporaryChDir tmp2(VUFIND_DIRECTORY);
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("php"), { "-d", "default_socket_timeout=600", ExecUtil::LocateOrDie("composer"), "install" });
    // We explicitly need to use sudo here, even if we're already root, or it will fail,
    // see https://stackoverflow.com/questions/16151018/how-to-fix-npm-throwing-error-without-sudo
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("sudo"), { "npm", "install" });

    Echo("Installer -> Building CSS");
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("grunt"), { "less" });

    const std::string vufind_system_type_string(VuFindSystemTypeToString(vufind_system_type));
    Echo("Installer -> Starting configuration for " + vufind_system_type_string);
    const std::string SOLR_BIBLIO_DIRECTORY = VUFIND_DIRECTORY + "/solr/vufind/biblio/conf";
    const std::string SOLR_AUTHORITY_DIRECTORY = VUFIND_DIRECTORY + "/solr/vufind/authority/conf";

    Echo("Installer -> SOLR Configuration (solrconfig.xml)");
    ExecUtil::ExecOrDie(SOLR_BIBLIO_DIRECTORY + "/make_symlinks.sh", { vufind_system_type_string });

    Echo("Installer -> SOLR Schema biblio (schema_local_*.xml)");
    ExecUtil::ExecOrDie(SOLR_BIBLIO_DIRECTORY + "/generate_xml.sh", { vufind_system_type_string });

    Echo("Installer -> Synonyms (synonyms_*.txt)");
    ExecUtil::ExecOrDie(SOLR_BIBLIO_DIRECTORY + "/touch_synonyms.sh", { vufind_system_type_string });

    Echo("Installer -> SOLR Schema authority (schema_local_*.xml)");
    ExecUtil::ExecOrDie(SOLR_AUTHORITY_DIRECTORY + "/generate_xml.sh", { vufind_system_type_string });

    Echo("Installer -> solrmarc (marc_local.properties)");
    ExecUtil::ExecOrDie(VUFIND_DIRECTORY + "/import/make_marc_local_properties.sh", { vufind_system_type_string });

    SetVuFindEnvironmentVariables(vufind_system_type_string);

    Echo("Installer -> alphabetical browse");
    UseCustomFileIfExists(VUFIND_DIRECTORY + "/index-alphabetic-browse_" + vufind_system_type_string + ".sh",
                          VUFIND_DIRECTORY + "/index-alphabetic-browse.sh");

    if (install_cronjobs) {
        Echo("Installer -> cronjobs");
        InstallVuFindCronjobs(production, vufind_system_type);
    }

    Echo("Installer -> creating log directory");
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("mkdir"), { "-p", UBTools::GetTueFindLogPath() });

    ConfigureSolrUserAndService(vufind_system_type, install_systemctl);
    ConfigureApacheUser();

    const std::string NEWSLETTER_DIRECTORY_PATH(UBTools::GetTuelibPath() + "newsletters");
    if (not FileUtil::Exists(NEWSLETTER_DIRECTORY_PATH)) {
        Echo("Installer -> creating " + NEWSLETTER_DIRECTORY_PATH);
        FileUtil::MakeDirectoryOrDie(NEWSLETTER_DIRECTORY_PATH, /*recursive=*/true);

        Echo("Installer -> creating " + NEWSLETTER_DIRECTORY_PATH + "/sent");
        FileUtil::MakeDirectoryOrDie(NEWSLETTER_DIRECTORY_PATH + "/sent");

        FileUtil::ChangeOwnerOrDie(NEWSLETTER_DIRECTORY_PATH, "vufind", "vufind", /*recursive=*/true);
    }

    Echo("Installer -> generating HMAC hash");
    const std::string HMAC_FILE_PATH(VUFIND_LOCAL_OVERRIDES_DIRECTORY + "/hmac.conf");
    if (not FileUtil::Exists(HMAC_FILE_PATH))
        FileUtil::WriteStringOrDie(HMAC_FILE_PATH,
                                   StringUtil::GenerateRandom(/*length=*/32, /*alphabet=*/"abcdefghijklmnopqrstuvwxyz0123456789"));

    Echo("Installer -> " + vufind_system_type_string + " configuration completed!");
}


void InstallFullTextBackendCronjobs(const bool production) {
    Echo("Installer -> install full text backend cronjobs");
    Template::Map empty_map;
    InstallCronjobs(production, "fulltext.cronjobs", "# START AUTOGENERATED", "# END AUTOGENERATED", empty_map);
}


void WaitForElasticsearchReady() {
    Echo("installer -> wait for elastic search ready");
    const std::string host("127.0.0.1"); // avoid docker address assign problem
    const std::string base_url("http://" + host + ":9200/");
    const unsigned MAX_ITERATIONS(5);
    const unsigned SLEEP_TIME_SECS(5);

    for (unsigned iteration(1); iteration <= MAX_ITERATIONS; ++iteration) {
        Downloader downloader(base_url);
        if (downloader.getResponseCode() == 200)
            break;
        ::sleep(SLEEP_TIME_SECS);
        if (iteration == MAX_ITERATIONS)
            LOG_ERROR("Installer -> ES apparently down [1]");
    }

    const unsigned TIMEOUT_MS(5 * 1000);
    for (unsigned iteration(1); iteration <= MAX_ITERATIONS; ++iteration) {
        std::string result;
        Download(base_url + "_cat/health?h=status", TIMEOUT_MS, &result);
        result = StringUtil::TrimWhite(result);

        if (result == "yellow" or result == "green")
            break;
        ::sleep(SLEEP_TIME_SECS);
        if (iteration == MAX_ITERATIONS)
            LOG_ERROR("Installer -> ES apparently down [2]");
    }
}


void ConfigureFullTextBackend(const bool production, const bool install_cronjobs = false) {
    Echo("Installer -> Configure full text backend");
    static const std::string elasticsearch_programs_dir("/usr/local/ub_tools/cpp/elasticsearch");
    bool es_was_already_running(false);
    pid_t es_install_pid(0);
    std::unordered_set<unsigned> running_pids;
    if (SystemdUtil::IsAvailable()) {
        SystemdUtil::EnableUnit("elasticsearch");
        if (not SystemdUtil::IsUnitRunning("elasticsearch"))
            SystemdUtil::StartUnit("elasticsearch");
        else
            es_was_already_running = true;
    } else {
        running_pids = ExecUtil::FindActivePrograms("elasticsearch");
        if (running_pids.size() == 0) {
            es_install_pid = ExecUtil::Spawn(ExecUtil::LocateOrDie("su"), { "--command", "/usr/share/elasticsearch/bin/elasticsearch",
                                                                            "--shell", "/bin/bash", "elasticsearch" });
            WaitForElasticsearchReady();
        } else
            es_was_already_running = true;
    }
    ExecUtil::ExecOrDie(elasticsearch_programs_dir + "/create_indices_and_type.sh", std::vector<std::string>{} /* args */,
                        "" /* new_stdin */, "" /* new_stdout */, "" /* new_stderr */, 0 /* timeout_in_seconds */,
                        SIGKILL /* tardy_child_signal */, std::unordered_map<std::string, std::string>() /* envs */,
                        elasticsearch_programs_dir);
    if (not es_was_already_running) {
        if (SystemdUtil::IsAvailable())
            SystemdUtil::StopUnit("elasticsearch");
        else
            ::kill(es_install_pid, SIGKILL);
    }
    SetFulltextEnvironmentVariables();
    if (install_cronjobs)
        InstallFullTextBackendCronjobs(production);
}


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    Echo("installer -> starting installer .....");
    std::string file_contents;
    if (not(FileUtil::ReadString("/etc/issue", &file_contents)
            and StringUtil::FindCaseInsensitive(file_contents, "ubuntu") != std::string::npos))
    {
        Error("OS type could not be detected or is not supported! aborting");
    }

    InstallationType installation_type(UB_TOOLS_ONLY);
    std::string vufind_system_type_string;
    VuFindSystemType vufind_system_type(IXTHEO);
    bool omit_cronjobs(false);
    bool omit_systemctl(false);
    bool production(false);

    if (std::strcmp("ub-tools-only", argv[1]) == 0)
        installation_type = UB_TOOLS_ONLY;
    else if (std::strcmp("fulltext-backend", argv[1]) == 0)
        installation_type = FULLTEXT_BACKEND;
    else if (std::strcmp("vufind", argv[1]) == 0)
        installation_type = VUFIND;
    else
        Usage();

    if (installation_type == UB_TOOLS_ONLY) {
        if (argc != 2)
            Usage();
    } else if (installation_type == VUFIND or installation_type == FULLTEXT_BACKEND) {
        int argv_additional_params_start(2);
        if (argc < 3)
            Usage();

        if (installation_type == VUFIND) {
            if (argc < 4)
                Usage();
            argv_additional_params_start = 3;
            vufind_system_type_string = argv[2];
            if (std::strcmp("ixtheo", vufind_system_type_string.c_str()) == 0)
                vufind_system_type = IXTHEO;
            else if (std::strcmp("krimdok", vufind_system_type_string.c_str()) == 0)
                vufind_system_type = KRIMDOK;
            else
                LOG_ERROR("argument 2 must be ixtheo or krimdok!");
        }

        if (argv_additional_params_start > argc - 1)
            Usage();

        if (std::strcmp("--production", argv[argv_additional_params_start]) == 0)
            production = true;
        else if (std::strcmp("--test", argv[argv_additional_params_start]) == 0)
            production = false;
        else
            LOG_ERROR("argument " + std::to_string(argv_additional_params_start) + " must be --production or --test!");

        for (int i(argv_additional_params_start + 1); i < argc; ++i) {
            if (std::strcmp("--omit-cronjobs", argv[i]) == 0)
                omit_cronjobs = true;
            else if (std::strcmp("--omit-systemctl", argv[i]) == 0)
                omit_systemctl = true;
            else
                LOG_ERROR("argument " + std::to_string(i) + " has an invalid value!");
        }
    }

    if (not omit_systemctl and not SystemdUtil::IsAvailable())
        Error(
            "Systemd is not available in this environment."
            "Please use --omit-systemctl explicitly if you want to skip service installations.");
    const bool install_systemctl(not omit_systemctl and SystemdUtil::IsAvailable());

    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    // Install dependencies before vufind
    // correct PHP version for composer dependancies
    InstallSoftwareDependencies(vufind_system_type_string, installation_type, install_systemctl);

    // Where to find our own stuff:
    MiscUtil::AddToPATH("/usr/local/bin/", MiscUtil::PreferredPathLocation::LEADING);

    MountDeptDriveAndInstallSSHKeysOrDie(vufind_system_type);

    Echo("installer -> checking MySQL server, whether it is active or not");
    // Init root DB connection for later re-use
    AssureMysqlServerIsRunning();
    DbConnection db_connection_root(DbConnection::MySQLFactory("mysql", "root", ""));
    // Needed so ub_tools user will be able to execute updates later, including triggers and stores procedures
    Echo("set global log_bin_trust_functions_creators");
    db_connection_root.queryOrDie("SET GLOBAL log_bin_trust_function_creators = 1");

    Echo("starting VUFIND installation");
    if (installation_type == VUFIND) {
        FileUtil::MakeDirectoryOrDie("/mnt/zram");
        Echo("installer -> download vufind ...");
        DownloadVuFind();
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
        Echo("installer -> configure vufind");
        ConfigureVuFind(production, vufind_system_type, not omit_cronjobs, install_systemctl);
#ifndef __clang__
#pragma GCC diagnostic error "-Wmaybe-uninitialized"
#endif
    }
    Echo("installer -> installing ub_tools");
    InstallUBTools(/* make_install = */ true, &db_connection_root);
    if (installation_type == FULLTEXT_BACKEND)
        ConfigureFullTextBackend(production, not omit_cronjobs);
    else if (installation_type == VUFIND) {
        Echo("Installer -> start creating vufind database");
        CreateVuFindDatabases(vufind_system_type, &db_connection_root);
        Echo("finish install vufind database");
    }

    Echo("installation complete.");
    return EXIT_SUCCESS;
}
