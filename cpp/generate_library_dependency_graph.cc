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

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--debug] absolute_library_paths");
}


struct LibraryAndSymbols {
    std::string library_path_;
    std::unordered_set<std::string> provided_, needed_;

public:
    LibraryAndSymbols() = default;
    LibraryAndSymbols(const LibraryAndSymbols &other) = default;
    explicit LibraryAndSymbols(const std::string &library_path): library_path_(library_path) { }
};


void ProcessLine(const bool debug, const std::string &line, LibraryAndSymbols * const library_and_symbols,
                 std::unordered_set<std::string> * const all_provided) {
    std::vector<std::string> parts;
    StringUtil::SplitThenTrimWhite(line, ' ', &parts);
    if (parts.size() == 2 and parts[1] != "_GLOBAL_OFFSET_TABLE_")
        library_and_symbols->needed_.emplace(parts[1] + (debug ? " (" + parts[0] + ")" : ""));
    else if (parts.size() == 3) {
        all_provided->emplace(parts[2] + (debug ? " (U)" : ""));
        if (parts[1] == "T")
            library_and_symbols->provided_.emplace(parts[2] + (debug ? " (" + parts[1] + ")" : ""));
    }
}


void ExtractSymbols(const bool debug, LibraryAndSymbols * const library_and_symbols) {
    std::string stdout;
    const std::string COMMAND("nm " + library_and_symbols->library_path_);
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(COMMAND, &stdout, /* suppress_stderr = */ true))
        LOG_ERROR("failed to execute \"" + COMMAND + "\"!");

    std::unordered_set<std::string> all_provided;

    std::string line;
    for (const char ch : stdout) {
        if (ch == '\n') {
            ProcessLine(debug, line, library_and_symbols, &all_provided);
            line.clear();
        } else
            line += ch;
    }

    if (not line.empty())
        ProcessLine(debug, line, library_and_symbols, &all_provided);

    // Remove referenced symbols that are implemented in the library themselves:
    for (const auto &symbol : all_provided)
        library_and_symbols->needed_.erase(symbol);

    LOG_DEBUG(library_and_symbols->library_path_ + " provided: " + std::to_string(library_and_symbols->provided_.size())
              + " needed: " + std::to_string(library_and_symbols->needed_.size()));
}


void ListSymbols(const std::string &library_path, const std::string &description, const std::unordered_set<std::string> &symbols) {
    std::vector<std::string> sorted_symbols;
    sorted_symbols.reserve(symbols.size());

    for (const auto &symbol : symbols)
        sorted_symbols.emplace_back(symbol);
    std::sort(sorted_symbols.begin(), sorted_symbols.end());

    std::cout << FileUtil::GetBasename(library_path) << " (" << description << ")\n";
    for (const auto &symbol : sorted_symbols)
        std::cout << "    " << symbol << '\n';
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc == 1)
        Usage();

    bool debug(false);
    if (argc > 1 and std::strcmp("--debug", argv[1]) == 0) {
        debug = true;
        logger->setMinimumLogLevel(Logger::LL_DEBUG);
        --argc, ++argv;
    }
    if (argc < 1)
        Usage();

    std::vector<LibraryAndSymbols> libraries_and_symbols;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        LibraryAndSymbols new_library_and_symbols(argv[arg_no]);
        ExtractSymbols(debug, &new_library_and_symbols);
        if (debug) {
            ListSymbols(new_library_and_symbols.library_path_, "defined", new_library_and_symbols.provided_);
            ListSymbols(new_library_and_symbols.library_path_, "referenced", new_library_and_symbols.needed_);
        } else
            libraries_and_symbols.emplace_back(new_library_and_symbols);
    }

    if (debug) {
        std::unordered_set<std::string> found_external_references;
        for (const auto &lib1 : libraries_and_symbols) {
            for (const auto &lib2 : libraries_and_symbols) {
                if (lib1.library_path_ == lib2.library_path_)
                    continue;

                for (const auto &external_symbol : lib1.needed_) {
                    if (lib2.provided_.find(external_symbol) != lib2.provided_.cend())
                        found_external_references.emplace(external_symbol);
                }
            }
        }

        std::cout << "Missing external references:\n";
        for (const auto &lib : libraries_and_symbols) {
            for (const auto &external_symbol : lib.needed_) {
                if (found_external_references.find(external_symbol) == found_external_references.cend())
                    std::cout << external_symbol << " (" << FileUtil::GetBasename(lib.library_path_) << ")\n";
            }
        }
    } else {
        for (const auto &lib1 : libraries_and_symbols) {
            for (const auto &lib2 : libraries_and_symbols) {
                if (lib1.library_path_ == lib2.library_path_)
                    continue;

                for (const auto &external_symbol : lib1.needed_) {
                    if (lib2.provided_.find(external_symbol) != lib2.provided_.cend()) {
                        std::cout << FileUtil::GetLastPathComponent(lib1.library_path_) << " -> "
                                  << FileUtil::GetLastPathComponent(lib2.library_path_) << '\n';
                        break;
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
