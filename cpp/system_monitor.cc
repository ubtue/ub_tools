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
#include "TimeUtil.h"
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


// Returns local time using an ISO 8601 format w/o time zone.
inline std::string GetLocalTime() {
    return TimeUtil::GetCurrentDateAndTime(TimeUtil::ISO_8601_FORMAT);
}


void CollectCPUStats(File * const log) {
    static auto proc_meminfo(FileUtil::OpenInputFileOrDie("/proc/stat"));
    const auto current_date_and_time(GetLocalTime());
    static uint64_t last_total, last_idle;
    std::string line;
    while (proc_meminfo->getline(&line) > 0) {
        if (StringUtil::StartsWith(line, "cpu ")) {
            std::vector<std::string> parts;
            StringUtil::Split(line, ' ', &parts, /* suppress_empty_components = */true);
            uint64_t total(0), idle(StringUtil::ToUInt64T(parts[4]));
            for (unsigned i(1); i < parts.size(); ++i)
                total += StringUtil::ToUInt64T(parts[i]);
            const uint64_t diff_idle(idle - last_idle);
            const uint64_t diff_total(total - last_total);
            const uint64_t diff_usage((1000ull * (diff_total - diff_idle) / diff_total + 5) / 10ull);
            (*log) << "CPU " << diff_usage << ' ' << current_date_and_time << '\n';
            log->flush();
            last_total = total;
            last_idle = idle;
            return;
        }
    }
}


void CollectMemoryStats(File * const log) {
    static auto proc_meminfo(FileUtil::OpenInputFileOrDie("/proc/meminfo"));

    const auto current_date_and_time(GetLocalTime());
    std::string line;
    while (proc_meminfo->getline(&line) > 0) {
        const auto first_colon_pos(line.find(':'));
        if (unlikely(first_colon_pos == std::string::npos))
            LOG_ERROR("missing colon in \"" + line + "\"!");
        const auto label(line.substr(0, first_colon_pos));

        const auto rest(StringUtil::LeftTrim(line.substr(first_colon_pos + 1)));
        const auto first_space_pos(rest.find(' '));
        if (first_space_pos == std::string::npos)
            (*log) << label << ' ' << rest << ' ' << current_date_and_time << '\n';
        else
            (*log) << label << ' ' << rest.substr(0, first_space_pos) << ' ' << current_date_and_time << '\n';
    }
    log->flush();

    proc_meminfo->rewind();
}


void CollectDiscStats(File * const log) {
    const auto current_date_and_time(GetLocalTime());
    FileUtil::Directory directory("/sys/block", "sd?");
    for (const auto &entry : directory) {
        const auto block_device_path("/sys/block/" + entry.getName() + "/size");
        const auto proc_entry(FileUtil::OpenInputFileOrDie(block_device_path));
        std::string line;
        proc_entry->getline(&line);
        (*log) << block_device_path <<  ' ' << StringUtil::ToUnsignedLong(line) * 512 << ' ' << current_date_and_time << '\n';
    }
    log->flush();
}


const std::string PID_FILE("/usr/local/run/system_monitor.pid");


bool IsAlreadyRunning(std::string * const pid_as_string) {
    if (not FileUtil::Exists(PID_FILE))
        return false;

    FileUtil::ReadString(PID_FILE, pid_as_string);
    pid_t pid;
    if (not StringUtil::ToNumber(*pid_as_string, &pid))
        LOG_ERROR("\"" + *pid_as_string + "\" is not a valid PID!");

    return ::getpgid(pid) >= 0;
}


void CheckStats(const uint64_t ticks, const unsigned stats_interval, void (*stats_func)(File * const log), File * const log) {
    if ((ticks % stats_interval) == 0) {
        SignalUtil::SignalBlocker sighup_blocker(SIGHUP);
        stats_func(log);
    }
    CheckForSigTermAndExitIfSeen();
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

    std::string pid;
    if (IsAlreadyRunning(&pid)) {
        std::cerr << "system_monitor: This service may already be running! (PID: "<< pid << ")\n";
        return EXIT_FAILURE;
    }

    const IniFile ini_file(UBTools::GetTuelibPath() + FileUtil::GetBasename(::progname) + ".conf");

    const unsigned memory_stats_interval(ini_file.getUnsigned("", "memory_stats_interval"));
    const unsigned disc_stats_interval(ini_file.getUnsigned("", "disc_stats_interval"));
    const unsigned cpu_stats_interval(ini_file.getUnsigned("", "cpu_stats_interval"));

    if (not foreground) {
        SignalUtil::InstallHandler(SIGTERM, SigTermHandler);

        if (::daemon(0, 1 /* do not close file descriptors and redirect to /dev/null */) != 0)
            LOG_ERROR("we failed to deamonize our process!");
    }
    if (not FileUtil::WriteString(PID_FILE, StringUtil::ToString(::getpid())))
        LOG_ERROR("failed to write our PID to " + PID_FILE + "!");

    const auto log(FileUtil::OpenForAppendingOrDie(argv[1]));

    uint64_t ticks(0);
    for (;;) {
        CheckStats(ticks, memory_stats_interval, CollectMemoryStats, log.get());
        CheckStats(ticks, disc_stats_interval, CollectDiscStats, log.get());
        CheckStats(ticks, cpu_stats_interval, CollectCPUStats, log.get());

        ::sleep(1);
        ++ticks;
    }
}
