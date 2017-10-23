/** \file   log_rotate.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include "FileUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


const unsigned DEFAULT_MAX_ROTATIONS(5);


void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--max-rotations=max_rotations|--no-of-lines-to-keep=max_line_count] directory file_regex\n"
              << "       where the default for \"max_rotations\" is " << DEFAULT_MAX_ROTATIONS << '\n'
              << "       and \"file_regex\" must be a PCRE.  (There is no default for \"max_line_count\".)\n"
              << "       When using --no-of-lines-to-keep, the result will be either empty, if the original\n"
              << "       was empty, or the file will end in a newline even if it originally didn't.\n\n";
    std::exit(EXIT_FAILURE);
}


void SkipLines(File * const input, unsigned skip_count) {
    while (skip_count > 0) {
        std::string line;
        input->getline(&line);
        --skip_count;
    }
}


void CopyLines(File * const input, File * const output) {
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        if (unlikely(not output->write(line + "\n")))
            logger->error("in CopyLines: failed to write a line to \"" + output->getPath() + "\"!");
    }
}


// Keeps the last \"max_line_count\" number of lines in \"filename\".
void KeepLines(const std::string &filename, const unsigned &max_line_count) {
    const size_t original_line_count(FileUtil::CountLines(filename));
    if (original_line_count <= max_line_count)
        return;

    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(filename));
    SkipLines(input.get(), original_line_count - max_line_count);

    FileUtil::AutoTempFile temp_file;
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(temp_file.getFilePath()));
    CopyLines(input.get(), output.get());
    input->close(), output->close();
    if (not FileUtil::RenameFile(temp_file.getFilePath(), filename, /* remove_target = */ true))
        logger->error("in KeepLines: failed to rename \"" + temp_file.getFilePath() + "\" to \"" + filename + "\"!");
}


inline bool HasNumericExtension(const std::string &filename) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\\.[0-9]+$"));
    return matcher->matched(filename);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 4)
        Usage();

    unsigned max_rotations(DEFAULT_MAX_ROTATIONS), max_line_count(0);
    std::string directory_path, file_regex;
    if (argc == 3) {
        directory_path = argv[1];
        file_regex     = argv[2];
    } else { // argc == 4.
        if (StringUtil::StartsWith(argv[1], "--max-rotations=")) {
            if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--max-rotations="), &max_rotations)
                or max_rotations == 0)
                logger->error("\"" + std::string(argv[1] + std::strlen("--max-rotations="))
                              + "\" is not a valid maximum rotation count!");
        } else if (StringUtil::StartsWith(argv[1], "--no-of-lines-to-keep=")) {
            if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--no-of-lines-to-keep="), &max_line_count)
                or max_line_count == 0)
                logger->error("\"" + std::string(argv[1] + std::strlen("--no-of-lines-to-keep="))
                              + "\" is not a valid line count!");
        } else
            Usage();
        directory_path = argv[2];
        file_regex     = argv[3];
    }

    try {
        FileUtil::Directory directory(directory_path, file_regex);
        for (const auto &entry : directory) {
            if (not HasNumericExtension(entry.getName())) {
                if (max_line_count > 0)
                    KeepLines(directory_path + "/" + entry.getName(), max_line_count);
                else
                    MiscUtil::LogRotate(entry.getName(), max_rotations);
            }
        }
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
