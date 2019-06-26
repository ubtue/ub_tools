/** \brief Utility for creating Debian/Ubunto AMD64 packages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \documentation See https://ubuntuforums.org/showthread.php?t=910717
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


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


void ExtractLibrary(const std::string &line, std::string * const full_name, std::string * const simplified_name) {
    const auto first_space_pos(line.find(' '));
    if (first_space_pos == std::string::npos)
        LOG_ERROR("no space found in \"" + line + "\"!");

    *full_name = line.substr(0, first_space_pos);
    const auto first_dot_pos(full_name->find('.'));
    *simplified_name = (first_dot_pos == std::string::npos) ? *full_name : full_name->substr(0, first_dot_pos);
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


std::string GetVersion(const std::string &full_library_name, const std::set<std::string> &blacklist) {
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

    static const std::set<std::string> BASE_PACKAGES{ "libc6", "libc6-i386", "lib32stdc++6", "libstdc++6", "lib32gcc1", "libgcc1" };
    packages = FilterPackages(packages, BASE_PACKAGES);

    if (packages.size() > 1) {
        packages = FilterPackages(packages, blacklist);
        if (packages.size() > 1)
            LOG_ERROR("multiple packages for \"" + full_library_name + "\": " + StringUtil::Join(packages, ", "));
    }
    if (packages.empty())
        return "";

    std::string version = GetVersionHelper(*(packages.cbegin()));
    if (version.empty())
        LOG_ERROR("library \"" + full_library_name + "\" not found!");

    return version;
}


void GetLibraries(const std::string &binary_path, const std::set<std::string> &blacklist, std::vector<Library> * const libraries) {
    std::string ldd_output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout("ldd " + binary_path, &ldd_output))
        LOG_ERROR("failed to execute ldd!");

    std::vector<std::string> lines;
    StringUtil::SplitThenTrimWhite(ldd_output, '\n', &lines);
    for (auto line(lines.cbegin() + 1); line != lines.cend(); ++line) {
        std::string full_name, simplified_name;
        ExtractLibrary(*line, &full_name, &simplified_name);
        const auto version(GetVersion(full_name, blacklist));
        if (not version.empty())
            libraries->emplace_back(full_name, simplified_name, version);
    }
}


void GenerateControl(File * const output, const std::string &package, const std::string &version, const std::string &description,
                     const std::vector<Library> &libraries)
{
    (*output) << "Package: " << StringUtil::Map(package, '_', '-') << '\n';
    (*output) << "Version: " << version << '\n';
    (*output) << "Section: ub_tools\n";
    (*output) << "Priority: optional\n";
    (*output) << "Architecture: amd64\n";

    (*output) << "Depends: ";
    bool first(true);
    for (const auto library : libraries) {
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


void BuildDebPackage(const std::string &binary_path, const std::string &package_version, const std::string &description,
                     const std::vector<Library> &libraries)
{
    const std::string PACKAGE_NAME(FileUtil::GetBasename(binary_path));
    const std::string WORKING_DIR(PACKAGE_NAME + "_" + package_version);

    const std::string TARGET_DIRECTORY(WORKING_DIR + "/usr/local/bin");
    FileUtil::MakeDirectoryOrDie(TARGET_DIRECTORY, /* recursive = */true);
    const std::string target_binary(TARGET_DIRECTORY + "/" + PACKAGE_NAME);
    FileUtil::CopyOrDie(binary_path, target_binary);
    ExecUtil::ExecOrDie(ExecUtil::Which("strip"), { target_binary });

    FileUtil::MakeDirectoryOrDie(WORKING_DIR + "/DEBIAN");
    const auto control(FileUtil::OpenOutputFileOrDie(WORKING_DIR + "/DEBIAN/control"));
    GenerateControl(control.get(), FileUtil::GetBasename(binary_path), package_version, description, libraries);
    control->close();

    ExecUtil::ExecOrDie(ExecUtil::Which("dpkg-deb"), { "--build", PACKAGE_NAME + "_" + package_version });

    if (not FileUtil::RemoveDirectory(WORKING_DIR))
        LOG_ERROR("failed to recursively delete \"" + WORKING_DIR + "\"!");
}


void GenerateSpecs(File * const output, const std::string &package, const std::string &version, const std::string &description,
                   const std::vector<Library> &libraries)
{
    (*output) << "Name:           " << package << '\n';
    (*output) << "Version:        " << version << '\n';
    for (const auto &library : libraries)
        (*output) << "Requires:       " << library.full_name_ << '\n';
    (*output) << "BuildArch          amd64\n";

    (*output) << "%description\n";
    std::vector<std::string> description_lines;
    StringUtil::SplitThenTrimWhite(description, "\\n", &description_lines);
    for (const auto &line : description_lines)
        (*output) << line << '\n';
    (*output) << '\n';
}


void BuildRPMPackage(const std::string &binary_path, const std::string &package_version, const std::string &description,
                     const std::vector<Library> &libraries)
{
    // Create rpmbuild directory tree in our home directory:
    ExecUtil::ExecOrDie(ExecUtil::Which("rpmdev-setuptree"));

    const std::string PACKAGE_NAME(FileUtil::GetBasename(binary_path));
    const auto specs(FileUtil::OpenOutputFileOrDie(MiscUtil::GetEnv("HOME") + "/SPECS/" + PACKAGE_NAME + ".specs"));
    GenerateSpecs(specs.get(), PACKAGE_NAME, package_version, description, libraries);
    specs->close();
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        ::Usage("(--deb|--rpm) path_to_binary description [blacklisted_library1 .. blacklisted_libraryN]");

    bool build_deb;
    if (std::strcmp(argv[1], "--deb") == 0)
        build_deb = true;
    else if (std::strcmp(argv[1], "--rpm") == 0)
        build_deb = false;
    else
        LOG_ERROR("first argument must be --deb or --rpm!");

    const std::string binary_path(argv[2]);
    if (not FileUtil::Exists(binary_path))
        LOG_ERROR("file not found: " + binary_path);

    const std::string description(argv[3]);

    std::set<std::string> blacklist;
    for (int arg_no(4); arg_no < argc; ++arg_no)
        blacklist.emplace(argv[arg_no]);

    std::vector<Library> libraries;
    GetLibraries(binary_path, blacklist, &libraries);

    const auto package_version(TimeUtil::GetCurrentDateAndTime("%Y.%m.%d"));

    if (build_deb)
        BuildDebPackage(binary_path, package_version, description, libraries);
    else
        BuildRPMPackage(binary_path, package_version, description, libraries);

    return EXIT_SUCCESS;
}
