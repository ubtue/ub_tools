/** \file    ExecUtil.h
 *  \brief   Declarations of MIME/media type utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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

#ifndef EXEC_UTIL_H
#define EXEC_UTIL_H


#include <string>
#include <vector>
#include <signal.h>


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
 *  \return The exit code of the subcommand or an error code if there was a failure along the way.
 */
int Exec(const std::string &command, const std::vector<std::string> &args = {}, const std::string &new_stdin = "",
         const std::string &new_stdout = "", const std::string &new_stderr = "",
         const unsigned timeout_in_seconds = 0, const int tardy_child_signal = SIGKILL);


/** \brief  Kicks off a subcommand and returns.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  new_stdin           An optional replacement file path for stdin.
 *  \param  new_stdout          An optional replacement file path for stdout.
 *  \param  new_stderr          An optional replacement file path for stderr.
 *  \return The PID of the child.
 */
int Spawn(const std::string &command, const std::vector<std::string> &args = {}, const std::string &new_stdin = "",
          const std::string &new_stdout = "", const std::string &new_stderr = "");


/** \brief Tries to find a path, with the help of the environment variable PATH, to "executable_candidate".
 *  \return The path where the executable can be found or the empty string if no such path was found or if
 *          "executable_candidate" is not executable.
 */
std::string Which(const std::string &executable_candidate);


/**  \brief Retrieve the stdout of a subcommand.
 *   \param command        A shell command.  Can include arguments. E.g. "ls -l".
 *   \param stdout_output  Where to store the output of the command.
 *   \note  The command will be executed by passing it to the standard shell interpreter: "/bin/sh -c command".
 */
bool ExecSubcommandAndCaptureStdout(const std::string &command, std::string * const stdout_output);


} // namespace ExecUtil


#endif // ifndef EXEC_UTIL_H
