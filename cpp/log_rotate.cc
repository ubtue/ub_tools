/** \file   log_rotate.cc
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <csignal>
#include <cstdlib>
#include <cstring>
#include "FileLocker.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "ProcessUtil.h"
#include "RegexMatcher.h"
#include "SELinuxUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


const unsigned DEFAULT_MAX_ROTATIONS(5);


[[noreturn]] void Usage() {
    ::Usage("[--verbose] [--max-rotations=max_rotations|--no-of-lines-to-keep=max_line_count] [--recreate] directory file_regex\n"
            "where the default for \"max_rotations\" is " + std::to_string(DEFAULT_MAX_ROTATIONS) + "\n"
            "if \"--recreate\" has been specified the original filename will be recreated with same owner, group,\n"
            "and, if appropriate, SELinux security context.\n"
            "\"file_regex\" must be a PCRE.  (There is no default for \"max_line_count\".)\n"
            "When using --no-of-lines-to-keep, the result will be either empty, if the original\n"
            "was empty, or the file will end in a newline even if it originally didn't.\n\n");
}


inline bool HasNumericExtension(const std::string &filename) {
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\\.[0-9]+$"));
    return matcher->matched(filename);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool verbose(false);
    if (std::strcmp(argv[1], "--verbose") == 0) {
        verbose = true;
        --argc, ++argv;
    }

    unsigned max_rotations(DEFAULT_MAX_ROTATIONS), max_line_count(0);
    if (StringUtil::StartsWith(argv[1], "--max-rotations=")) {
        if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--max-rotations="), &max_rotations) or max_rotations == 0)
            LOG_ERROR("\"" + std::string(argv[1] + std::strlen("--max-rotations=")) + "\" is not a valid maximum rotation count!");
        --argc, ++argv;
    } else if (StringUtil::StartsWith(argv[1], "--no-of-lines-to-keep=")) {
        if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--no-of-lines-to-keep="), &max_line_count) or max_line_count == 0)
            if (not StringUtil::ToUnsigned(argv[1] + std::strlen("--no-of-lines-to-keep="), &max_line_count) or max_line_count == 0)
                LOG_ERROR("\"" + std::string(argv[1] + std::strlen("--no-of-lines-to-keep=")) + "\" is not a valid line count!");
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    const bool recreate(__builtin_strcmp(argv[1], "--recreate") == 0);
    if (recreate)
        --argc, ++argv;

    if (argc != 3)
        Usage();
    const std::string directory_path(argv[1]), file_regex(argv[2]);

    FileUtil::Directory directory(directory_path, file_regex);
    for (const auto &entry : directory) {
        if (not HasNumericExtension(entry.getName())) {
            if (verbose)
                std::cout << "About to rotate \"" << entry.getName() << "\".\n";

            const std::string filename(entry.getFullName());
            if (max_line_count > 0) {
                const int fd(::open(filename.c_str(), O_RDWR));
                { // New scope to ensure that the FileLocker instance goes out of scope before we close fd!
                    FileLocker file_locker(fd, FileLocker::READ_WRITE);
                    FileUtil::OnlyKeepLastNLines(filename, max_line_count);
                }
                ::close(fd);
            } else {
                const auto pids(ProcessUtil::GetProcessIdsForPath(filename, /* exclude_self = */ true));
                MiscUtil::LogRotate(filename, max_rotations);
                if (recreate) {
                    FileUtil::TouchFileOrDie(filename);

                    const mode_t mode(entry.getFileTypeAndMode());
                    if (::chmod(filename.c_str(), mode & (~S_IFMT)))
                        LOG_ERROR("chmod(2) failed on \"" + filename + "\"!");

                    uid_t uid;
                    gid_t gid;
                    entry.getUidAndGid(&uid, &gid);
                    FileUtil::ChangeOwnerOrDie(filename, FileUtil::UsernameFromUID(uid), FileUtil::GroupnameFromGID(gid));
                    if (SELinuxUtil::IsEnabled())
                        SELinuxUtil::FileContext::ApplyChanges(filename);

                    for (const auto pid : pids)
                        ::kill(pid, SIGHUP);
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
