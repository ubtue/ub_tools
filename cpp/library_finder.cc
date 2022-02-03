/** \brief Utility for determining absolute paths to link libraries.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
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
#include <cstdlib>
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


const std::vector<std::string> DEFAULT_LIBRARY_PATH{
    "/usr/lib",
    "/usr/local/lib",
};


// \return The empty string if "library" was not found, o/w the library with the prepended path.
std::string FindLibraryHelper(const std::string &library_directory, const std::string &library) {
    FileUtil::Directory directory(library_directory);
    for (const auto entry : directory) {
        const auto entry_type(entry.getType());
        if (entry_type == DT_REG or entry_type == DT_LNK) {
            if (entry.getName() == library)
                return library_directory + "/" + library;
        } else if (entry_type == DT_DIR) {
            const auto subdirectory_name(entry.getName());
            if (subdirectory_name == "." or subdirectory_name == "..")
                continue;
            const auto resolved_name(FindLibraryHelper(library_directory + "/" + subdirectory_name, library));
            if (not resolved_name.empty())
                return resolved_name;
        }
    }

    return "";
}


// \return The empty string if "library" was not found, o/w the library with the prepended path.
std::string FindLibrary(const std::string &library_directory, const std::string &library) {
    if (not FileUtil::Exists(library_directory))
        return "";

    return FindLibraryHelper(library_directory, library);
}


void ProcessLibrary(const std::vector<std::string> &library_path, const std::string &library_option) {
    if (library_option.length() < 3)
        LOG_ERROR("weird library option: \"" + library_option + "\"!");

    const std::string library("lib" + library_option.substr(2 /* Strip off "-l". */) + ".a");

    for (const auto &directory : library_path) {
        if (unlikely(directory.empty()))
            LOG_ERROR("illegal empty library directory name!");
        const auto resolved_name(FindLibrary(directory, library));
        if (not resolved_name.empty()) {
            std::cout << resolved_name << '\n';
            return;
        }
    }

    LOG_ERROR("Library \"" + library + "\" not found in path \"" + StringUtil::Join(library_path, ':') + "\"!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc == 1)
        ::Usage("library_flags");

    for (int arg_no(1); arg_no < argc; ++arg_no) {
        if (__builtin_strncmp(argv[arg_no], "-L", 2) == 0) {
            ++arg_no;
            if (arg_no == argc)
                LOG_ERROR("last argument starts with -L!");

            ProcessLibrary({ argv[arg_no - 1] + 2 /* Skip over "-L". */ }, argv[arg_no]);
        } else
            ProcessLibrary(DEFAULT_LIBRARY_PATH, argv[arg_no]);
    }

    return EXIT_SUCCESS;
}
