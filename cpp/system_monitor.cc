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
#include <unordered_map>
#include "BinaryIO.h"
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


// Logs entries are written in the following binary format:
// <timestamp:4 bytes><ordinal:1 byte><value:4 bytes>
void WriteLogEntryToFile(const time_t timestamp, const uint8_t ordinal, const uint32_t value, File * const output_file) {
    // the timestamp is truncated to a 32-bit value before serialisation, will break in 2038
    const int32_t truncated_timestamp(static_cast<int32_t>(timestamp));
    BinaryIO::WriteOrDie(*output_file, truncated_timestamp);
    BinaryIO::WriteOrDie(*output_file, ordinal);
    BinaryIO::WriteOrDie(*output_file, value);
}


void CollectCPUStats(File * const log, const std::unordered_map<std::string, uint8_t> &label_to_ordinal_map) {
    static auto proc_meminfo(FileUtil::OpenInputFileOrDie("/proc/stat"));
    const auto current_time(std::time(nullptr));
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
            WriteLogEntryToFile(current_time, label_to_ordinal_map.at("CPU"), diff_usage, log);
            log->flush();
            last_total = total;
            last_idle = idle;
            return;
        }
    }
}


void CollectMemoryStats(File * const log, const std::unordered_map<std::string, uint8_t> &label_to_ordinal_map) {
    static const auto proc_meminfo(FileUtil::OpenInputFileOrDie("/proc/meminfo"));

    const auto current_time(std::time(nullptr));
    std::string line;
    while (proc_meminfo->getline(&line) > 0) {
        const auto first_colon_pos(line.find(':'));
        if (unlikely(first_colon_pos == std::string::npos))
            LOG_ERROR("missing colon in \"" + line + "\"!");
        const auto label(line.substr(0, first_colon_pos));
        if (label_to_ordinal_map.find(label) == label_to_ordinal_map.cend())
            continue;

        auto rest(StringUtil::LeftTrim(line.substr(first_colon_pos + 1)));
        const auto first_space_pos(rest.find(' '));
        if (first_space_pos != std::string::npos)
            rest = rest.substr(0, first_space_pos);

        WriteLogEntryToFile(current_time, label_to_ordinal_map.at(label), StringUtil::ToUnsigned(rest), log);
    }
    log->flush();

    proc_meminfo->rewind();
}


void CollectDiscStats(File * const log, const std::unordered_map<std::string, uint8_t> &label_to_ordinal_map) {
    const auto current_time(std::time(nullptr));
    FileUtil::Directory directory("/sys/block", "sd?");
    for (const auto &entry : directory) {
        if (label_to_ordinal_map.find(entry.getName()) == label_to_ordinal_map.end())
            LOG_ERROR("hard disk partition '" + entry.getName() + "' does not have an ordinal");

        const auto block_device_path("/sys/block/" + entry.getName() + "/size");
        const auto proc_entry(FileUtil::OpenInputFileOrDie(block_device_path));
        std::string line;
        proc_entry->getline(&line);
        const auto free_space(StringUtil::ToUnsignedLong(line) * 512 / 1024);   // in kilobytes

        WriteLogEntryToFile(current_time, label_to_ordinal_map.at(entry.getName()), free_space, log);
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


void CheckStats(const uint64_t ticks, const unsigned stats_interval,
                void (*stats_func)(File * const, const std::unordered_map<std::string, uint8_t> &),
                File * const log, const std::unordered_map<std::string, uint8_t> &label_to_ordinal_map)
{
    if ((ticks % stats_interval) == 0) {
        SignalUtil::SignalBlocker sighup_blocker(SIGHUP);
        stats_func(log, label_to_ordinal_map);
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

    std::unordered_map<std::string, uint8_t> label_to_ordinal_map;
    for (const auto &entry : *ini_file.getSection("Label Ordinals")) {
        if (entry.name_.empty())
            continue;
        if (label_to_ordinal_map.find(entry.name_) != label_to_ordinal_map.cend())
            LOG_ERROR("multiple ordinals assigned to label '" + entry.value_ + "'");

        label_to_ordinal_map[entry.name_] = StringUtil::ToUnsigned(entry.value_);
    }

    if (not foreground) {
        SignalUtil::InstallHandler(SIGTERM, SigTermHandler);

        if (::daemon(0, 1 /* do not close file descriptors and redirect to /dev/null */) != 0)
            LOG_ERROR("we failed to daemonize our process!");
    }
    if (not FileUtil::WriteString(PID_FILE, StringUtil::ToString(::getpid())))
        LOG_ERROR("failed to write our PID to " + PID_FILE + "!");

    const auto log(FileUtil::OpenForAppendingOrDie(argv[1]));

    uint64_t ticks(0);
    for (;;) {
        CheckStats(ticks, memory_stats_interval, CollectMemoryStats, log.get(), label_to_ordinal_map);
        CheckStats(ticks, disc_stats_interval, CollectDiscStats, log.get(), label_to_ordinal_map);
        CheckStats(ticks, cpu_stats_interval, CollectCPUStats, log.get(), label_to_ordinal_map);

        ::sleep(1);
        ++ticks;
    }
}
