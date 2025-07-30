/** \file    ExecUtil.cc
 *  \brief   Implementation of the ExecUtil class.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *  Copyright 2017-2020 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ExecUtil.h"
#include <stdexcept>
#include <unordered_map>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "FileUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


// The following variables are set in Execute.
static bool alarm_went_off;
pid_t child_pid;


// SigAlarmHandler -- Used by Execute.
//
void SigAlarmHandler(int /* sig_no */) {
    alarm_went_off = true;
}


bool IsExecutableFile(const std::string &path) {
    struct stat statbuf;
    return ::stat(path.c_str(), &statbuf) == 0 and (statbuf.st_mode & S_IXUSR);
}


enum class ExecMode {
    WAIT,  //< Exec() will wait for the child to exit.
    DETACH //< Exec() will not wait for the child to exit and will return the child's PID.
};


int Exec(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin, const std::string &new_stdout,
         const std::string &new_stderr, const ExecMode exec_mode, unsigned timeout_in_seconds, const int tardy_child_signal,
         const std::unordered_map<std::string, std::string> &envs, const std::string &working_directory) {
    errno = 0;
    if (::access(command.c_str(), X_OK) != 0)
        throw std::runtime_error("in ExecUtil::Exec: can't execute \"" + command + "\"!");

    if (exec_mode == ExecMode::DETACH and timeout_in_seconds > 0)
        throw std::runtime_error("in ExecUtil::Exec: non-zero timeout is incompatible w/ ExecMode::DETACH!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
        throw std::runtime_error("in Exec: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
        // Make us the leader of a new process group:
        if (::setsid() == static_cast<pid_t>(-1))
            logger->error("in Exec(): child failed to become a new session leader!");

        if (not new_stdin.empty()) {
            const int new_stdin_fd(::open(new_stdin.c_str(), O_RDONLY));
            if (new_stdin_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stdin_fd, STDIN_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stdin_fd);
        }

        if (not new_stdout.empty()) {
            const int new_stdout_fd(::open(new_stdout.c_str(), O_WRONLY | O_CREAT, 0644));
            if (new_stdout_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stdout_fd, STDOUT_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stdout_fd);
        }

        if (not new_stderr.empty()) {
            const int new_stderr_fd(::open(new_stderr.c_str(), O_WRONLY | O_CREAT, 0644));
            if (new_stderr_fd == -1)
                ::_exit(-1);
            if (::dup2(new_stderr_fd, STDERR_FILENO) == -1)
                ::_exit(-1);
            ::close(new_stderr_fd);
        }

        // Set environment variables
        for (const auto &env : envs)
            ::setenv(env.first.c_str(), env.second.c_str(), 1);

        // Change working directory
        if (not working_directory.empty()) {
            if (::chdir(working_directory.c_str()) == -1)
                throw std::runtime_error("in ExecUtil::Exec: ::chdir() failed: " + std::to_string(errno));
        }

// Build the argument list for execve(2):
#pragma GCC diagnostic ignored "-Wvla"
        char *argv[1 + args.size() + 1];
#ifndef __clang__
#pragma GCC diagnostic ignored "+Wvla"
#endif
        unsigned arg_no(0);
        argv[arg_no++] = ::strdup(command.c_str());
        for (const auto &arg : args)
            argv[arg_no++] = ::strdup(arg.c_str());
        argv[arg_no] = nullptr;
        ::execv(command.c_str(), argv);

        ::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else
    {
        if (exec_mode == ExecMode::DETACH)
            return pid;

        void (*old_alarm_handler)(int) = nullptr;

        if (timeout_in_seconds > 0) {
            // Install new alarm handler...
            alarm_went_off = false;
            child_pid = pid;
            old_alarm_handler = ::signal(SIGALRM, SigAlarmHandler);

            // ...and wind the clock:
            ::alarm(timeout_in_seconds);
        }

        int child_exit_status;
        errno = 0;
        int wait_retval = ::wait4(pid, &child_exit_status, 0, nullptr);
        assert(wait_retval == pid or errno == EINTR);

        if (timeout_in_seconds > 0) {
            // Cancel any outstanding alarm:
            ::alarm(0);

            // Restore the old alarm handler:
            ::signal(SIGALRM, old_alarm_handler);

            // Check to see whether the test timed out or not:
            if (alarm_went_off) {
                // Snuff out all of our offspring.
                ::kill(-pid, tardy_child_signal);
                while (::wait4(-pid, &child_exit_status, 0, nullptr) != -1)
                    /* Intentionally empty! */;

                errno = ETIME;
                return -1;
            }
        }

        // Now process the child's various exit status values:
        if (WIFEXITED(child_exit_status)) {
            switch (WEXITSTATUS(child_exit_status)) {
            case EXECVE_FAILURE:
                throw std::runtime_error("in Exec: failed to execve(2) in child!");
            default:
                return WEXITSTATUS(child_exit_status);
            }
        } else if (WIFSIGNALED(child_exit_status))
            throw std::runtime_error("in Exec: \"" + command + "\" killed by signal " + std::to_string(WTERMSIG(child_exit_status)) + "!");
        else // I have no idea how we got here!
            logger->error("in Exec: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}


} // unnamed namespace


namespace ExecUtil {


SignalBlocker::SignalBlocker(const int signal_to_block) {
    sigset_t new_set;
    ::sigemptyset(&new_set);
    ::sigaddset(&new_set, signal_to_block);
    if (::sigprocmask(SIG_BLOCK, &new_set, &saved_set_) != 0)
        logger->error("in ExecUtil::SignalBlocker::SignalBlocker: call to sigprocmask(2) failed!");
}


SignalBlocker::~SignalBlocker() {
    if (::sigprocmask(SIG_SETMASK, &saved_set_, nullptr) != 0)
        logger->error("in ExecUtil::SignalBlocker::~SignalBlocker: call to sigprocmask(2) failed!");
}


int Exec(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin, const std::string &new_stdout,
         const std::string &new_stderr, const unsigned timeout_in_seconds, const int tardy_child_signal,
         const std::unordered_map<std::string, std::string> &envs, const std::string &working_directory) {
    return ::Exec(command, args, new_stdin, new_stdout, new_stderr, ExecMode::WAIT, timeout_in_seconds, tardy_child_signal, envs,
                  working_directory);
}


void ExecOrDie(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin,
               const std::string &new_stdout, const std::string &new_stderr, const unsigned timeout_in_seconds,
               const int tardy_child_signal, const std::unordered_map<std::string, std::string> &envs,
               const std::string &working_directory) {
    int exit_code;
    if ((exit_code =
             Exec(command, args, new_stdin, new_stdout, new_stderr, timeout_in_seconds, tardy_child_signal, envs, working_directory))
        != 0)
    {
        LOG_ERROR("Failed to execute \"" + command + "\""
                  " with args \"" + StringUtil::Join(args, ";") + "\"!"
                  " (exit code was " + std::to_string(exit_code) + ")");
    }
}


pid_t Spawn(const std::string &command, const std::vector<std::string> &args, const std::string &new_stdin, const std::string &new_stdout,
            const std::string &new_stderr, const std::unordered_map<std::string, std::string> &envs, const std::string &working_directory) {
    return ::Exec(command, args, new_stdin, new_stdout, new_stderr, ExecMode::DETACH, 0, SIGKILL /* Not used because the timeout is 0. */,
                  envs, working_directory);
}


std::unordered_map<std::string, std::string> which_cache;


std::string Which(const std::string &executable_candidate) {
    auto which_cache_entry = which_cache.find(executable_candidate);
    if (which_cache_entry != which_cache.cend())
        return which_cache[executable_candidate];

    std::string executable;

    const size_t last_slash_pos(executable_candidate.find_last_of('/'));
    if (last_slash_pos != std::string::npos) {
        if (not IsExecutableFile(executable_candidate))
            return "";
        executable = executable_candidate;
    }

    if (executable.empty()) {
        const auto PATH(MiscUtil::SafeGetEnv("PATH"));
        if (PATH.empty())
            return "";

        std::vector<std::string> path_compoments;
        StringUtil::Split(PATH, ':', &path_compoments, /* suppress_empty_components = */ true);
        for (const auto &path_compoment : path_compoments) {
            const std::string full_path(path_compoment + "/" + executable_candidate);
            if (IsExecutableFile(full_path)) {
                executable = full_path;
                break;
            }
        }
    }

    if (executable.empty())
        return "";
    else {
        which_cache[executable_candidate] = executable;
        return executable;
    }
}


std::string LocateOrDie(const std::string &executable_candidate) {
    const std::string path(ExecUtil::Which(executable_candidate));
    if (path.empty())
        logger->error("in ExecUtil::LocateOrDie: can't find \"" + executable_candidate + "\" in our PATH environment!");
    return path;
}


bool ExecSubcommandAndCaptureStdout(const std::string &command, std::string * const stdout_output, const bool suppress_stderr) {
    stdout_output->clear();

    FILE * const subcommand_stdout(::popen((command + (suppress_stderr ? " 2>/dev/null" : "")).c_str(), "r"));
    if (subcommand_stdout == nullptr)
        return false;

    int ch;
    while ((ch = std::getc(subcommand_stdout)) != EOF)
        *stdout_output += static_cast<char>(ch);

    const int ret_code(::pclose(subcommand_stdout));
    if (ret_code == -1)
        LOG_ERROR("pclose(3) failed: " + std::string(::strerror(errno)));

    return WEXITSTATUS(ret_code) == 0;
}


bool ExecSubcommandAndCaptureStdoutAndStderr(const std::string &command, const std::vector<std::string> &args,
                                             std::string * const stdout_output, std::string * const stderr_output,
                                             const unsigned timeout_in_seconds, const int tardy_child_signal,
                                             const std::unordered_map<std::string, std::string> &envs,
                                             const std::string &working_directory) {
    FileUtil::AutoTempFile stdout_temp;
    FileUtil::AutoTempFile stderr_temp;

    const int retcode(Exec(command, args, /* new_stdin = */ "", stdout_temp.getFilePath(), stderr_temp.getFilePath(), timeout_in_seconds,
                           tardy_child_signal, envs, working_directory));

    if (not FileUtil::ReadString(stdout_temp.getFilePath(), stdout_output))
        LOG_ERROR("failed to read temporary file w/ stdout contents!");
    if (not FileUtil::ReadString(stderr_temp.getFilePath(), stderr_output))
        LOG_ERROR("failed to read temporary file w/ stderr contents!");

    return retcode == 0;
}


bool ShouldScheduleNewProcess() {
    static const long NO_OF_CORES(::sysconf(_SC_NPROCESSORS_ONLN));
    double loadavg;
    if (unlikely(::getloadavg(&loadavg, 1) == -1))
        LOG_ERROR("getloadavg(3) failed!");
    return loadavg < NO_OF_CORES - 0.5;
}


void FindActivePrograms(const std::string &program_name, std::unordered_set<unsigned> * const pids) {
    pids->clear();

    FILE * const subcommand_stdout(::popen(("pgrep " + program_name + " 2>/dev/null").c_str(), "r"));
    if (subcommand_stdout == nullptr)
        LOG_ERROR(
            "failed to execute \""
            "pgrep "
            + program_name + "\"!");

    std::string stdout;
    int ch;
    while ((ch = std::getc(subcommand_stdout)) != EOF)
        stdout += static_cast<char>(ch);

    const int ret_code(::pclose(subcommand_stdout));
    if (ret_code == -1)
        LOG_ERROR("pclose(3) failed: " + std::string(::strerror(errno)));

    switch (WEXITSTATUS(ret_code)) {
    case 0: // We found some PIDs.
        break;
    case 1: // No processes matched.
        return;
    case 2:
        LOG_ERROR("pgrep: Syntax error in the command line.");
    case 3:
        LOG_ERROR("pgrep: Fatal error: out of memory etc.");
    default:
        LOG_ERROR("unexpected exit code from pgrep!");
    }

    std::unordered_set<std::string> pids_strings;
    StringUtil::Split(stdout, '\n', &pids_strings, /* suppress_empty_components = */ true);

    for (const auto &pid : pids_strings)
        pids->emplace(StringUtil::ToUnsigned(pid));
}


std::unordered_set<unsigned> FindActivePrograms(const std::string &program_name) {
    std::unordered_set<unsigned> pids;
    FindActivePrograms(program_name, &pids);
    return pids;
}


bool SetProcessName(char *argv0, const std::string &new_process_name) {
    if (new_process_name.length() > std::strlen(argv0))
        return false;
    constexpr size_t MAX_PR_SET_NAME_LENGTH(16 - 1);
    if (new_process_name.length() > MAX_PR_SET_NAME_LENGTH)
        return false;

    std::memset(argv0, 0, std::strlen(argv0));
    std::strcpy(argv0, new_process_name.c_str());
    return ::prctl(PR_SET_NAME, (unsigned long)new_process_name.c_str(), 0) == 0;
}


std::string GetOriginalCommandNameFromPID(const pid_t pid) {
    std::string ps_path;
    if (FileUtil::Exists("/bin/ps"))
        ps_path = "/bin/ps";
    else if (FileUtil::Exists("/usr/bin/ps"))
        ps_path = "/usr/bin/ps";
    else
        LOG_ERROR("Neither /bin/ps nor /usr/bin/ps can be found!");

    std::string stdout_output;
    if (not ExecSubcommandAndCaptureStdout(ps_path + " --pid " + std::to_string(pid) + " --no-headers -o comm", &stdout_output,
                                           /* suppress_stderr = */ true))
        return "";
    else
        return StringUtil::EndsWith(stdout_output, "\n") ? stdout_output.substr(0, stdout_output.length() - 1) : stdout_output;
}


} // namespace ExecUtil
