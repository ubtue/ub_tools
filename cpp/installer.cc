/** \brief A tool for installing IxTheo and KrimDok from scratch on Ubuntu and Centos systems.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
static std::string master_password;


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


void ExecOrDie(const std::string &command, const std::vector<std::string> &arguments) {
    int exit_code;
    if ((exit_code = ExecUtil::Exec(command, arguments)) != 0)
        Error("Failed to execute \"" + command + "\"! (exit code was " + std::to_string(exit_code) + ")");
}


void MountLUKSContainerOrDie() {
    FileUtil::AutoTempFile key_file_temp_file;
    if (not FileUtil::WriteString(key_file_temp_file.getFilePath(), master_password))
        Error("failed to write the master password into the temporary LUKS key file!");
        
    const std::string LUKS_IMAGE_FILE("/usr/local/ub_tools/configs.ext4.luks");
    const std::string LUKS_MOUNT_POINT("/usr/local/ub_tools/configs");
    ExecOrDie(ExecUtil::Which("cryptsetup"),
              { "luksOpen", "--batch-mode", "--key-file", key_file_temp_file.getFilePath(), LUKS_IMAGE_FILE,
                "configs" });
    ExecOrDie("/bin/mount", { "/dev/mapper/configs", LUKS_MOUNT_POINT });
}


void ExecuteCommandSequence(
    const std::vector<std::pair<std::string, std::vector<std::string>>> &commands_and_arguments)
{
    for (const auto &command_and_arguments : commands_and_arguments)
        ExecOrDie(command_and_arguments.first, command_and_arguments.second);
}


inline std::pair<std::string, std::vector<std::string>> CmdAndArgs(const std::string &command,
                                                                   const std::vector<std::string> &arguments)
{
    return std::pair<std::string, std::vector<std::string>>(command, arguments);
}

                 
void InstallUbuntuSoftwarePackages(const VuFindSystemType vufind_system_type) {
    const std::vector<std::pair<std::string, std::vector<std::string>>> &commands_and_arguments {
        CmdAndArgs("/usr/bin/add-apt-repository", { "--yes", "ppa:ubuntu-lxc/lxd-stable" }),
        CmdAndArgs("/usr/bin/apt", { "update" }),
        CmdAndArgs(
            "/usr/bin/apt",
            { "install", "--yes", "clang", "golang", "wget", "curl", "git", "apache2", "libapache2-mod-gnutls",
                    "mysql-server", "php7.0", "php7.0-dev", "php-pear", "php7.0-json", "php7.0-ldap", "php7.0-mcrypt",
              "php7.0-mysql", "php7.0-xsl", "php7.0-intl", "php7.0-gd", "libapache2-mod-php7.0", "composer",
              "openjdk-8-jdk", "libmagic-dev", "libpcre3-dev", "libssl-dev", "libkyotocabinet-dev", "mutt",
              "libxml2-dev", "libmysqlclient-dev", "libcurl4-openssl-dev", "ant", "libtokyocabinet-dev",
              "liblz4-tool", "libarchive-dev", "libboost-all-dev", "clang-3.8", "clang++-3.8", "clang", "golang" }),
        CmdAndArgs("/usr/sbin/a2enmod", { "rewrite" }),
        CmdAndArgs("/usr/sbin/phpenmod", { "mcrypt" }),
        CmdAndArgs("/etc/init.d/apache2", { "restart" }),
//        CmdAndArgs("mysql_secure_installation", {  }),
    };
    ExecuteCommandSequence(commands_and_arguments);

    // If installing a KrimDok system we need tesseract:
    if (vufind_system_type == KRIMDOK)
        ExecOrDie("/usr/bin/apt", { "install", "--yes", "tesseract-ocr-all" });
}


void InstallCentOSSoftwarePackages(const VuFindSystemType vufind_system_type) {
    const std::vector<std::pair<std::string, std::vector<std::string>>> &commands_and_arguments {
        CmdAndArgs("/bin/yum", { "update" }),
        CmdAndArgs("/bin/yum", { "-y", "install", "epel-release" }),
        CmdAndArgs(
            "/bin/yum",
            { "-y", "install", "mawk", "git", "mariadb", "mariadb-server", "httpd", "php", "php-devel", "php-mcrypt",
              "php-intl", "php-ldap", "php-mysql", "php-xsl", "php-gd", "php-mbstring", "php-mcrypt",
              "java-*-openjdk-devel", "mawk", "mod_ssl", "epel-release", "wget", "policycoreutils-python" }),
        CmdAndArgs("systemctl", { "start", "mariadb.service" }),
        CmdAndArgs("mysql_secure_installation", {  }),
        CmdAndArgs(
            "/bin/wget",
            { "http://download.opensuse.org/repositories/security:shibboleth/CentOS_7/security:shibboleth.repo",
              "--directory-prefix=/etc/yum.repos.d/" }),
        CmdAndArgs(
            "/bin/yum",
            { "-y", "install", "curl-openssl", "mutt", "golang", "lsof", "clang", "gcc-c++.x86_64", "file-devel",
              "pcre-devel", "openssl-devel", "kyotocabinet-devel", "tokyocabinet-devel", "poppler-utils", "libwebp",
              "mariadb-devel.x86_64", "libxml2-devel.x86_64", "libcurl-openssl-devel.x86_64", "ant", "lz4", "unzip",
              "libarchive-devel", "boost-devel" }),
        CmdAndArgs(
            "/bin/ln",
            { "-s", "/usr/share/tessdata/deu.traineddata", "/usr/share/tesseract/tessdata/deu.traineddata" }),
    };
    ExecuteCommandSequence(commands_and_arguments);

    if (vufind_system_type == KRIMDOK) {
        std::vector<std::string> rpm_package_install_args;
        FileUtil::GetFileNameList("*.\\\\.rpm", &rpm_package_install_args,
                                  "/mnt/ZE020150/IT-Abteilung/02_Projekte/11_KrimDok_neu/05_Pakete/");
        rpm_package_install_args.insert(rpm_package_install_args.begin(), "-y");
        rpm_package_install_args.insert(rpm_package_install_args.begin(), "install");
        ExecOrDie("/bin/yum", rpm_package_install_args);
    }
}


void InstallSoftwarePackages(const OSSystemType os_system_type, const VuFindSystemType vufind_system_type) {
    if (os_system_type == UBUNTU)
        InstallUbuntuSoftwarePackages(vufind_system_type);
    else
        InstallCentOSSoftwarePackages(vufind_system_type);
    Echo("installed software packages");
}


void ChangeDirectoryOrDie(const std::string &new_working_directory) {
    if (::chdir(new_working_directory.c_str()) != 0)
        Error("failed to set the new working directory to \"" + new_working_directory + "\"! ("
              + std::string(::strerror(errno)) + ")");
}


void InstallUBTools() {
    const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");
    ExecOrDie(ExecUtil::Which("git"), { "clone", "https://github.com/ubtue/ub_tools.git", UB_TOOLS_DIRECTORY });
    ChangeDirectoryOrDie(UB_TOOLS_DIRECTORY);
    ExecOrDie(ExecUtil::Which("make"), { "install" });
}


void GetMasterPassword() {
    errno = 0;
    master_password = ::getpass("Master password >");
    if (errno != 0)
        Error("failed to read the password from the terminal!");
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();
    
    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    GetMasterPassword();
    
    VuFindSystemType vufind_system_type;
    if (::strcasecmp(argv[1], "krimdok") == 0)
        vufind_system_type = KRIMDOK;
    else if (::strcasecmp(argv[1], "ixtheo") == 0)
        vufind_system_type = IXTHEO;
    else
        Error("system type must be either \"krimdok\" or \"ixtheo\"!");

    const OSSystemType os_system_type(DetermineOSSystemType());

    try {
        MountLUKSContainerOrDie();
        InstallSoftwarePackages(os_system_type, vufind_system_type);
        InstallUBTools();
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
