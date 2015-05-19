#include "ExecUtil.h"
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "util.h"


int Exec(const std::string &command, const std::vector<std::string> &args) {
    if (::access(command.c_str(), X_OK) != 0)
	throw std::runtime_error("in Exec: can't execute \"" + command + "\"!");

    const int EXECVE_FAILURE(248);

    const pid_t pid = ::fork();
    if (pid == -1)
	throw std::runtime_error("in Exec: ::fork() failed: " + std::to_string(errno) + "!");

    // The child process:
    else if (pid == 0) {
	// Build the argument list for execve(2):
	char *argv[args.size() + 1];
	argv[0] = ::strdup(command.c_str());
	for (unsigned arg_no(0); arg_no < args.size(); ++arg_no)
	    argv[arg_no + 1] = ::strdup(args[arg_no].c_str());
	::execv(command.c_str(), argv);

	::_exit(EXECVE_FAILURE); // We typically never get here.
    }

    // The parent of the fork:
    else {
	int child_exit_status;
	errno = 0;
	int wait_retval = ::wait4(pid, &child_exit_status, 0, NULL);
	assert(wait_retval == pid or errno == EINTR);

	// Now process the child's various exit status values:
	if (WIFEXITED(child_exit_status)) {
	    switch (WEXITSTATUS(child_exit_status)) {
	    case EXECVE_FAILURE:
		throw std::runtime_error("in Exec: failed to execve(2) in child!");
	    default:
		return WEXITSTATUS(child_exit_status);
	    }
	}
	else if (WIFSIGNALED(child_exit_status))
	    throw std::runtime_error("in Exec: \"" + command + "\" killed by signal "
				     + std::to_string(WTERMSIG(child_exit_status)) + "!");
	else // I have no idea how we got here!
	    Error("in Exec: dazed and confused!");
    }

    return 0; // Keep the compiler happy!
}
