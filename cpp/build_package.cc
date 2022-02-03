/** \brief Utility for creating Debian/Ubunto AMD64 packages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \documentation See https://ubuntuforums.org/showthread.php?t=910717
 *
 *  \copyright 2019-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <sys/stat.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("(--deb|--rpm) [--output-directory=directory] path_to_binary description [blacklisted_library1 .. blacklisted_libraryN]");
}


struct Library {
    std::string full_name_;
    std::string name_;
    std::string version_;

public:
    Library() = default;
    Library(const Library &other) = default;
    Library(const std::string &full_name, const std::string &name, const std::string &version)
        : full_name_(full_name), name_(name), version_(version) { }

    inline std::string toString() const { return name_ + " (>= " + version_ + ")"; }
};


// Parses lines of the form "libkyotocabinet.so.16 => /lib64/libkyotocabinet.so.16 (0x00007f2334ecc000)".
void ExtractLibrary(const std::string &line, std::string * const full_name, std::string * const path, std::string * const simplified_name) {
    const auto first_space_pos(line.find(' '));
    if (first_space_pos == std::string::npos)
        LOG_ERROR("no space found in \"" + line + "\"!");

    *full_name = line.substr(0, first_space_pos);
    const auto first_dot_pos(full_name->find('.'));
    *simplified_name = (first_dot_pos == std::string::npos) ? *full_name : full_name->substr(0, first_dot_pos);

    const auto arrow_pos(line.find(" => "));
    if (arrow_pos == std::string::npos)
        LOG_ERROR("no => found in \"" + line + "\"!");

    const auto space_pos(line.find(' ', arrow_pos + 4));
    if (space_pos == std::string::npos)
        LOG_ERROR("no space found after library path in \"" + line + "\"!");

    *path = line.substr(arrow_pos + 4, space_pos - arrow_pos - 4);
}


std::string GetVersionHelper(const std::string &library_name) {
    std::string dpkg_output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout("dpkg -s " + library_name, &dpkg_output))
        LOG_ERROR("failed to execute dpkg!");

    std::vector<std::string> lines;
    StringUtil::SplitThenTrimWhite(dpkg_output, '\n', &lines);
    for (const auto &line : lines) {
        if (StringUtil::StartsWith(line, "Version: ")) {
            const auto version(line.substr(__builtin_strlen("Version: ")));
            const auto first_plus_pos(version.find('+'));
            return (first_plus_pos == std::string::npos) ? version : version.substr(0, first_plus_pos);
        }
    }

    return "";
}


inline std::set<std::string> FilterPackages(const std::set<std::string> &unfiltered_set, const std::set<std::string> &filter) {
    std::set<std::string> filtered_set;
    for (const auto &element : unfiltered_set) {
        if (filter.find(element) == filter.cend())
            filtered_set.emplace(element);
    }

    return filtered_set;
}


bool GetPackageAndVersion(const std::string &full_library_name, const std::set<std::string> &blacklist, std::string * const package_name,
                          std::string * const package_version) {
    std::string dpkg_output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout("dpkg -S " + full_library_name, &dpkg_output))
        LOG_ERROR("failed to execute dpkg!");

    std::vector<std::string> lines;
    StringUtil::SplitThenTrimWhite(dpkg_output, '\n', &lines);
    std::set<std::string> packages;
    for (const auto &line : lines) {
        const auto first_colon_pos(line.find(':'));
        if (first_colon_pos == std::string::npos or first_colon_pos == 0)
            LOG_ERROR("weird output line of \"dpkg -S\": \"" + line + "\"!");
        const auto package(line.substr(0, first_colon_pos));
        if (not StringUtil::EndsWith(package, "-dev"))
            packages.emplace(package);
    }

    if (packages.empty())
        LOG_ERROR("no packages found for library \"" + full_library_name + "\"!");

    if (packages.size() > 1) {
        packages = FilterPackages(packages, blacklist);
        if (packages.size() > 1)
            LOG_ERROR("multiple packages for \"" + full_library_name + "\": " + StringUtil::Join(packages, ", "));
    }
    if (packages.empty())
        return false;

    *package_version = GetVersionHelper(*(packages.cbegin()));
    if (package_version->empty())
        LOG_ERROR("library \"" + full_library_name + "\" not found!");

    *package_name = *(packages.cbegin());
    return true;
}


void GetLibraries(const bool build_deb, const std::string &binary_path, const std::set<std::string> &blacklist,
                  std::vector<Library> * const libraries) {
    std::string ldd_output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout("ldd " + binary_path, &ldd_output))
        LOG_ERROR("failed to execute ldd!");

    std::vector<std::string> lines;
    StringUtil::SplitThenTrimWhite(ldd_output, '\n', &lines);
    for (auto line(lines.cbegin() + 1); line != lines.cend(); ++line) {
        if (StringUtil::StartsWith(*line, "linux-vdso.so") or StringUtil::StartsWith(*line, "/lib64/ld-linux-x86-64.so") or line->empty())
            continue;

        std::string full_name, path, simplified_name;
        ExtractLibrary(*line, &full_name, &path, &simplified_name);

        if (build_deb) {
            std::string package_name, package_version;
            if (GetPackageAndVersion(full_name, blacklist, &package_name, &package_version))
                libraries->emplace_back(full_name, package_name, package_version);
        } else {
            std::string rpm_output;
            const std::string COMMAND("rpm --query --whatprovides " + path);
            if (not ExecUtil::ExecSubcommandAndCaptureStdout(COMMAND, &rpm_output))
                LOG_ERROR("failed to execute \"" + COMMAND + "\"!");
            StringUtil::SplitThenTrimWhite(rpm_output, '\n', &lines);
            if (lines.size() != 1)
                LOG_ERROR("found multiple packages for \"" + path + "\": " + StringUtil::Join(lines, ','));
        }
    }
}


void GenerateControl(File * const output, const std::string &package, const std::string &version, const std::string &description,
                     const std::vector<Library> &libraries) {
    (*output) << "Package: " << StringUtil::Map(package, '_', '-') << '\n';
    (*output) << "Version: " << version << '\n';
    (*output) << "Section: ub_tools\n";
    (*output) << "Priority: optional\n";
    (*output) << "Architecture: amd64\n";

    (*output) << "Depends: locales, ";
    bool first(true);
    for (const auto &library : libraries) {
        if (first)
            first = false;
        else
            (*output) << ", ";
        (*output) << library.toString();
    }
    (*output) << '\n';

    (*output) << "Maintainer: johannes.ruscheinski@uni-tuebingen.de\n";
    (*output) << "Description:";
    std::vector<std::string> description_lines;
    StringUtil::SplitThenTrimWhite(description, "\\n", &description_lines);
    for (const auto &line : description_lines)
        (*output) << ' ' << line << '\n';
}


void GeneratePostInst(const std::string &path) {
    FileUtil::WriteStringOrDie(path, "#!/bin/bash\nlocale-gen " UB_DEFAULT_LOCALE "\n");
    if (::chmod(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
        LOG_ERROR("chmod(3) on \"" + path + "\" failed!");
}


void BuildDebPackage(const std::string &binary_path, const std::string &package_version, const std::string &description,
                     const std::vector<Library> &libraries, const std::string &output_directory) {
    const std::string PACKAGE_NAME(FileUtil::GetBasename(binary_path));
    const std::string WORKING_DIR(PACKAGE_NAME + "_" + package_version);

    const std::string TARGET_DIRECTORY(WORKING_DIR + "/usr/local/bin");
    FileUtil::MakeDirectoryOrDie(TARGET_DIRECTORY, /* recursive = */ true);
    const std::string target_binary(TARGET_DIRECTORY + "/" + PACKAGE_NAME);
    FileUtil::CopyOrDie(binary_path, target_binary);
    if (::chmod(target_binary.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
        LOG_ERROR("chmod(3) on \"" + target_binary + "\" failed!");
    ExecUtil::ExecOrDie(ExecUtil::Which("strip"), { target_binary });

    FileUtil::MakeDirectoryOrDie(WORKING_DIR + "/DEBIAN");
    const auto control(FileUtil::OpenOutputFileOrDie(WORKING_DIR + "/DEBIAN/control"));
    GenerateControl(control.get(), FileUtil::GetBasename(binary_path), package_version, description, libraries);
    control->close();
    GeneratePostInst(WORKING_DIR + "/DEBIAN/postinst");

    ExecUtil::ExecOrDie(ExecUtil::Which("dpkg-deb"), { "--build", PACKAGE_NAME + "_" + package_version });

    if (not FileUtil::RemoveDirectory(WORKING_DIR))
        LOG_ERROR("failed to recursively delete \"" + WORKING_DIR + "\"!");

    if (not output_directory.empty()) {
        const std::string DEB_NAME(PACKAGE_NAME + "_" + package_version + ".deb");
        FileUtil::RenameFileOrDie(DEB_NAME, output_directory + "/" + DEB_NAME, /* remove_target = */ true);
    }
}


void GenerateSpecs(File * const output, const std::string &package, const std::string &version, const std::string &description,
                   const std::vector<Library> &libraries) {
    (*output) << "Name:           " << package << '\n';
    (*output) << "Version:        " << version << '\n';
    (*output) << "License:        AGPL 3";
    for (const auto &library : libraries)
        (*output) << "Requires:       " << library.full_name_ << '\n';
    (*output) << "BuildArch:      x86_64\n";

    (*output) << "%description\n";
    std::vector<std::string> description_lines;
    StringUtil::SplitThenTrimWhite(description, "\\n", &description_lines);
    for (const auto &line : description_lines)
        (*output) << line << '\n';
    (*output) << '\n';
}


void BuildRPMPackage(const std::string &binary_path, const std::string &package_version, const std::string &description,
                     const std::vector<Library> &libraries, const std::string & /*output_directory*/) {
    // Create rpmbuild directory tree in our home directory:
    ExecUtil::ExecOrDie(ExecUtil::Which("rpmdev-setuptree"));

    const std::string PACKAGE_NAME(FileUtil::GetBasename(binary_path));
    const std::string WORKING_DIR(MiscUtil::GetEnv("HOME") + "/rpmbuild");
    const auto specs(FileUtil::OpenOutputFileOrDie(WORKING_DIR + "/SPECS/" + PACKAGE_NAME + ".specs"));
    GenerateSpecs(specs.get(), PACKAGE_NAME, package_version, description, libraries);
    specs->close();

    ExecUtil::ExecOrDie("/bin/rm", { "--recursive", WORKING_DIR });
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    bool build_deb;
    if (std::strcmp(argv[1], "--deb") == 0)
        build_deb = true;
    else if (std::strcmp(argv[1], "--rpm") == 0)
        build_deb = false;
    else
        LOG_ERROR("first argument must be --deb or --rpm!");

    std::string output_directory;
    if (StringUtil::StartsWith(argv[2], "--output-directory=")) {
        output_directory = argv[2] + __builtin_strlen("--output-directory=");
        --argc, ++argv;
    }
    if (argc < 4)
        Usage();

    const std::string binary_path(argv[2]);
    if (not FileUtil::Exists(binary_path))
        LOG_ERROR("file not found: " + binary_path);

    const std::string description(argv[3]);

    std::set<std::string> blacklist;
    for (int arg_no(4); arg_no < argc; ++arg_no)
        blacklist.emplace(argv[arg_no]);

    std::vector<Library> libraries;
    GetLibraries(build_deb, binary_path, blacklist, &libraries);

    const auto package_version(TimeUtil::GetCurrentDateAndTime("%Y.%m.%d"));

    if (build_deb)
        BuildDebPackage(binary_path, package_version, description, libraries, output_directory);
    else
        BuildRPMPackage(binary_path, package_version, description, libraries, output_directory);

    return EXIT_SUCCESS;
}
