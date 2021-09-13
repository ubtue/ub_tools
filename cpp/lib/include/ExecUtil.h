/** \file    ExecUtil.h
 *  \brief   The ExecUtil interface.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *  Copyright 2017-2019 Universitätsbibliothek Tübingen
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
#pragma once


#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include <signal.h>
#include <sys/types.h>
#include "StringUtil.h"


namespace ExecUtil {


/** \class SignalBlocker
 *  \brief Blocks a signal for the livetime of an instance of this class.
 */
class SignalBlocker {
    sigset_t saved_set_;
public:
    explicit SignalBlocker(const int signal_to_block);
    ~SignalBlocker();
};


/** \brief  Run a subcommand to completion.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  new_stdin           An optional replacement file path for stdin.
 *  \param  new_stdout          An optional replacement file path for stdout.
 *  \param  new_stderr          An optional replacement file path for stderr.
 *  \param  timeout_in_seconds  If not zero, the subprocess will be killed if the timeout expires before
 *                              the process terminates.
 *  \param  tardy_child_signal  The signal to send to our offspring if there was a timeout.
 *  \param  envs                The environment variables to be set in the child process.
 *  \param  working_directory   The working directory to be set in the child process.
 *  \note   in case of a timeout, we set errno to ETIME and return -1
 *  \return The exit code of the subcommand or an error code if there was a failure along the way.
 */
int Exec(const std::string &command, const std::vector<std::string> &args = std::vector<std::string>{}, const std::string &new_stdin = "",
         const std::string &new_stdout = "", const std::string &new_stderr = "", const unsigned timeout_in_seconds = 0,
         const int tardy_child_signal = SIGKILL,
         const std::unordered_map<std::string, std::string> &envs = std::unordered_map<std::string, std::string>(),
         const std::string &working_directory = "");

void ExecOrDie(const std::string &command, const std::vector<std::string> &args = std::vector<std::string>{},
               const std::string &new_stdin = "", const std::string &new_stdout = "", const std::string &new_stderr = "",
               const unsigned timeout_in_seconds = 0, const int tardy_child_signal = SIGKILL,
               const std::unordered_map<std::string, std::string> &envs = std::unordered_map<std::string, std::string>(),
               const std::string &working_directory = "");

/** \brief  Kicks off a subcommand and returns.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  new_stdin           An optional replacement file path for stdin.
 *  \param  new_stdout          An optional replacement file path for stdout.
 *  \param  new_stderr          An optional replacement file path for stderr.
 *  \param  envs                The environment variables to be set in the child process.
 *  \param  working_directory   The working directory to be set in the child process.
 *  \return The PID of the child.
 */
pid_t Spawn(const std::string &command, const std::vector<std::string> &args = std::vector<std::string>{}, const std::string &new_stdin = "",
            const std::string &new_stdout = "", const std::string &new_stderr = "",
            const std::unordered_map<std::string, std::string> &envs = std::unordered_map<std::string, std::string>(),
            const std::string &working_directory = "");


/** \brief Tries to find a path, with the help of the environment variable PATH, to "executable_candidate".
 *  \return The path where the executable can be found or the empty string if no such path was found or if
 *          "executable_candidate" is not executable.
 */
std::string Which(const std::string &executable_candidate);


/** \brief  Like Which(), only we abort if we can't find \"executable_candidate\".
 *  \return The path where the executable can be found.
 */
std::string LocateOrDie(const std::string &executable_candidate);


/** \brief  Retrieve the stdout of a subcommand.
 *  \param  command          A shell command.  Can include arguments. E.g. "ls -l".
 *  \param  stdout_output    Where to store the output of the command.
 *  \param  suppress_stderr  If true, stderr will be redirected to /dev/null.
 *  \note   The command will be executed by passing it to the standard shell interpreter: "/bin/sh -c command".
 */
bool ExecSubcommandAndCaptureStdout(const std::string &command, std::string * const stdout_output,
                                    const bool suppress_stderr = false);


/** \brief  Retrieve the stdout and stderr of a subcommand.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  stdout_output  Where to store the stdout of the command.
 *  \param  stderr_output  Where to store the stderr of the command.
 */
bool ExecSubcommandAndCaptureStdoutAndStderr(const std::string &command, const std::vector<std::string> &args,
                                             std::string * const stdout_output, std::string * const stderr_output,
                                             const unsigned timeout_in_seconds = 0, const int tardy_child_signal = SIGKILL,
                                             const std::unordered_map<std::string, std::string> &envs =
                                             std::unordered_map<std::string, std::string>(),
                                             const std::string &working_directory = "");


// Based on CPU load and number of cores, tells us if it would be prudent to spawn a new process at this time or not.
// \return false if the CPU is too buasy, o/w true.
bool ShouldScheduleNewProcess();


/** \brief Finds the list of PID's given a program's name
 *  \param programe_name  If this starts w/ a slash we look for exact matches o/w we only comparte the last path component.
 */
void FindActivePrograms(const std::string &program_name, std::unordered_set<unsigned> * const pids);


std::unordered_set<unsigned> FindActivePrograms(const std::string &program_name);


/** \brief Change the name of the current process.
 *  \param argv0             The address of the current process name, i.e. argv[0].
 *  \param new_process_name  The new process name.
 *  \note The new process name has some severe limitations: 1) It may not be longer than 15 bytes and 2) it must not exceed the
 *        length of the original process name.
 */
bool SetProcessName(char *argv0, const std::string &new_process_name);


/** \return The unmodified, i.e. orginal, command-name for the given PID or the empty string if no process with
    the provided PID was found. */
std::string GetOriginalCommandNameFromPID(const pid_t pid);


inline std::string EscapeArg(const std::string &arg, const char quote_type) {
    return StringUtil::BackslashEscape(quote_type, arg);
}


inline std::string EscapeAndQuoteArg(const std::string &arg) {
    return "\"" + EscapeArg(arg, '"') + "\"";
}


} // namespace ExecUtil
