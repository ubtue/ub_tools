#!/bin/python2
# -*- coding: utf-8 -*-

"""
@author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)

@copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""


import errno
import os
import signal
import sys
import time


def _SigAlarmHandler(signal_no, frame):
   pass


# N.B., if provided, "args" must be a list, ditto for "env".  "timeout" is in seconds.
# @return either the exit code of the child, or if there was a timeout then -1
def Exec(cmd_path, args = None, timeout = 0, env = None, new_stdout = None, new_stderr = None,
         append_stdout = False, append_stderr = False):
    if args is None:
        args = []

    if not os.access(cmd_path, os.X_OK):
        raise Exception("in process_util.Exec: command \"" + cmd_path + "\" either does not exist or is not executable!")

    child_pid = os.fork()
    if child_pid != 0: # We're the parent.
        if timeout != 0:
            old_handler = signal.getsignal(signal.SIGALRM)
            signal.signal(signal.SIGALRM, _SigAlarmHandler)
            signal.alarm(timeout)
        
        interrupted = False
        try:
            (pid, exit_code, _) = os.wait4(child_pid, 0)
        except OSError:
            interrupted = True

        if timeout != 0:
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old_handler)
            if interrupted:
                os.kill(-child_pid, signal.SIGTERM)
                time.sleep(2) # 2 seconds
                os.kill(-child_pid, signal.SIGKILL)
                try:
                    while True:
                        (_, exit_code, _) = os.wait4(-child_pid, 0)
                except OSError as e:
                    pass
                return -1

        if os.WIFEXITED(exit_code):
            return os.WEXITSTATUS(exit_code)
        elif os.WIFSIGNALED(exit_code):
            raise Exception("in process_util.Exec: " + cmd_path + " was killed by signal \""
                            + str(os.WTERMSIG(exit_code)) + "!")
        else:
            raise Exception("in process_util.Exec: no idea why " + cmd_path + " exited!")
        return exit_code

    else: # We're the child.
        if os.setsid() == -1:
            util.Info("in process_util.Exec: os.setsid() failed!", file=sys.stderr)
            sys.exit(-1)

        if new_stdout is not None:
            sys.stdout = open(new_stdout, "ab" if append_stdout else "wb")
            os.dup2(sys.stdout.fileno(), 1)

        if new_stderr is not None:
            sys.stderr = open(new_stderr, "ab" if append_stderr else "wb")
            os.dup2(sys.stderr.fileno(), 2)

        errno.errno = 0
        args = [cmd_path] + args
        if env is None:
            os.execv(cmd_path, args)
        else: 
           os.execve(cmd_path, args, env)
        raise Exception("in process_util.Exec: we should never get here! (" + os.strerror(errno.EPERM) + ")")


if __name__ == '__main__':
    try:
        Exec("Non-existent")
    except Exception as e:
        util.Info(str(e))
    util.Info("hello.sh returned " + str(Exec("./cpp/hello.sh")))
    util.Info("fail.sh returned " + str(Exec("./fail.sh")))
    util.Info("more_than_5_seconds.sh returned " + str(Exec("./more_than_5_seconds.sh", timeout=5)))
    util.Info("\"echo_args.sh a b c\" returned " + str(Exec("./echo_args.sh", ['a', 'b', 'c'])))
