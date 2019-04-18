/** \file   system_monitor.cc
 *  \brief  Collect a few basic system metrics
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
#include <csignal>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "SignalUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--foreground] output_filename\n"
            "       When --foreground has been specified the program does not daemonise.\n"
            "       The config file path is \"" + UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf\".");
}


volatile sig_atomic_t sigterm_seen = false;


void SigTermHandler(int /* signum */) {
    sigterm_seen = true;
}


void CheckForSigTermAndExitIfSeen() {
    if (sigterm_seen) {
        LOG_WARNING("caught SIGTERM, exiting...");
        std::exit(EXIT_SUCCESS);
    }
}


void CollectMemoryStats(File * const log) {
    static auto proc_meminfo(FileUtil::OpenInputFileOrDie("/proc/meminfo"));

    std::string line;
    while (proc_meminfo->getline(&line) > 0) {
        const auto first_colon_pos(line.find(':'));
        if (unlikely(first_colon_pos == std::string::npos))
            LOG_ERROR("missing colon in \"" + line + "\"!");
        const auto label(line.substr(0, first_colon_pos));

        const auto rest(StringUtil::LeftTrim(line.substr(first_colon_pos + 1)));
        const auto first_space_pos(rest.find(' '));
        if (first_space_pos == std::string::npos)
            (*log) << label << ' ' << rest << '\n';
        else
            (*log) << label << ' ' << rest.substr(0, first_space_pos) << '\n';
    }
    log->flush();

    proc_meminfo->rewind();
}


void CollectDiscStats(File * const log) {
    FileUtil::Directory directory("/sys/block", "sd?");
    for (const auto &entry : directory) {
        const auto block_device_path("/sys/block/" + entry.getName() + "/size");
        const auto proc_entry(FileUtil::OpenInputFileOrDie(block_device_path));
        std::string line;
        proc_entry->getline(&line);
        (*log) << block_device_path <<  ' ' << StringUtil::ToUnsignedLong(line) * 512 << '\n';
    }
    log->flush();
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool foreground(false);
    if (std::strcmp(argv[1], "--foreground") == 0) {
        foreground = true;
        --argc, ++argv;
    }
    if (argc != 2)
        Usage();

    const IniFile ini_file(UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf");

    const unsigned memory_stats_interval(ini_file.getUnsigned("", "memory_stats_interval"));
    const unsigned disc_stats_interval(ini_file.getUnsigned("", "disc_stats_interval"));

    if (not foreground) {
        SignalUtil::InstallHandler(SIGTERM, SigTermHandler);

        if (::daemon(0, 1 /* do not close file descriptors and redirect to /dev/null */) != 0)
            LOG_ERROR("we failed to deamonize our process!");
    }

    const auto log(FileUtil::OpenForAppendingOrDie(argv[1]));

    uint64_t ticks(0);
    for (;;) {
        if ((ticks % memory_stats_interval) == 0) {
            SignalUtil::SignalBlocker sighup_blocker(SIGHUP);
            CollectMemoryStats(log.get());
        }
        CheckForSigTermAndExitIfSeen();

        if ((ticks % disc_stats_interval) == 0) {
            SignalUtil::SignalBlocker sighup_blocker(SIGHUP);
            CollectDiscStats(log.get());
        }
        CheckForSigTermAndExitIfSeen();

        ::sleep(1);
        ++ticks;
    }
}
