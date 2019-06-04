/** \brief Utility for generating a dependency graph for externally-referenced symbols.
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
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


struct LibraryAndSymbols {
    std::string library_path_;
    std::unordered_set<std::string> provided_, needed_;
public:
    LibraryAndSymbols() = default;
    LibraryAndSymbols(const LibraryAndSymbols &other) = default;
    explicit LibraryAndSymbols(const std::string &library_path): library_path_(library_path) { }
};


void ProcessLine(const std::string &line, LibraryAndSymbols * const library_and_symbols) {
    std::vector<std::string> parts;
    StringUtil::SplitThenTrimWhite(line, ' ', &parts);
    if (parts.size() == 2 and (parts[0] == "U" or parts[0] == "u"))
        library_and_symbols->needed_.emplace(parts[1]);
    else if (parts.size() == 3
             and (parts[1] == "T" or parts[1] == "t" or parts[0] == "W" or parts[0] == "w" or parts[0] == "D"
                  or parts[0] == "d" or parts[0] == "B" or parts[0] == "b" or parts[0] == "V" or parts[0] == "v"))
        library_and_symbols->provided_.emplace(parts[2]);
}


void ExtractSymbols(LibraryAndSymbols * const library_and_symbols) {
    std::string stdout;
    const std::string COMMAND("nm " + library_and_symbols->library_path_);
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(COMMAND, &stdout, /* suppress_stderr = */true))
        LOG_ERROR("failed to execute \"" + COMMAND + "\"!");

    std::string line;
    for (const char ch : stdout) {
        if (ch == '\n') {
            ProcessLine(line, library_and_symbols);
            line.clear();
        } else
            line += ch;
    }

    if (not line.empty())
        ProcessLine(line, library_and_symbols);

    LOG_DEBUG(library_and_symbols->library_path_ + " provided: " + std::to_string(library_and_symbols->provided_.size())
              + " needed: " + std::to_string(library_and_symbols->needed_.size()));
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc == 0)
        ::Usage("absolute_library_paths");

    std::vector<LibraryAndSymbols> libraries_and_symbols;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        LibraryAndSymbols new_library_and_symbols(argv[arg_no]);
        ExtractSymbols(&new_library_and_symbols);
        libraries_and_symbols.emplace_back(new_library_and_symbols);
    }

    for (const auto &lib1 : libraries_and_symbols) {
        for (const auto &lib2 : libraries_and_symbols) {
            if (lib1.library_path_ == lib2.library_path_)
                continue;

            for (const auto &external_symbol : lib1.needed_) {
                if (lib2.provided_.find(external_symbol) != lib2.provided_.cend())
                    std::cout << FileUtil::GetLastPathComponent(lib1.library_path_) << " -> "
                              << FileUtil::GetLastPathComponent(lib2.library_path_) << '\n';
                break;
            }
        }
    }

    return EXIT_SUCCESS;
}
