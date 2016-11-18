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


OSSystemType DetermineOSSystemType() {
    std::string file_contents;
    if (FileUtil::ReadString("/etc/issue", &file_contents)
        and StringUtil::FindCaseInsensitive(file_contents, "ubuntu") != std::string::npos)
        return UBUNTU;
    if (FileUtil::ReadString("/etc/redhat_release", &file_contents)
        and StringUtil::FindCaseInsensitive(file_contents, "centos") != std::string::npos)
        return CENTOS;
    Error("you're probably not on an Ubunto nor on a CentOS system!");
}


// Returns true if a line starting with "line_prefix" was found in "filename", o/w returns false.
bool FileContainsLineStartingWith(const std::string &filename, const std::string &line_prefix) {
    std::string file_contents;
    if (unlikely(not FileUtil::ReadString(filename, &file_contents)))
        Error("in FileContainsLineStartingWith: could not read the contents of \"" + filename + "\"!");

    std::vector<std::string> lines;
    StringUtil::Split(file_contents, '\n', &lines);
    for (const auto &line : lines) {
        if (StringUtil::StartsWith(line, line_prefix))
            return true;
    }

    return false;
}


void AppendLineToFileOrDie(const std::string &filename, std::string line) {
    line += '\n';
    std::unique_ptr<File> file(FileUtil::OpenForAppeningOrDie(filename));
    if (file->size() == 0) {
        if (unlikely(file->write(line.data(), line.size()) != line.size()))
            Error("in AppendLineToFileOrDie: failed to append a line to \"" + filename + "\"!");
        return;
    }
    if (unlikely(not file->seek(-1, SEEK_END)))
        Error("in AppendLineToFileOrDie: failed to seek to the last byte in \"" + filename + "\"!");

    // Do we need to append a newline before appending our new line?
    const int last_char(file->get());
    if (last_char != '\n')
        line = '\n' + line; // Apprently we do.

    if (unlikely(file->write(line.data(), line.size()) != line.size()))
        Error("in AppendLineToFileOrDie: failed to write a line at the end of \"" + filename + "\"!");
}


void InsertFsTabLineOrDie(const std::string &line) {
    const std::string::size_type first_space_pos(line.find(' '));
    if (unlikely(first_space_pos == std::string::npos))
        Error("InsertFsTabLineOrDie: could not find a space in \"" + line + "\"!");
    if (unlikely(first_space_pos == 0))
        Error("InsertFsTabLineOrDie: first field of \"" + line + "\" must not be empty!");
    const std::string first_field_plus_space(line.substr(0, first_space_pos + 1));
    if (FileContainsLineStartingWith("/etc/fstab", first_field_plus_space))
        return; // Nothing to do.
    AppendLineToFileOrDie("/etc/fstab", line);
}


void ExecOrDie(const std::string &command, const std::vector<std::string> &arguments) {
    int exit_code;
    if ((exit_code = ExecUtil::Exec(command, arguments)) != 0)
        Error("Failed to execute \"" + command + "\"! (exit code was " + std::to_string(exit_code) + ")");
}


void MountDepartmentDriveOrDie() {
    InsertFsTabLineOrDie("//sn00.zdv.uni-tuebingen.de/ZE020150 /mnt/ZE020150 cifs "
                         "credentials=/root/.smbcredentials,workgroup=uni-tuebingen.de,uid=root,gid=root,auto 0 0");
    ExecOrDie("/bin/mount", { "--all" });
    Echo("mounted department drive");
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

                 
void InstallUbuntuSoftwarePackages() {
    const std::vector<std::pair<std::string, std::vector<std::string>>> &commands_and_arguments {
        CmdAndArgs("/usr/bin/add-apt-repository", { "ppa:ubuntu-lxc/lxd-stable" }),
        CmdAndArgs("/usr/bin/apt", { "update" }),
        CmdAndArgs(
            "/usr/bin/apt",
            { "install", "-y", "clang", "golang", "wget", "curl", "git", "apache2", "libapache2-mod-gnutls",
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
    Echo("installed software packages");
}


void InstallCentOSSoftwarePackages() {
    std::vector<std::string> rpm_package_install_args;
    FileUtil::GetFileNameList("*.\\\\.rpm", &rpm_package_install_args,
                              "/mnt/ZE020150/IT-Abteilung/02_Projekte/11_KrimDok_neu/05_Pakete/");
    rpm_package_install_args.insert(rpm_package_install_args.begin(), "-y");
    rpm_package_install_args.insert(rpm_package_install_args.begin(), "install");

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
        CmdAndArgs("/bin/yum", rpm_package_install_args),
        CmdAndArgs(
            "/bin/ln",
            { "-s", "/usr/share/tessdata/deu.traineddata", "/usr/share/tesseract/tessdata/deu.traineddata" }),
    };
    ExecuteCommandSequence(commands_and_arguments);
}


void InstallSoftwarePackages(const OSSystemType os_system_type) {
    if (os_system_type == UBUNTU)
        InstallUbuntuSoftwarePackages();
    else
        InstallCentOSSoftwarePackages();
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    VuFindSystemType vufind_system_type;
    if (::strcasecmp(argv[1], "krimdok") == 0)
        vufind_system_type = KRIMDOK;
    else if (::strcasecmp(argv[1], "ixtheo") == 0)
        vufind_system_type = IXTHEO;
    else
        Error("system type msut be either \"krimdok\" or \"ixtheo\"!");
    (void)vufind_system_type;

    const OSSystemType os_system_type(DetermineOSSystemType());
    
    if (::geteuid() != 0)
        Error("you must execute this program as root!");

    try {
        MountDepartmentDriveOrDie();
        InstallSoftwarePackages(os_system_type);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
